#include "graph/analysis.hpp"

#include <cmath>

#include "math/engine.hpp"
#include "math/numeric_solve.hpp"

namespace graph {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kTSlot = 't' - 'a';
constexpr int kXSlot = 'x' - 'a';

int indep_slot(Mode mode) {
    switch (mode) {
        case Mode::kParametric:
            return kTSlot;
        case Mode::kPolar:
            return math::Variables::kTheta;
        default:
            return kXSlot;
    }
}

// Independent-variable span the derivative step and intersect bracket
// scale with (spec §4.2: h ~= 1e-4 * range).
double indep_span(const GraphState& st) {
    double span = 0;
    switch (st.mode) {
        case Mode::kParametric:
            span = st.t_max - st.t_min;
            break;
        case Mode::kPolar:
            span = st.theta_max - st.theta_min;
            break;
        default:
            span = st.window.x_max - st.window.x_min;
            break;
    }
    return span > 0 ? span : 1.0;
}

// The expression solved/extremized per mode: Y (function), Y_T
// (parametric — zeros and extrema are of y, spec §4.1), r (polar).
// Returns nullptr when the slot is out of range or empty.
const char* target_expr(const GraphState& st, int slot) {
    switch (st.mode) {
        case Mode::kParametric:
            if (slot < 0 || slot >= kParametricSlots || st.param.x_expr[slot][0] == 0 ||
                st.param.y_expr[slot][0] == 0) {
                return nullptr;
            }
            return st.param.y_expr[slot];
        case Mode::kPolar:
            if (slot < 0 || slot >= kPolarSlots || st.polar.expr[slot][0] == 0) {
                return nullptr;
            }
            return st.polar.expr[slot];
        default:
            if (slot < 0 || slot >= kFunctionSlots || st.y.expr[slot][0] == 0) {
                return nullptr;
            }
            return st.y.expr[slot];
    }
}

// Polar theta -> Cartesian, honoring degree mode like PolarSource.
void polar_xy(double r, double theta, double* x, double* y) {
    const double tr = math::angle_mode() == math::AngleMode::kDegrees ? theta * kPi / 180.0 : theta;
    *x = r * std::cos(tr);
    *y = r * std::sin(tr);
}

// Compiled-handle callback context for the math primitives.
struct HandleCtx {
    void* h;
    int slot;
};

math::calc_t eval_handle(void* ctx, math::calc_t v) {
    auto* c = static_cast<HandleCtx*>(ctx);
    return math::engine().eval_compiled(c->h, c->slot, v);
}

// Fill the plot-space point (x, y and polar aux r) for an independent
// value on a slot. Shared by every operation's result reporting.
// Returns false (with res->error set) on compile failure or a
// non-finite point.
bool fill_point(const GraphState& st, int slot, double indep, AnalysisResult* res) {
    auto& eng = math::engine();
    res->indep = indep;
    switch (st.mode) {
        case Mode::kParametric: {
            void* xh = eng.compile(st.param.x_expr[slot]);
            void* yh = eng.compile(st.param.y_expr[slot]);
            if (xh == nullptr || yh == nullptr) {
                eng.free_compiled(xh);
                eng.free_compiled(yh);
                res->error = "Syntax error";
                return false;
            }
            const math::calc_t saved = eng.vars().vars[kTSlot];
            res->x = eng.eval_compiled(xh, kTSlot, indep);
            res->y = eng.eval_compiled(yh, kTSlot, indep);
            eng.vars().vars[kTSlot] = saved;
            eng.free_compiled(xh);
            eng.free_compiled(yh);
            break;
        }
        case Mode::kPolar: {
            void* rh = eng.compile(st.polar.expr[slot]);
            if (rh == nullptr) {
                res->error = "Syntax error";
                return false;
            }
            const math::calc_t saved = eng.vars().vars[math::Variables::kTheta];
            const double r = eng.eval_compiled(rh, math::Variables::kTheta, indep);
            eng.vars().vars[math::Variables::kTheta] = saved;
            eng.free_compiled(rh);
            res->aux = r;
            polar_xy(r, indep, &res->x, &res->y);
            break;
        }
        default: {
            void* h = eng.compile(st.y.expr[slot]);
            if (h == nullptr) {
                res->error = "Syntax error";
                return false;
            }
            const math::calc_t saved = eng.vars().vars[kXSlot];
            res->x = indep;
            res->y = eng.eval_compiled(h, kXSlot, indep);
            eng.vars().vars[kXSlot] = saved;
            eng.free_compiled(h);
            break;
        }
    }
    if (!std::isfinite(res->x) || !std::isfinite(res->y)) {
        res->error = "Undefined at point";
        return false;
    }
    return true;
}

}  // namespace

