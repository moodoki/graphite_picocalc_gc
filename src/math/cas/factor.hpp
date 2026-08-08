#pragma once

#include "math/cas/expr.hpp"

// Polynomial factoring (Phase 5, phase5-spec.md §7, tasks 4D.11-4D.12).
// Handles common-factor (content + lowest power of var) extraction, and
// integer-root factoring via the rational-root theorem + synthetic division
// (covers difference-of-squares and quadratics whose roots are rational, and
// degree 3-4 with rational roots). Returns the factored form, or the
// (expanded) original if no factoring was found. Numeric-coefficient
// univariate polynomials only.
namespace math::cas {

Expr* factor(const Expr* expr, char var);

}  // namespace math::cas
