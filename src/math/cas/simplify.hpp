#pragma once

#include "math/cas/expr.hpp"

// CAS simplifier (Phase 5, phase5-spec.md §5, tasks 4D.5-4D.7). Rewrites an
// Expr tree to a canonical, reduced form via bottom-up normalization inside a
// fixed-point loop (hard pass cap — spec §13 Risk 1). Returns a fresh tree in
// the current pool; the input is not modified. Returns nullptr on pool
// exhaustion.
//
// Canonical form produced:
//   * Sums (ADD): like terms combined (a*x + b*x -> (a+b)*x), numeric
//     constants folded into one trailing term, operands in canonical order.
//   * Products (MUL): numeric coefficient folded to one leading factor,
//     like bases combined via exponents (x^a * x^b -> x^(a+b)), operands in
//     canonical order.
//   * Negation/subtraction fold through a -1 coefficient; division folds
//     through negative exponents (a/b == a*b^-1); powers distribute over
//     product bases for integer exponents ((a*b)^n -> a^n*b^n).
//   * The reserved imaginary unit obeys i^2 = -1 (a single rule).
//   * Identity/annihilation (x+0, x*1, x*0, x^0, x^1) and constant folding
//     (2+3 -> 5, 2^10 -> 1024) applied throughout; select exact function
//     values folded (sin(0), cos(0), ln(1), ...).
namespace math::cas {

Expr* simplify(const Expr* expr);

}  // namespace math::cas