AnalysisResult analyze_value(const GraphState& st, int slot, double indep) {
    AnalysisResult res;
    if (target_expr(st, slot) == nullptr) {
        res.error = "No function";
        return res;
    }
    res.ok = fill_point(st, slot, indep, &res);
    return res;
}

AnalysisResult analyze_zero(const GraphState& st, int slot, double lo, double hi, double guess) {
    AnalysisResult res;
    const char* expr = target_expr(st, slot);
    if (expr == nullptr) {
        res.error = "No function";
        return res;
    }
    if (hi < lo) {
        const double t = lo;
        lo = hi;
        hi = t;
    }
    const int var = indep_slot(st.mode);
    math::SolveResult sr = math::numeric_solve(expr, var, lo, hi);
    const double slack = 1e-6 * (hi - lo > 0 ? hi - lo : 1.0);
    if (!sr.converged || sr.root < lo - slack || sr.root > hi + slack) {
        // No sign change (or Newton escaped the bracket): retry from
        // the guess, still requiring the root inside the bounds.
        const math::SolveResult sg = math::numeric_solve(expr, var, guess, guess);
        if (sg.converged && sg.root >= lo - slack && sg.root <= hi + slack) {
            sr = sg;
        } else {
            res.error = sr.error != nullptr ? sr.error : "No zero in bounds";
            if (sr.converged) {
                res.error = "No zero in bounds";
            }
            return res;
        }
    }
    res.ok = fill_point(st, slot, sr.root, &res);
    return res;
}

AnalysisResult analyze_extremum(const GraphState& st, int slot, double lo, double hi,
                                bool find_max) {
    AnalysisResult res;
    const char* expr = target_expr(st, slot);
    if (expr == nullptr) {
        res.error = "No function";
        return res;
    }
    const math::ExtremumResult er =
        math::numeric_extremum(expr, indep_slot(st.mode), lo, hi, find_max);
    if (!er.converged) {
        res.error = er.error;
        return res;
    }
    res.ok = fill_point(st, slot, er.x, &res);
    return res;
}

AnalysisResult analyze_intersect(const GraphState& st, int slot_a, int slot_b, double guess) {
    AnalysisResult res;
    const char* ea = target_expr(st, slot_a);
    const char* eb = target_expr(st, slot_b);
    if (ea == nullptr || eb == nullptr) {
        res.error = "No function";
        return res;
    }
    if (slot_a == slot_b) {
        res.error = "Same curve";
        return res;
    }
    const int var = indep_slot(st.mode);
    // Bracket around the guess: a sign change inside it bisects; none
    // falls back to Newton from the guess (numeric_solve midpoint).
    const double d = 0.05 * indep_span(st);
    math::SolveResult sr = math::numeric_solve_equation(ea, eb, var, guess - d, guess + d);
    if (!sr.converged) {
        // Widen once before giving up — the nearest crossing may sit
        // outside the 5% bracket.
        sr = math::numeric_solve_equation(ea, eb, var, guess - 5 * d, guess + 5 * d);
    }
    if (!sr.converged) {
        res.error = sr.error != nullptr ? sr.error : "No intersection";
        return res;
    }
    res.ok = fill_point(st, slot_a, sr.root, &res);
    return res;
}

AnalysisResult analyze_derivative(const GraphState& st, int slot, double at) {
    AnalysisResult res;
    if (target_expr(st, slot) == nullptr) {
        res.error = "No function";
        return res;
    }
    if (!fill_point(st, slot, at, &res)) {
        return res;
    }
    auto& eng = math::engine();
    const double h = 1e-4 * indep_span(st);
    math::DerivResult dr;
    switch (st.mode) {
        case Mode::kParametric: {
            // Slope of the curve: (dy/dt) / (dx/dt).
            const math::DerivResult dy =
                math::numeric_derivative(st.param.y_expr[slot], kTSlot, at, h);
            const math::DerivResult dx =
                math::numeric_derivative(st.param.x_expr[slot], kTSlot, at, h);
            dr.ok = dy.ok && dx.ok;
            dr.value = dy.value / dx.value;
            break;
        }
        case Mode::kPolar: {
            // Differentiate x(theta) = r cos, y(theta) = r sin — the
            // ratio equals the polar slope formula and the degree-mode
            // unit factors cancel.
            void* rh = eng.compile(st.polar.expr[slot]);
            if (rh == nullptr) {
                res.error = "Syntax error";
                res.ok = false;
                return res;
            }
            const math::calc_t saved = eng.vars().vars[math::Variables::kTheta];
            HandleCtx ctx{rh, math::Variables::kTheta};
            // Two small named thunks keep the EvalFn signature.
            struct Thunks {
                static math::calc_t eval_y(void* c, math::calc_t theta) {
                    const double r = eval_handle(c, theta);
                    double x = 0;
                    double y = 0;
                    polar_xy(r, theta, &x, &y);
                    return y;
                }
                static math::calc_t eval_x(void* c, math::calc_t theta) {
                    const double r = eval_handle(c, theta);
                    double x = 0;
                    double y = 0;
                    polar_xy(r, theta, &x, &y);
                    return x;
                }
            };
            const math::DerivResult dy = math::numeric_derivative_fn(&Thunks::eval_y, &ctx, at, h);
            const math::DerivResult dx = math::numeric_derivative_fn(&Thunks::eval_x, &ctx, at, h);
            eng.vars().vars[math::Variables::kTheta] = saved;
            eng.free_compiled(rh);
            dr.ok = dy.ok && dx.ok;
            dr.value = dy.value / dx.value;
            break;
        }
        default:
            dr = math::numeric_derivative(st.y.expr[slot], kXSlot, at, h);
            break;
    }
    if (!dr.ok || !std::isfinite(dr.value)) {
        res.ok = false;
        res.error = "Undefined slope";
        return res;
    }
    res.aux = dr.value;
    res.ok = true;
    return res;
}

