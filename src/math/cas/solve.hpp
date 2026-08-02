#pragma once

#include "math/cas/expr.hpp"

// Symbolic equation solving (Phase 5, phase5-spec.md §8, tasks 4D.13-4D.15).
// Single-equation, single-variable. Handles linear and quadratic polynomials
// (real and, when allowed, complex roots via the symbolic imaginary unit i),
// plus inverse-function isolation for f(x)=c. Cubic/quartic rational-root
// solving is added alongside factoring (Stage 2d).
namespace math::cas {

struct SolveResult {
    Expr* solutions[8] = {};
    int count = 0;
    bool exact = false;    // all solutions symbolic/exact (vs numeric)
    bool complex = false;  // any solution has a nonzero imaginary part
};

// Solve `equation` (an EQ node, or a bare expression treated as = 0) for var.
// allow_complex: when false, complex roots are suppressed (REAL number mode)
// and such an equation returns count == 0 with complex == true.
SolveResult solve(const Expr* equation, char var, bool allow_complex);

}  // namespace math::cas
