#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "bt_spp.h"

static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
}

void core1_main() {
    printf("Entered core1 (core=%d)\n", get_core_num());

    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return;
    }

    btstack_main(0, NULL);

    while (1) {
        // printf("Hello, core2!\n");
        hal_led_toggle();
        sleep_ms(1000);
    }
}

int main() {
    sleep_ms(1000);
    stdio_init_all();
    sleep_ms(1000);
    printf("Entered core0 (core=%d)\n", get_core_num());
    multicore_launch_core1(core1_main);

    while (true) {
        // printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
