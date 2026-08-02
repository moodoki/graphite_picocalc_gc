#pragma once

#include "math/cas/expr.hpp"
#include "math/types.hpp"

// Symbolic integration (Phase 5, phase5-spec.md §9, tasks 4D.16-4D.19).
// A table-based integrator with linearity, linear substitution (f(ax+b)),
// and one-level integration by parts (LIATE). Not a general algorithm — it
// returns nullptr when it cannot find an antiderivative.
namespace math::cas {

// Indefinite integral of expr w.r.t. var (without +C). nullptr if unable.
Expr* integrate(const Expr* expr, char var);

struct DefIntResult {
    bool has_symbolic = false;  // a symbolic antiderivative was found
    Expr* antideriv = nullptr;  // the antiderivative (nullable)
    bool has_numeric = false;   // a numeric value was computed
    calc_t numeric_val = 0.0;   // value of the definite integral
};

// Definite integral over [lower, upper] (both expressions; numeric constants
// or pi). Uses the symbolic antiderivative when available, else falls back to
// numeric quadrature.
DefIntResult definite_integrate(const Expr* expr, char var, const Expr* lower, const Expr* upper);

}  // namespace math::cas
