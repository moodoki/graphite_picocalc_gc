#include "gfx/font.hpp"

#include <cstring>

// Font data (const arrays => internal linkage; include in this TU only).
#include "lcdspi/fonts/font1.h"

namespace gfx {

void Font::draw_char_impl(Framebuffer& fb, int x, int y, char c, Color fg, const Color* bg) const {
    const int w = width();
    const int h = height();
    const uint8_t first = data_[2];
    const uint8_t count = data_[3];
    const auto uc = static_cast<uint8_t>(c);

    if (uc < first || uc >= first + count) {
        if (bg != nullptr) {
            fb.fill_rect(x, y, w, h, *bg);
        }
        return;
    }

    // Skip glyphs fully outside the active strip.
    if (y + h <= fb.clip_y0() || y >= fb.clip_y1()) {
        return;
    }

    const uint8_t* glyph = data_ + 4 + (uc - first) * ((w * h) / 8);
    int bit = 0;
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            const bool on = (glyph[bit / 8] >> (7 - (bit % 8))) & 1;
            ++bit;
            if (on) {
                fb.set_pixel(x + xx, y + yy, fg);
            } else if (bg != nullptr) {
                fb.set_pixel(x + xx, y + yy, *bg);
            }
        }
    }
}

void Font::draw_char(Framebuffer& fb, int x, int y, char c, Color fg, Color bg) const {
    draw_char_impl(fb, x, y, c, fg, &bg);
}

void Font::draw_char(Framebuffer& fb, int x, int y, char c, Color fg) const {
    draw_char_impl(fb, x, y, c, fg, nullptr);
}

void Font::draw_string(Framebuffer& fb, int x, int y, const char* s, Color fg, Color bg) const {
    while (*s != 0) {
        draw_char(fb, x, y, *s++, fg, bg);
        x += width();
    }
}

void Font::draw_string(Framebuffer& fb, int x, int y, const char* s, Color fg) const {
    while (*s != 0) {
        draw_char(fb, x, y, *s++, fg);
        x += width();
    }
}

int Font::text_width(const char* s) const {
    return static_cast<int>(std::strlen(s)) * width();
}

const Font& main_font() {
    static const Font font(font1);
    return font;
}

}  // namespace gfx
