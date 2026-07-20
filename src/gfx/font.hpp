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

// Non-ASCII glyphs baked into unused high slots by scripts/gen-fonts.sh
// (bdf_to_utft.py --map / --extra). Greek/math symbols for the complex
// and polar display paths (testdrive 2026-07-20). Present in the 8x16
// main font only — the 5x8 small font stays plain ASCII and draws these
// blank, same as pi has always been. Regenerate via scripts/gen-fonts.sh
// if the slot map changes.
constexpr char kGlyphPi = '\x7f';          // 127 pi (D24)
constexpr char kGlyphAngle = '\x80';       // 128 angle sign (polar phasor)
constexpr char kGlyphTheta = '\x81';       // 129 theta
constexpr char kGlyphSigmaLower = '\x82';  // 130 sigma
constexpr char kGlyphSigmaUpper = '\x83';  // 131 Sigma
constexpr char kGlyphChi = '\x84';         // 132 chi
constexpr char kGlyphMu = '\x85';          // 133 mu
constexpr char kGlyphImagI = '\x86';       // 134 slanted imaginary-unit i
constexpr char kGlyphStore = '\x87';       // 135 store arrow (U+21D2 "=>")
constexpr char kGlyphLambda = '\x88';      // 136 lambda
constexpr char kGlyphNotEqual = '\x89';    // 137 not-equal (U+2260)
constexpr char kGlyphEllipsis = '\x8a';    // 138 horizontal ellipsis (U+2026)
constexpr char kGlyphSuperTwo = '\x8b';    // 139 superscript two (U+00B2)
constexpr char kGlyphSqrt = '\x8c';        // 140 square-root radical (U+221A)

// Main text font: Spleen 8x16 (D9 — replaced the interim Coyote 8x12).
const Font& main_font();

// Small font: Spleen 5x8 — axis labels, dense annotations.
const Font& small_font();

}  // namespace gfx
