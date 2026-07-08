#pragma once

#include <cstddef>

#include "math/types.hpp"

namespace math {

// Format a number for display (task 2.4). Rules:
//  - Integer-valued and |x| < 1e10: no decimal point
//  - Otherwise up to 10 significant figures, trailing zeros stripped
//  - Scientific notation for |x| >= 1e10 or 0 < |x| < 1e-4
//  - Specials: "Inf", "-Inf", "NaN"
// Returns the number of characters written (excluding NUL).
int format_number(calc_t x, char* buf, size_t buf_len);

}  // namespace math
