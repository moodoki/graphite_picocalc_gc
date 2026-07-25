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
    void* h = engine().compile(expr, var_slot);
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

namespace {

// Adapter so the expr-string wrappers share the callback cores: a
// compiled handle evaluated at the given variable slot.
struct CompiledCtx {
    void* handle;
    int var_slot;
};

calc_t eval_compiled_ctx(void* ctx, calc_t x) {
    auto* c = static_cast<CompiledCtx*>(ctx);
    return engine().eval_compiled(c->handle, c->var_slot, x);
}

// Runs `body` with expr compiled into a CompiledCtx, handling the
// compile error / save-restore boilerplate shared by the wrappers.
template <typename Result, typename Body>
Result with_compiled(const char* expr, int var_slot, Body body) {
    Result res;
    if (var_slot < 0 || var_slot >= Variables::kCount) {
        res.error = "Bad variable";
        return res;
    }
    void* h = engine().compile(expr, var_slot);
    if (h == nullptr) {
        res.error = "Syntax error";
        return res;
    }
    const calc_t saved = engine().vars().vars[var_slot];
    CompiledCtx ctx{h, var_slot};
    res = body(&ctx);
    engine().vars().vars[var_slot] = saved;
    engine().free_compiled(h);
    return res;
}

}  // namespace

ExtremumResult numeric_extremum_fn(EvalFn f, void* ctx, calc_t lo, calc_t hi, bool find_max,
                                   calc_t tolerance, int max_iter) {
    ExtremumResult res;
    if (hi < lo) {
        const calc_t t = lo;
        lo = hi;
        hi = t;
    }
    // Brent's localmin (Algorithms for Minimization without
    // Derivatives, ch. 5): golden-section with parabolic
    // interpolation. Maximum = minimum of -f.
    const calc_t sign = find_max ? -1.0 : 1.0;
    constexpr calc_t kGold = 0.3819660112501051;  // (3 - sqrt(5)) / 2
    calc_t a = lo;
    calc_t b = hi;
    calc_t x = a + kGold * (b - a);
    calc_t w = x;
    calc_t v = x;
    calc_t fx = sign * f(ctx, x);
    calc_t fw = fx;
    calc_t fv = fx;
    if (!std::isfinite(fx)) {
        res.error = "Undefined in interval";
        return res;
    }
    calc_t d = 0;
    calc_t e = 0;
    for (int i = 0; i < max_iter; ++i) {
        ++res.iterations;
        const calc_t mid = 0.5 * (a + b);
        const calc_t tol1 = tolerance * (std::fabs(x) > 1.0 ? std::fabs(x) : 1.0) + 1e-15;
        const calc_t tol2 = 2 * tol1;
        if (std::fabs(x - mid) <= tol2 - 0.5 * (b - a)) {
            res.converged = true;
            break;
        }
        bool golden = true;
        if (std::fabs(e) > tol1) {
            // Fit a parabola through (v, fv), (w, fw), (x, fx).
            const calc_t r = (x - w) * (fx - fv);
            calc_t q = (x - v) * (fx - fw);
            calc_t p = (x - v) * q - (x - w) * r;
            q = 2 * (q - r);
            if (q > 0) {
                p = -p;
            }
            q = std::fabs(q);
            const calc_t e_prev = e;
            e = d;
            // Accept the parabolic step only if it falls inside the
            // bracket and beats half the step before last.
            if (std::fabs(p) < std::fabs(0.5 * q * e_prev) && p > q * (a - x) && p < q * (b - x)) {
                d = p / q;
                const calc_t u = x + d;
                if (u - a < tol2 || b - u < tol2) {
                    d = mid > x ? tol1 : -tol1;
                }
                golden = false;
            }
        }
        if (golden) {
            e = (mid > x ? b : a) - x;
            d = kGold * e;
        }
        const calc_t u = std::fabs(d) >= tol1 ? x + d : x + (d > 0 ? tol1 : -tol1);
        const calc_t fu = sign * f(ctx, u);
        if (!std::isfinite(fu)) {
            break;
        }
        if (fu <= fx) {
            if (u >= x) {
                a = x;
            } else {
                b = x;
            }
            v = w;
            fv = fw;
            w = x;
            fw = fx;
            x = u;
            fx = fu;
        } else {
            if (u < x) {
                a = u;
            } else {
                b = u;
            }
            if (fu <= fw || w == x) {
                v = w;
                fv = fw;
                w = u;
                fw = fu;
            } else if (fu <= fv || v == x || v == w) {
                v = u;
                fv = fu;
            }
        }
    }
    res.x = x;
    res.value = sign * fx;
    if (!res.converged) {
        res.error = "No extremum found";
    }
    return res;
}

ExtremumResult numeric_extremum(const char* expr, int var_slot, calc_t lo, calc_t hi, bool find_max,
                                calc_t tolerance, int max_iter) {
    return with_compiled<ExtremumResult>(expr, var_slot, [&](CompiledCtx* ctx) {
        return numeric_extremum_fn(eval_compiled_ctx, ctx, lo, hi, find_max, tolerance, max_iter);
    });
}