namespace {

// Parametric fnInt integrand: Y(t) * X'(t) (spec §4.1).
struct ParamIntegrand {
    void* xh;
    void* yh;
    double h;  // Central-difference step for X'
};

math::calc_t eval_param_integrand(void* ctx, math::calc_t t) {
    auto* c = static_cast<ParamIntegrand*>(ctx);
    auto& eng = math::engine();
    const math::calc_t y = eng.eval_compiled(c->yh, kTSlot, t);
    const math::calc_t xp =
        (eng.eval_compiled(c->xh, kTSlot, t + c->h) - eng.eval_compiled(c->xh, kTSlot, t - c->h)) /
        (2 * c->h);
    return y * xp;
}

// Polar area integrand: r(theta)^2.
math::calc_t eval_r_squared(void* ctx, math::calc_t theta) {
    const math::calc_t r = eval_handle(ctx, theta);
    return r * r;
}

}  // namespace

AnalysisResult analyze_integral(const GraphState& st, int slot, double a, double b) {
    AnalysisResult res;
    if (target_expr(st, slot) == nullptr) {
        res.error = "No function";
        return res;
    }
    auto& eng = math::engine();
    math::IntegralResult ir;
    switch (st.mode) {
        case Mode::kParametric: {
            void* xh = eng.compile(st.param.x_expr[slot]);
            void* yh = eng.compile(st.param.y_expr[slot]);
            if (xh == nullptr || yh == nullptr) {
                eng.free_compiled(xh);
                eng.free_compiled(yh);
                res.error = "Syntax error";
                return res;
            }
            const math::calc_t saved = eng.vars().vars[kTSlot];
            const double span = std::fabs(b - a);
            ParamIntegrand ctx{xh, yh, 1e-5 * (span > 0 ? span : 1.0)};
            ir = math::numeric_integral_fn(eval_param_integrand, &ctx, a, b);
            eng.vars().vars[kTSlot] = saved;
            eng.free_compiled(xh);
            eng.free_compiled(yh);
            break;
        }
        case Mode::kPolar: {
            void* rh = eng.compile(st.polar.expr[slot]);
            if (rh == nullptr) {
                res.error = "Syntax error";
                return res;
            }
            const math::calc_t saved = eng.vars().vars[math::Variables::kTheta];
            HandleCtx ctx{rh, math::Variables::kTheta};
            ir = math::numeric_integral_fn(eval_r_squared, &ctx, a, b);
            eng.vars().vars[math::Variables::kTheta] = saved;
            eng.free_compiled(rh);
            // Area = (1/2) int r^2 dtheta with theta in radians; a
            // degree-mode sweep integrates over degrees, so rescale.
            ir.value *= 0.5;
            if (math::angle_mode() == math::AngleMode::kDegrees) {
                ir.value *= kPi / 180.0;
            }
            break;
        }
        default:
            ir = math::numeric_integral(st.y.expr[slot], kXSlot, a, b);
            break;
    }
    if (!ir.converged) {
        res.error = ir.error != nullptr ? ir.error : "No result";
        return res;
    }
    // Report the upper-limit point for the readout/cursor; a curve
    // undefined exactly at b shouldn't void the integral itself.
    // fill_point sets aux = r in polar mode, so the integral value
    // lands in aux afterwards.
    if (!fill_point(st, slot, b, &res)) {
        res.indep = b;
        res.error = nullptr;
    }
    res.aux = ir.value;
    res.ok = true;
    return res;
}

}  // namespace graph
