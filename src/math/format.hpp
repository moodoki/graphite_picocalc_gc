#pragma once

#include <cstddef>

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

}  // namespace math
