#pragma once

#include "math/cas/expr.hpp"

// Shared polynomial helpers for solve/factor (Phase 5). Operate on numeric
// (constant-coefficient) univariate polynomials in a chosen variable.
namespace math::cas {

// Extract numeric coefficients of `f` in `var` into coeffs[0..*degree]
// (coeffs[k] is the coefficient of var^k). `f` should be expand()ed first.
// Returns false if `f` is not a polynomial in `var` with purely numeric
// coefficients, or its degree exceeds maxdeg. coeffs must hold maxdeg+1
// entries.
bool poly_coeffs(const Expr* f, char var, double* coeffs, int maxdeg, int* degree);

}  // namespace math::cas
