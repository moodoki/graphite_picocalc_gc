#pragma once

#include <cstdint>

#include "gfx/framebuffer.hpp"

namespace gfx {

// Bitmap font in UTFT header layout: data[0]=width, data[1]=height,
// data[2]=first char, data[3]=char count, then (w*h/8) bytes per glyph,
// row-major, MSB-left.
class Font {
public:
    explicit constexpr Font(const uint8_t* data) : data_(data) {}

    int width() const { return data_[0]; }
    int height() const { return data_[1]; }

    void draw_char(Framebuffer& fb, int x, int y, char c, Color fg, Color bg) const;
    // Transparent background variant.
    void draw_char(Framebuffer& fb, int x, int y, char c, Color fg) const;

    void draw_string(Framebuffer& fb, int x, int y, const char* s, Color fg, Color bg) const;
    void draw_string(Framebuffer& fb, int x, int y, const char* s, Color fg) const;

    int text_width(const char* s) const;

private:
    const uint8_t* data_;

    void draw_char_impl(Framebuffer& fb, int x, int y, char c, Color fg, const Color* bg) const;
};

// Main text font (8x12, vendored Coyote OS font1 — interim until a
// proper 8x16 is generated; see worklog).
const Font& main_font();

}  // namespace gfx
