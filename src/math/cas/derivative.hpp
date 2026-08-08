#pragma once

#include "math/cas/expr.hpp"

// Symbolic differentiation (Phase 5, phase5-spec.md §6, tasks 4D.8-4D.9).
// Structural recursion over the Expr tree with the standard rules (sum,
// product, power, chain), then simplify() on the result. Single-variable
// only — no partials, no implicit differentiation (spec scope note).
namespace math::cas {

// d/d(var) of expr, simplified. Returns nullptr if the expression contains a
// function with no known derivative rule, or on pool exhaustion.
Expr* differentiate(const Expr* expr, char var);

// Apply differentiate n times (n >= 0). n == 0 returns simplify(expr).
Expr* differentiate_n(const Expr* expr, char var, int n);

}  // namespace math::cas
