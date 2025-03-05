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

ST7565 disp(spi0, 3, 2, 5, 4, 6);
LCDdisplay myLCD(8, 9, 10, 11, 12, 13, 16, 2); // DB4, DB5, DB6, DB7, RS, E, character_width, no_of_lines

mutex_t line_mutex;
bool line_ready = false;
char line[64];

static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
}

void bt_recv(char *recv_line, size_t size) {
    mutex_enter_blocking(&line_mutex);
    strncpy(line, recv_line, sizeof(line));
    line_ready = true;
    mutex_exit(&line_mutex);
    printf("recv data size %d: '%s'\n", size, line);
}

void core1_main() {
    printf("Entered core1 (core=%d)\n", get_core_num());

    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return;
    }

    set_recv_callback(bt_recv);
    btstack_main();
    printf("btstack init success\n");

    while (1) {
        // printf("Hello, core2!\n");
        hal_led_toggle();
        sleep_ms(1000);
    }
}

const int NUM_MOODS = 4;
const char *MOOD_STRINGS[] = {
    "Peace ",
    "Money ",
    "Health",
    "Sleep ",
};
int selection = 0;

void update_lcd() {
    myLCD.clear() ;
    myLCD.print_wrapped("Select mood:    ");

    char buf[17];
    sprintf(buf, "<    %s    >", MOOD_STRINGS[selection]);
    myLCD.print(buf);
}

int main() {
    sleep_ms(1000);
    stdio_init_all();
    sleep_ms(1000);
    printf("Entered core0 (core=%d)\n", get_core_num());
    multicore_launch_core1(core1_main);

    mutex_init(&line_mutex);

    disp.display_init();
    printf("display init success\n");

    // disp.fill(1);
    disp.print(12, 20, "Hello world!");
    // disp.pixel(8, 10, 1);
    // disp.buffer[1] = 0xff;
    // for (int i = 0; i < 8; i++) {
    //     disp.pixel(i, 0, 1);
    //     disp.pixel(0, i, 1);
    //     disp.pixel(8, i, 1);
    //     disp.pixel(i, 8, 1);
    // }
    disp.show();

    myLCD.init();
    myLCD.cursor_off();
    myLCD.clear() ;
    myLCD.print("Select mood:    ");
    myLCD.print("<    Peace     >");
    // sleep_ms(2500);

    gpio_init(14);
    gpio_set_input_enabled(14, true);
    gpio_set_pulls(14, true, false);
    gpio_init(15);
    gpio_set_input_enabled(15, true);
    gpio_set_pulls(15, true, false);
    gpio_init(16);
    gpio_set_input_enabled(16, true);
    gpio_set_pulls(16, true, false);

    while (true) {
        // printf("Hello, world!\n");
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
