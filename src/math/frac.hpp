#pragma once

#include <cstddef>

// Rational display helpers (4D.2/4D.3): the home screen's `>frac`
// postfix and the graph's pi-multiple tick labels both need "is this
// double (a multiple of) a small fraction?" answered by a bounded
// continued-fraction expansion.
namespace math::frac {

// Best rational p/q ~= x with 1 <= q <= max_den. True when the match
// is tight (|x - p/q| within ~1e-9 relative); p carries the sign.
bool decimal_to_fraction(double x, long max_den, long* p, long* q);

// "3/4", "-5/3", or a plain integer when q == 1. False when x has no
// tight fraction with denominator <= max_den (caller falls back to
// format_number).
bool format_fraction(double x, long max_den, char* buf, size_t buf_len);

// x ~= (p/q) * pi with q <= max_den and |p| <= max_num: the pi-tick
// detector (4D.3). Excludes 0.
bool pi_multiple(double x, long max_num, long max_den, long* p, long* q);

}  // namespace math::frac
