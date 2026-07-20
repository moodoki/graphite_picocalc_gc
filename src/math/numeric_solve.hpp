#pragma once

#include "math/types.hpp"

// Numeric root-finder (task 4A.9, phase4-spec §3.4) plus the Phase 4B
// calculus primitives (extremum, derivative, integral — spec §4.2).
// Host-testable (no platform dependencies).
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

// Generic scalar function for the callback-based primitives below.
// Graph analysis (4B) uses these with compiled-expression contexts —
// parametric/polar integrands aren't expressible as a single string.
using EvalFn = calc_t (*)(void* ctx, calc_t x);

struct ExtremumResult {
    bool converged = false;
    calc_t x = 0;      // Location of the extremum
    calc_t value = 0;  // f(x) there
    int iterations = 0;
    const char* error = nullptr;  // Static string when !converged
};

// Local minimum (or maximum) of f in [lo, hi] via Brent's method
// (golden section + parabolic interpolation — derivative-free, spec
// §4.2). tolerance is relative in x.
ExtremumResult numeric_extremum_fn(EvalFn f, void* ctx, calc_t lo, calc_t hi, bool find_max,
                                   calc_t tolerance = 1e-9, int max_iter = 100);
ExtremumResult numeric_extremum(const char* expr, int var_slot, calc_t lo, calc_t hi, bool find_max,
                                calc_t tolerance = 1e-9, int max_iter = 100);

struct DerivResult {
    bool ok = false;
    calc_t value = 0;
    const char* error = nullptr;  // Static string when !ok
};

// Numeric derivative at `at`: central difference with one Richardson
// extrapolation step, (4*D(h/2) - D(h)) / 3. h = 0 picks
// 1e-4 * max(|at|, 1); graph analysis passes h scaled to the window.
DerivResult numeric_derivative_fn(EvalFn f, void* ctx, calc_t at, calc_t h = 0);
DerivResult numeric_derivative(const char* expr, int var_slot, calc_t at, calc_t h = 0);

struct IntegralResult {
    bool converged = false;
    calc_t value = 0;
    calc_t error_est = 0;         // Accumulated |K15 - G7| over accepted panels
    int evals = 0;                // Integrand evaluations
    const char* error = nullptr;  // Static string when !converged
};

// Definite integral over [a, b]: adaptive Gauss-Kronrod (G7-K15),
// bisecting panels whose error estimate exceeds the tolerance share.
// At the depth cap the panel is accepted as-is (its error still lands
// in error_est) rather than hanging — spec §4.2 / Risk 4. Kronrod
// nodes never touch panel endpoints, so endpoint singularities
// (1/sqrt(x) on [0,1]) integrate cleanly. b < a negates.
IntegralResult numeric_integral_fn(EvalFn f, void* ctx, calc_t a, calc_t b,
                                   calc_t tolerance = 1e-9);
IntegralResult numeric_integral(const char* expr, int var_slot, calc_t a, calc_t b,
                                calc_t tolerance = 1e-9);

}  // namespace math
