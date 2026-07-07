// Phase 0 stub. Blinks the on-board LED on both Pico 1 and Pico 2.
// Replaced in Phase 1, task 1.7 (line-buffer renderer + dual-core dispatch).

#include "pico/stdlib.h"

#include "config.hpp"

int main() {
    stdio_init_all();

    const uint led_pin = PICO_DEFAULT_LED_PIN;
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    while (true) {
        gpio_put(led_pin, 1);
        sleep_ms(500);
        gpio_put(led_pin, 0);
        sleep_ms(500);
    }
}
