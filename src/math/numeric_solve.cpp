#include "math/numeric_solve.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/engine.hpp"

namespace math {

namespace {

// Central-difference derivative for the Newton steps.
calc_t deriv(void* h, int var_slot, calc_t x) {
    const calc_t step = 1e-7 * (std::fabs(x) > 1.0 ? std::fabs(x) : 1.0);
    const calc_t fp = engine().eval_compiled(h, var_slot, x + step);
    const calc_t fm = engine().eval_compiled(h, var_slot, x - step);
    return (fp - fm) / (2 * step);
}

// Newton from a starting guess. Used directly when there is no
// bracket, and as the polish phase after bisection.
SolveResult newton(void* h, int var_slot, calc_t x0, calc_t tolerance, int max_iter,
                   int iterations_so_far) {
    SolveResult res;
    res.iterations = iterations_so_far;
    calc_t x = x0;
    calc_t fx = engine().eval_compiled(h, var_slot, x);
    for (int i = 0; i < max_iter; ++i) {
        if (!std::isfinite(fx)) {
            break;
        }
        if (std::fabs(fx) <= tolerance) {
            res.converged = true;
            break;
        }
        const calc_t d = deriv(h, var_slot, x);
        if (!std::isfinite(d) || d == 0) {
            break;
        }
        const calc_t dx = fx / d;
        x -= dx;
        fx = engine().eval_compiled(h, var_slot, x);
        ++res.iterations;
        if (std::fabs(dx) <= tolerance * (std::fabs(x) > 1.0 ? std::fabs(x) : 1.0) &&
            std::isfinite(fx)) {
            res.converged = true;
            break;
        }
    }
    res.root = x;
    res.residual = std::fabs(fx);
    if (!res.converged) {
        res.error = "No solution found";
    }
    return res;
}

}  // namespace

SolveResult numeric_solve(const char* expr, int var_slot, calc_t lo, calc_t hi, calc_t tolerance,
                          int max_iter) {
    SolveResult res;
    if (var_slot < 0 || var_slot >= Variables::kCount) {
        res.error = "Bad variable";
        return res;
    }
    void* h = engine().compile(expr);
    if (h == nullptr) {
        res.error = "Syntax error";
        return res;
    }
    const calc_t saved = engine().vars().vars[var_slot];
    if (hi < lo) {
        const calc_t t = lo;
        lo = hi;
        hi = t;
    }

    calc_t fa = engine().eval_compiled(h, var_slot, lo);
    const calc_t fb = engine().eval_compiled(h, var_slot, hi);
    const bool bracketed =
        std::isfinite(fa) && std::isfinite(fb) && ((fa <= 0 && fb >= 0) || (fa >= 0 && fb <= 0));

    if (!bracketed) {
        // Newton from the midpoint guess (spec: fall back when the
        // bounds don't straddle a sign change; lo == hi is the
        // explicit-guess form).
        res = newton(h, var_slot, 0.5 * (lo + hi), tolerance, max_iter, 2);
    } else {
        // Bisection to a tight interval, then a short Newton polish
        // for the last digits.
        calc_t a = lo;
        calc_t b = hi;
        calc_t mid = 0.5 * (a + b);
        res.iterations = 2;
        if (fa == 0) {
            mid = a;
        } else if (fb == 0) {
            mid = b;
        } else {
            for (int i = 0; i < max_iter; ++i) {
                mid = 0.5 * (a + b);
                const calc_t fm = engine().eval_compiled(h, var_slot, mid);
                ++res.iterations;
                if (!std::isfinite(fm) || fm == 0 ||
                    b - a <= tolerance * (std::fabs(mid) > 1.0 ? std::fabs(mid) : 1.0)) {
                    break;
                }
                if ((fa <= 0) == (fm <= 0)) {
                    a = mid;
                    fa = fm;
                } else {
                    b = mid;
                }
            }
        }
        const SolveResult polished = newton(h, var_slot, mid, tolerance, 8, res.iterations);
        const calc_t fmid = std::fabs(engine().eval_compiled(h, var_slot, mid));
        // Keep the polish only when it actually improved the residual
        // and stayed finite (Newton can escape the bracket).
        if (polished.residual <= fmid && std::isfinite(polished.root)) {
            res = polished;
            res.converged = true;
            res.error = nullptr;
        } else {
            res.converged = true;
            res.root = mid;
            res.residual = fmid;
            res.iterations += 1;
        }
    }

    engine().vars().vars[var_slot] = saved;
    engine().free_compiled(h);
    if (res.converged && !std::isfinite(res.root)) {
        res.converged = false;
        res.error = "No solution found";
    }
    return res;
}

SolveResult numeric_solve_equation(const char* lhs, const char* rhs, int var_slot, calc_t lo,
                                   calc_t hi, calc_t tolerance, int max_iter) {
    char composed[256];
    const int n = std::snprintf(composed, sizeof(composed), "(%s)-(%s)", lhs, rhs);
    if (n < 0 || n >= static_cast<int>(sizeof(composed))) {
        SolveResult res;
        res.error = "Expression too long";
        return res;
    }
    return numeric_solve(composed, var_slot, lo, hi, tolerance, max_iter);
}

}  // namespace math
