#include <algorithm>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/mutex.h"
#include "hardware/gpio.h"

#include "lcd_display.hpp"

#include "bt_spp.h"
#include "st7565.h"

#include "filesystem/vfs.h"

// Graphical LCD (Using SPI0, MOSI=3, SCK=2, CS=5, DC=4, RST=6)
ST7565 disp(spi0, 3, 2, 5, 4, 6);

// 16x2 LCD (DB4=8, DB5=9, DB6=10, DB7=11, RS=12, E=13, character_width=16, no_of_lines=2)
LCDdisplay myLCD(8, 9, 10, 11, 12, 13, 16, 2);

// Mutex and data for bluetooth receive buffer
mutex_t line_mutex;
bool line_ready = false;
char line[64];

// Example code for blinking the builtin led
static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
}

// Bluetooth receive callback
//  (called to transfer data from bluetooth context on core1 to our application on core0)
void bt_recv(char *recv_line, size_t size) {
    mutex_enter_blocking(&line_mutex);
    strncpy(line, recv_line, 63);
    line_ready = true;
    mutex_exit(&line_mutex);
    printf("recv data size %d: '%s'\n", size, line);
}

void core1_main() {
    printf("Entered core1 (core=%d)\n", get_core_num());

    // init wifi hardware
    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return;
    }

    // set callback for receiving a line of data
    set_recv_callback(bt_recv);
    // startup btstack
    btstack_main();
    printf("btstack init success\n");

    // loop doing nothing (blinking the led)
    while (1) {
        // printf("Hello, core2!\n");
        hal_led_toggle();
        sleep_ms(1000);
    }
}

// TEMP: lcd demo
const int NUM_MOODS = 4;
const char *MOOD_STRINGS[] = {
    "Peace ",
    "Money ",
    "Health",
    "Sleep ",
};
int selection = 0;

// TEMP: lcd demo
void update_lcd() {
    myLCD.clear() ;
    myLCD.print_wrapped("Select mood:    ");

    char buf[17];
    sprintf(buf, "<    %s    >", MOOD_STRINGS[selection]);
    myLCD.print(buf);
}

int main() {
    // Init Pico SDK STDIO (e.g. printf)
    stdio_init_all();
    // Init filesystem
    fs_init();

    // wait a little
    sleep_ms(1000);
    printf("Entered core0 (core=%d)\n", get_core_num());

    // create mutex for bluetooth data
    mutex_init(&line_mutex);

    // init graphical lcd
    disp.display_init();
    printf("display init success\n");

    // display test
    // disp.fill(1);
    disp.print(12, 20, "Hello world!");
    disp.show();

    // init 16x2 lcd
    myLCD.init();
    myLCD.cursor_off();
    myLCD.clear() ;
    myLCD.print("Select mood:    ");
    myLCD.print("<    Peace     >");

    // setup buttons as input w/ pullup
    // pin 14 = blue button
    gpio_init(14);
    gpio_set_input_enabled(14, true);
    gpio_set_pulls(14, true, false);
    // GPIO 15 = red button
    gpio_init(15);
    gpio_set_input_enabled(15, true);
    gpio_set_pulls(15, true, false);
    // GPIO 16 = yellow button
    gpio_init(16);
    gpio_set_input_enabled(16, true);
    gpio_set_pulls(16, true, false);

    // setup finished, start core1
    multicore_launch_core1(core1_main);

    while (true) {
        // check if data from bluetooth is ready and send it to the projector
        if (mutex_try_enter(&line_mutex, NULL)) {
            if (line_ready) {
                printf("line_ready\n");
                line_ready = false;
                disp.fill(0);
                disp.print(0, 0, line);
                disp.show();
            }
            mutex_exit(&line_mutex);
        }

        // FIXME: all of this is bad and we need to do actual debouncing,
        //        probably find a library that handles buttons
        if (!gpio_get(14)) {
            // printf("blue button\n");
            selection = (selection + 1) % NUM_MOODS;
            update_lcd();
            sleep_ms(10);
        }
        if (!gpio_get(15)) {
            // printf("red button\n");
            selection = (selection + NUM_MOODS - 1) % NUM_MOODS;
            update_lcd();
            sleep_ms(10);
        }
        if (!gpio_get(16)) {
            printf("yellow button\n");
        }

        sleep_ms(10);
    }
}