DerivResult numeric_derivative_fn(EvalFn f, void* ctx, calc_t at, calc_t h) {
    DerivResult res;
    if (h <= 0) {
        h = 1e-4 * (std::fabs(at) > 1.0 ? std::fabs(at) : 1.0);
    }
    // Central differences at h and h/2, one Richardson step: the h^2
    // error terms cancel, leaving O(h^4).
    auto central = [&](calc_t step) {
        return (f(ctx, at + step) - f(ctx, at - step)) / (2 * step);
    };
    const calc_t d1 = central(h);
    const calc_t d2 = central(0.5 * h);
    res.value = (4 * d2 - d1) / 3;
    res.ok = std::isfinite(res.value);
    if (!res.ok) {
        res.error = "Undefined at point";
    }
    return res;
}

DerivResult numeric_derivative(const char* expr, int var_slot, calc_t at, calc_t h) {
    return with_compiled<DerivResult>(expr, var_slot, [&](CompiledCtx* ctx) {
        return numeric_derivative_fn(eval_compiled_ctx, ctx, at, h);
    });
}

namespace {

// Gauss-Kronrod 7-15 nodes and weights (QUADPACK dqk15). Positive
// half; xgk odd indices + the center are the embedded G7 points.
constexpr calc_t kXgk[8] = {
    0.9914553711208126, 0.9491079123427585, 0.8648644233597691, 0.7415311855993944,
    0.5860872354676911, 0.4058451513773972, 0.2077849550078985, 0.0};
constexpr calc_t kWgk[8] = {0.0229353220105292, 0.0630920926299785, 0.1047900103222502,
                            0.1406532597155259, 0.1690047266392679, 0.1903505780647854,
                            0.2044329400752989, 0.2094821410847278};
constexpr calc_t kWg[4] = {0.1294849661688697, 0.2797053914892767, 0.3818300505051189,
                           0.4179591836734694};

// One G7-K15 panel over [a, b]. Returns false when the integrand is
// non-finite at a node.
bool gk15(EvalFn f, void* ctx, calc_t a, calc_t b, calc_t* k15, calc_t* err, int* evals) {
    const calc_t c = 0.5 * (a + b);
    const calc_t half = 0.5 * (b - a);
    const calc_t fc = f(ctx, c);
    *evals += 1;
    if (!std::isfinite(fc)) {
        return false;
    }
    calc_t kron = kWgk[7] * fc;
    calc_t gauss = kWg[3] * fc;
    for (int j = 0; j < 7; ++j) {
        const calc_t dx = half * kXgk[j];
        const calc_t f1 = f(ctx, c - dx);
        const calc_t f2 = f(ctx, c + dx);
        *evals += 2;
        if (!std::isfinite(f1) || !std::isfinite(f2)) {
            return false;
        }
        kron += kWgk[j] * (f1 + f2);
        if (j % 2 == 1) {  // j = 1, 3, 5 are the Gauss points
            gauss += kWg[j / 2] * (f1 + f2);
        }
    }
    *k15 = kron * half;
    *err = std::fabs((kron - gauss) * half);
    return true;
}

constexpr int kMaxIntegralDepth = 12;

// Adaptive bisection: accept a panel when its G7/K15 discrepancy is
// within its length-proportional share of the tolerance; otherwise
// split. At the depth cap the panel is accepted as-is (Risk 4 — never
// hang on an oscillatory/singular integrand).
bool integrate_panel(EvalFn f, void* ctx, calc_t a, calc_t b, calc_t tol_share, int depth,
                     IntegralResult* out) {
    calc_t k15 = 0;
    calc_t err = 0;
    if (!gk15(f, ctx, a, b, &k15, &err, &out->evals)) {
        if (depth >= kMaxIntegralDepth) {
            return false;
        }
        // Non-finite node: bisect toward the trouble spot; the bad
        // panel keeps shrinking until the cap rejects it.
        const calc_t mid = 0.5 * (a + b);
        return integrate_panel(f, ctx, a, mid, 0.5 * tol_share, depth + 1, out) &&
               integrate_panel(f, ctx, mid, b, 0.5 * tol_share, depth + 1, out);
    }
    if (err <= tol_share || depth >= kMaxIntegralDepth) {
        out->value += k15;
        out->error_est += err;
        return true;
    }
    const calc_t mid = 0.5 * (a + b);
    return integrate_panel(f, ctx, a, mid, 0.5 * tol_share, depth + 1, out) &&
           integrate_panel(f, ctx, mid, b, 0.5 * tol_share, depth + 1, out);
}

}  // namespace

IntegralResult numeric_integral_fn(EvalFn f, void* ctx, calc_t a, calc_t b, calc_t tolerance) {
    IntegralResult res;
    if (a == b) {
        res.converged = true;
        return res;
    }
    calc_t sign = 1.0;
    if (b < a) {
        const calc_t t = a;
        a = b;
        b = t;
        sign = -1.0;
    }
    if (!std::isfinite(a) || !std::isfinite(b)) {
        res.error = "Bad bounds";
        return res;
    }
    if (!integrate_panel(f, ctx, a, b, tolerance, 0, &res)) {
        res.value = 0;
        res.error = "Undefined in interval";
        return res;
    }
    res.value *= sign;
    res.converged = std::isfinite(res.value);
    if (!res.converged) {
        res.error = "No result";
    }
    return res;
}

IntegralResult numeric_integral(const char* expr, int var_slot, calc_t a, calc_t b,
                                calc_t tolerance) {
    return with_compiled<IntegralResult>(expr, var_slot, [&](CompiledCtx* ctx) {
        return numeric_integral_fn(eval_compiled_ctx, ctx, a, b, tolerance);
    });
}

}  // namespace math
