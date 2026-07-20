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

// Format a complex value per number mode (Phase 4C, phase4-spec.md
// §5.3). RECTANGULAR: "3 + 2i", "-1.5 - 0.5i", "4" (pure real), "2i"
// (pure imag, unit coefficients elide the "1"); the kReal fallback
// degrades to the same rectangular form (callers keep real results on
// format_number directly, so this only fires if one calls it anyway).
// POLAR: "r<theta" (no baked angle glyph yet — "<" mirrors the common
// ASCII/EE phasor convention, e.g. "2<60"; theta respects angle_mode()
// the same way real trig does). Each component reuses format_number,
// so it honors the active FIX/SCI/FLOAT display mode.
int format_complex(const Complex& z, NumberMode mode, char* buf, size_t buf_len);

}  // namespace math
