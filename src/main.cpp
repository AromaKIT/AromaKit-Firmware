#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/mutex.h"
#include "pico/flash.h"
#include "hardware/gpio.h"
#include "filesystem/vfs.h"

#include "lcd_display.hpp"
#include "bt_spp.h"
#include "st7565.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

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
    // printf("recv data size %d: '%s'\n", size, line);
}

void bt_printf(const char *format...) {
    va_list args;
    va_start(args, format);

    char buf[128];
    int len = vsnprintf(buf, 128, format, args);
    spp_write(buf, 1, len);

    va_end(args);
}

void core1_main() {
    flash_safe_execute_core_init();
    sleep_ms(5000);
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

bool str_to_int(const std::string &s, int &out) {
    errno = 0;
    char *endptr;
    const char *nptr = s.c_str();
    int n = strtol(nptr, &endptr, 10);

    if (nptr == endptr || errno != 0) {
        return false;
    } else {
        out = n;
        return true;
    }
}

std::vector<std::vector<std::string>> messages;

void handleMessage(char *data) {
    std::string msg(data);

    size_t pos;
    if ((pos = msg.find("\n")) != std::string::npos) {
        auto line = msg.substr(0, pos);

        std::string tmp;
        std::stringstream ss(line);
        std::vector<std::string> parts;

        while(getline(ss, tmp, ' ')){
            parts.push_back(tmp);
        }

        auto cmd = parts[0];
        auto rest = std::string();
        if (parts.size() > 1) {
            rest = line.substr(cmd.length()+1);
        }
        printf("cmd: '%s', rest: '%s'\n", cmd.c_str(), rest.c_str());

        if (cmd == "HELLO") {
            bt_printf("OK Hello!\n");
        } else if (cmd == "VER") {
            bt_printf("OK Version %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
        } else if (cmd == "NEW") {
            messages.push_back(std::vector<std::string>());
            bt_printf("OK %d\n", messages.size());
        } else if (cmd == "ADD") {
            if ((pos = rest.find(" ")) != std::string::npos) {
                // try {
                //     auto i = std::stoi(rest.substr(0, pos));
                //     auto m = rest.substr(pos);
                //     bt_printf("OK\n");
                // } catch (std::exception) {
                //     bt_printf("ERR Invalid Argument\n");
                // }
            } else {
                bt_printf("ERR Invalid Argument\n");
            }
        } else if (cmd == "DEL") {

        } else if (cmd == "LIST") {
            if (parts.size() < 2) {
                bt_printf("ERR Invalid Argument\n");
                return;
            }

            int id;
            if (!str_to_int(parts[1], id)) {
                bt_printf("ERR Invalid Argument\n");
                return;
            }

            if (messages.size() > 0 && id > messages.size() - 1) {
                bt_printf("ERR Invalid Argument\n");
                return;
            }

            bt_printf("OK [");
            for (int i = 0; i < messages[id].size(); i++) {
                bt_printf("\"%s\"", messages[id][i].c_str());
                if (i != messages[id].size() - 1) {
                    bt_printf(", ");
                }
            }
            bt_printf("]\n");
        } else if (cmd == "TEST") {
            if (parts.size() < 4) {
                bt_printf("ERR Invalid Argument\n");
                return;
            }

            int x, y;
            if (!str_to_int(parts[1], x) || !str_to_int(parts[2], y)) {
                bt_printf("ERR Invalid Argument\n");
                return;
            }

            if (x < 0) x = 0;
            if (x > 128) x = 128;
            if (y < 0) y = 0;
            if (y > 64) y = 64;

            auto m = line.substr(cmd.length() + parts[1].length() + parts[2].length() + 3);

            disp.fill(0);
            disp.print(x, y, m.c_str());
            disp.show();

            myLCD.clear();
            myLCD.print_wrapped(m.c_str());
        } else {
            disp.fill(0);
            disp.print(0, 0, line.c_str());
            disp.show();

            myLCD.clear();
            myLCD.print_wrapped(line.c_str());

            bt_printf("ERR Unknown Command\n");
        }
    }
}

int main() {
    // Init Pico SDK STDIO (e.g. printf)
    stdio_init_all();
    sleep_ms(2000);
    // Launch core1
    multicore_launch_core1(core1_main);
    // Init filesystem
    if (!fs_init()) {
        printf("Error during filesystem init!!\n");
    }

    // wait a little
    sleep_ms(1000);
    printf("Entered core0 (core=%d)\n", get_core_num());

    // create mutex for bluetooth data
    mutex_init(&line_mutex);

    // init graphical lcd
    disp.display_init();

    // init 16x2 lcd
    myLCD.init();
    myLCD.cursor_off();
    myLCD.clear() ;
    // myLCD.print("Select mood:    ");
    // myLCD.print("<    Peace     >");

    // FILE *file;
    // // char line[17] = {0};
    // if ((file = fopen("/line", "r"))) {
    //     printf("File opened successfully\n");
    //     int size = fread(line, 1, 64, file);
    //     line[size] = 0;
    //     printf("Read %d bytes: %s\n", size, line);
    //     fclose(file);
    // }

    // if (strlen(line)) {
    //     myLCD.print(line);
    // }

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

    printf("setup complete\n");

    // projected display demo
    disp.print(0, 0, "   Welcome to  ");
    disp.print(0, 8, "    AromaKIT!  ");
    disp.print(0, 24," For more info,");
    disp.print(0, 32,"  example.com  ");
    disp.show();

    while (true) {
        // check if data from bluetooth is ready and send it to the projector
        if (mutex_try_enter(&line_mutex, NULL)) {
            if (line_ready) {
                line_ready = false;
                handleMessage(line);
                // bt_print("Hello\n");

                // disp.fill(0);
                // disp.print(0, 0, line);
                // disp.show();

                // myLCD.goto_pos(0,1);
                // myLCD.clear();
                // myLCD.print_wrapped(line);
                // if ((file = fopen("/line", "w"))) {
                //     fwrite(line, 1, strlen(line), file);
                //     fclose(file);
                // }
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
