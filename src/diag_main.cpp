// Hardware bring-up diagnostic — bisects the display problem.
//
// Uses ONLY the vendored Coyote OS blocking driver (known-good on
// PicoCalc): no DMA, no core 1, no strip framebuffer, no RGB565->666
// path of our own. If this shows clean solid colors + text, the panel
// and vendored path are fine and the bug is in our gfx pipeline. If it
// still shows garbage, the panel init or SPI wiring differs from what we
// assumed.
//
// Build target: picocalc_diag (see CMakeLists.txt).

#include <cstdio>

#include "pico/stdlib.h"

extern "C" {
#include "i2ckbd/i2ckbd.h"
#include "lcdspi/lcdspi.h"
}

int main() {
    stdio_init_all();

    // Keyboard/I2C bus up (harmless; also lets us read keys later).
    init_i2c_kbd();

    // Bring up the LCD purely through the vendored driver.
    lcd_init();

    int frame = 0;
    while (true) {
        // Cycle solid fills so we can see the panel is driven cleanly.
        const int colors[] = {RED, GREEN, BLUE, WHITE, BLACK};
        const int c = colors[frame % 5];
        draw_rect_spi(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, c);

        // Overlay text so we know rendering + font work.
        lcd_set_text_color(WHITE, c == WHITE ? BLACK : c);
        set_current_x(8);
        set_current_y(8);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) vendored API takes char*
        lcd_print_string(const_cast<char*>("PicoCalc vendored-path diag"));
        set_current_x(8);
        set_current_y(28);
        char line[32];
        std::snprintf(line, sizeof(line), "frame %d", frame);
        lcd_print_string(line);

        printf("diag alive: frame %d, color idx %d\n", frame, frame % 5);
        ++frame;
        sleep_ms(1000);
    }
}
