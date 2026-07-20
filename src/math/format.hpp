#pragma once

#include <cstddef>

#include "math/complex.hpp"
#include "math/types.hpp"

namespace math {

// Display format (task 5.3). kFloat is the default auto formatter;
// kFix forces a fixed number of decimals; kSci forces scientific.
enum class DisplayMode { kFloat, kFix, kSci };

DisplayMode display_mode();
void set_display_mode(DisplayMode m);
int fix_digits();  // Decimals used in kFix (default 2)
void set_fix_digits(int n);

// Format a number for display (task 2.4). In kFloat mode:
//  - Integer-valued and |x| < 1e10: no decimal point
//  - Otherwise up to 10 significant figures, trailing zeros stripped
//  - Scientific notation for |x| >= 1e10 or 0 < |x| < 1e-4
//  - Specials: "Inf", "-Inf", "NaN"
// kFix/kSci override the mantissa rules (specials still apply).
// Returns the number of characters written (excluding NUL).
int format_number(calc_t x, char* buf, size_t buf_len);

// Compact variant of format_number for dense contexts (list summaries):
// ordinary fractional values get 4 significant figures (~5 chars)
// instead of 10, so more elements fit before truncation (testdrive
// 2026-07-20). Integers, the scientific-notation range, non-finite
// values, and FIX/SCI display modes are identical to format_number —
// only the default-float fractional case shortens. Returns chars
// written (excluding NUL).
int format_number_compact(calc_t x, char* buf, size_t buf_len);

// Display-glyph bytes emitted by format_complex and rendered by the
// 8x16 main font's high slots (testdrive 2026-07-20 — real symbols
// instead of ASCII stand-ins). These byte values MUST match the slot
// map in gfx/font.hpp (kGlyphAngle / kGlyphImagI). The small 5x8 font
// draws them blank, so keep them off axis labels and other small-font
// contexts.
constexpr char kAngleGlyph = '\x80';     // polar phasor angle sign
constexpr char kImagUnitGlyph = '\x86';  // slanted imaginary-unit i
constexpr char kEllipsisGlyph = '\x8a';  // horizontal ellipsis (U+2026), for truncation

// Format a complex value per number mode (Phase 4C, phase4-spec.md
// §5.3). RECTANGULAR: "3 + 2i", "-1.5 - 0.5i", "4" (pure real), "2i"
// (pure imag, unit coefficients elide the "1") — the "i" is the slanted
// kImagUnitGlyph. The kReal fallback degrades to the same rectangular
// form (callers keep real results on format_number directly, so this
// only fires if one calls it anyway). POLAR: "r<theta" using the real
// angle glyph kAngleGlyph for "<" (e.g. "2<60"; theta respects
// angle_mode() the same way real trig does). Each component reuses
// format_number, so it honors the active FIX/SCI/FLOAT display mode.
int format_complex(const Complex& z, NumberMode mode, char* buf, size_t buf_len);

}  // namespace math
