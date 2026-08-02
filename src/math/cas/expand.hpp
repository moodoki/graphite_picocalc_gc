#pragma once

#include "math/cas/expr.hpp"

// Expansion (Phase 5, phase5-spec.md §7, task 4D.10). Distributes products
// over sums and expands non-negative integer powers of sums, then simplifies.
// E.g. (x+1)*(x-1) -> x^2 - 1; (x+1)^3 -> x^3 + 3x^2 + 3x + 1.
//
// The exponent for power expansion is capped (spec: n <= 20) to bound
// combinatorial growth; larger powers are left unexpanded. The result is
// always value-equivalent to the input even when not fully expanded.
namespace math::cas {

Expr* expand(const Expr* expr);

}  // namespace math::cas
