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

// Non-ASCII glyph baked into an unused slot by bdf_to_utft.py --map
// (D24): Greek pi at DEL. Present in the 8x16 main font only — the
// 5x8 BDF has no pi glyph, so small_font() draws it blank.
constexpr char kGlyphPi = '\x7f';

// Main text font: Spleen 8x16 (D9 — replaced the interim Coyote 8x12).
const Font& main_font();

// Small font: Spleen 5x8 — axis labels, dense annotations.
const Font& small_font();

}  // namespace gfx
