#pragma once

#include "math/types.hpp"

// Numeric root-finder (task 4A.9, phase4-spec §3.4). Also the engine
// behind Phase 4B's zero/intersect graph analysis. Host-testable (no
// platform dependencies).
namespace math {

struct SolveResult {
    bool converged = false;
    calc_t root = 0;
    int iterations = 0;
    calc_t residual = 0;          // |f(root)|
    const char* error = nullptr;  // Static string when !converged
};

// Solve f(x) = 0 for the variable in slot var_slot ('x'-'a' for x,
// Variables::kTheta for theta, ...). When f(lo) and f(hi) have
// opposite signs: bisection with Newton (secant-step) acceleration.
// Otherwise: Newton's method from the midpoint guess, constrained to
// [lo, hi]. The variable slot's value is saved and restored.
SolveResult numeric_solve(const char* expr, int var_slot, calc_t lo, calc_t hi,
                          calc_t tolerance = 1e-10, int max_iter = 100);

// Solve lhs = rhs by composing "(lhs)-(rhs)" and solving for zero.
SolveResult numeric_solve_equation(const char* lhs, const char* rhs, int var_slot, calc_t lo,
                                   calc_t hi, calc_t tolerance = 1e-10, int max_iter = 100);

}  // namespace math
