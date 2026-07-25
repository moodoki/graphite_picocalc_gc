#include "apps/graph_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "pico/time.h"

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/seq_expr.hpp"
#include "apps/calc_menu.hpp"
#include "apps/graph_model.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/split_screen.hpp"
#include "apps/table_screen.hpp"
#include "apps/window_screen.hpp"
#include "graph/function_source.hpp"
#include "graph/parametric_source.hpp"
#include "graph/plotter.hpp"
#include "graph/polar_source.hpp"
#include "graph/seq_points.hpp"
#include "graph/stat_plot.hpp"

namespace apps {

namespace {
// Polar independent-variable label: the real theta glyph, was ASCII
// "th" (testdrive 2026-07-21).
const char kThetaLabel[] = {gfx::kGlyphTheta, 0};

// Clamp a pixel coordinate for the int16 caches (far-offscreen values
// keep line joins sane; truly undefined points use kOffscreen).
int16_t clamp_px(int v) {
    constexpr int kClamp = graph::Plotter::kClampPy;
    return static_cast<int16_t>(v < -kClamp ? -kClamp : (v > kClamp ? kClamp : v));
}

graph::Mode mode() {
    return graph::state().mode;
}

// Grid/label thinning: the smallest integer k >= 1 such that k grid
// steps span at least min_px pixels. `per_px` is the pixel span of one
// scl step. Used to coarsen a too-dense grid (thousands of merged lines
// when scl is tiny relative to the range — slow, and an illegible wash)
// down to the largest meaningful grid, and to keep tick labels spaced.
int thin_factor(double per_px, double min_px) {
    if (per_px >= min_px || per_px <= 0.0) {
        return 1;
    }
    // Cap before the int cast: an absurdly small scl relative to the
    // range makes min_px/per_px astronomically large (int overflow / UB).
    // The cap still keeps the line count bounded for any sane input.
    const double k = std::ceil(min_px / per_px);
    constexpr double kMaxFactor = 1'000'000.0;
    return static_cast<int>(k < kMaxFactor ? k : kMaxFactor);
}

// Minimum pixels between drawn grid lines. Below this they merge into a
// solid wash and cost render time for nothing, so the step is coarsened
// to the smallest multiple of scl that clears this spacing.
constexpr double kMinGridPx = 4.0;

// Parametric and polar both sweep a parameter into the shared
// point-per-step cache; function mode uses the column cache.
bool param_style() {
    return mode() != graph::Mode::kFunction;
}
}  // namespace

void GraphScreen::on_activate() {
    dirty_ = true;
    // A fresh activation never resumes a half-collected CALC session;
    // CalcMenuScreen pops (activating us) before starting the new one.
    analysis_.cancel();
    analysis_line_[0] = 0;
}

void GraphScreen::start_trace() {
    trace_.active = true;
    // Start on the first active slot.
    for (int i = 0; i < slot_count(); ++i) {
        if (slot_active(i)) {
            trace_.slot = i;
            break;
        }
    }
    trace_.clamp(trace_max_index());
}

void GraphScreen::begin_analysis(graph::AnalysisOp op) {
    if (mode() == graph::Mode::kSeq) {
        return;  // CALC ops are continuous-curve constructs (4D v1)
    }
    if (dirty_) {
        recompute();  // Typed-command entry: slot caches may be stale
    }
    int first = -1;
    for (int i = 0; i < slot_count(); ++i) {
        if (slot_active(i)) {
            first = i;
            break;
        }
    }
    if (first < 0) {
        return;  // Nothing plots — the on-screen hint already says so
    }
    trace_.active = false;
    trace_.slot = first;
    trace_.clamp(trace_max_index());
    analysis_.begin(op, first);
    analysis_line_[0] = 0;
}

// Modal key handling while a CALC session collects inputs or shows its
// result: arrows ride the cursor, ENTER commits, ESC cancels;
// everything else is swallowed (zoom or screen switches would
// invalidate the collected world-space inputs).
bool GraphScreen::handle_analysis_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (analysis_.done) {
        if (ev.key == Key::kF6) {  // Straight back to the menu
            ui::screen_manager().push(&calc_menu());
            return true;
        }
        if (ev.key == Key::kEscape || ev.key == Key::kEnter) {
            analysis_.cancel();
            analysis_line_[0] = 0;
        }
        return true;
    }
    switch (ev.key) {
        case Key::kLeft:
            trace_.step(-1, trace_max_index());
            return true;
        case Key::kRight:
            trace_.step(+1, trace_max_index());
            return true;
        case Key::kUp:
        case Key::kDown:
            // Cycle curves until the operation locks onto one (TI
            // behavior; intersect keeps cycling through both picks).
            if (!analysis_.slot_locked()) {
                for (int step = 0; step < slot_count(); ++step) {
                    trace_.slot = (trace_.slot + 1) % slot_count();
                    if (slot_active(trace_.slot)) {
                        break;
                    }
                }
                trace_.clamp(trace_max_index());
            }
            return true;
        case Key::kEnter:
            if (analysis_.commit(trace_.slot, trace_value())) {
                analysis_.compute(graph::state());
                finish_analysis();
            }
            return true;
        case Key::kEscape:
            analysis_.cancel();
            return true;
        default:
            return true;
    }
}

void GraphScreen::finish_analysis() {
    const auto& r = analysis_.result;
    const auto op = analysis_.op;
    char ib[24];
    char xb[24];
    char yb[24];
    if (!r.ok) {
        std::snprintf(analysis_line_, sizeof(analysis_line_), "%s: %s", graph::analysis_op_name(op),
                      r.error != nullptr ? r.error : "Error");
        return;
    }

    // Park the cursor on the result and store it TI-style: the mode's
    // independent variable gets the location, Ans the headline value.
    sync_trace_to_value(r.indep);
    trace_.active = false;
    auto& vars = math::engine().vars();
    int vslot = 'x' - 'a';
    const char* vname = "x";
    if (mode() == graph::Mode::kParametric) {
        vslot = 't' - 'a';
        vname = "t";
    } else if (mode() == graph::Mode::kPolar) {
        vslot = math::Variables::kTheta;
        vname = kThetaLabel;
    }
    vars.set_real(vslot, r.indep);
    switch (op) {
        case graph::AnalysisOp::kZero:
        case graph::AnalysisOp::kIntersect:
            vars.set_real(math::Variables::kAns, r.indep);
            break;
        case graph::AnalysisOp::kDerivative:
        case graph::AnalysisOp::kIntegral:
            vars.set_real(math::Variables::kAns, r.aux);
            break;
        default:  // value, min, max
            vars.set_real(math::Variables::kAns, r.y);
            break;
    }

    math::format_number(r.indep, ib, sizeof(ib));
    math::format_number(r.x, xb, sizeof(xb));
    math::format_number(r.y, yb, sizeof(yb));
    switch (op) {
        case graph::AnalysisOp::kDerivative: {
            char ab[24];
            math::format_number(r.aux, ab, sizeof(ab));
            std::snprintf(analysis_line_, sizeof(analysis_line_), "dy/dx=%s  %s=%s", ab, vname, ib);
            break;
        }
        case graph::AnalysisOp::kIntegral: {
            char ab[24];
            char lb[24];
            math::format_number(r.aux, ab, sizeof(ab));
            math::format_number(analysis_.vals[0], lb, sizeof(lb));
            std::snprintf(analysis_line_, sizeof(analysis_line_), "Int=%s  %s=%s..%s", ab, vname,
                          lb, ib);
            break;
        }
        default:
            if (mode() == graph::Mode::kFunction) {
                std::snprintf(analysis_line_, sizeof(analysis_line_), "%s  x=%s y=%s",
                              graph::analysis_op_name(op), xb, yb);
            } else {
                std::snprintf(analysis_line_, sizeof(analysis_line_), "%s  %s=%s x=%s y=%s",
                              graph::analysis_op_name(op), vname, ib, xb, yb);
            }
            break;
    }
}

graph::Viewport GraphScreen::viewport() const {
    const auto& w = graph_window();
    graph::Viewport vp;
    vp.x_min = w.x_min;
    vp.x_max = w.x_max;
    vp.y_min = w.y_min;
    vp.y_max = w.y_max;
    vp.left = 0;
    vp.top = top_;
    vp.width = kWidth;
    vp.height = height_;
    return vp;
}

int GraphScreen::slot_count() const {
    switch (mode()) {
        case graph::Mode::kParametric:
            return graph::kParametricSlots;
        case graph::Mode::kPolar:
            return graph::kPolarSlots;
        case graph::Mode::kSeq:
            return graph::kSeqSlots;
        default:
            return graph::kFunctionSlots;
    }
}

bool GraphScreen::slot_active(int s) const {
    return param_style() ? pactive_[s] : active_[s];
}

int GraphScreen::trace_max_index() const {
    if (!param_style()) {
        return kWidth - 1;
    }
    return pcount_[trace_.slot] > 0 ? pcount_[trace_.slot] - 1 : 0;
}

double GraphScreen::trace_value() const {
    const auto& st = graph::state();
    switch (mode()) {
        case graph::Mode::kParametric:
            return st.t_min + trace_.index * st.t_step;
        case graph::Mode::kPolar:
            return st.theta_min + trace_.index * st.theta_step;
        case graph::Mode::kSeq: {
            // Web polyline points come in pairs per step (4D.7).
            if (st.seq_style == 1) {
                const int steps = trace_.index / 2;
                return st.n_min + steps + 1;
            }
            return st.plot_start + trace_.index * st.plot_step;
        }
        default:
            return viewport().data_x(trace_.index);
    }
}

void GraphScreen::sync_trace_to_value(double v) {
    const auto& st = graph::state();
    trace_.active = true;
    switch (mode()) {
        case graph::Mode::kParametric:
            if (st.t_step > 0) {
                trace_.index = static_cast<int>(std::lround((v - st.t_min) / st.t_step));
            }
            break;
        case graph::Mode::kPolar:
            if (st.theta_step > 0) {
                trace_.index = static_cast<int>(std::lround((v - st.theta_min) / st.theta_step));
            }
            break;
        case graph::Mode::kSeq:
            if (st.seq_style == 1) {
                trace_.index = static_cast<int>(std::lround((v - st.n_min - 1) * 2));
            } else if (st.plot_step > 0) {
                trace_.index = static_cast<int>(std::lround((v - st.plot_start) / st.plot_step));
            }
            break;
        default:
            trace_.index = viewport().px_x(v);
            break;
    }
    trace_.clamp(trace_max_index());
}

void GraphScreen::recompute() {
    const graph::Viewport vp = viewport();
    const uint64_t t0 = time_us_64();

    switch (mode()) {
        case graph::Mode::kParametric:
            recompute_parametric(vp);
            break;
        case graph::Mode::kPolar:
            recompute_polar(vp);
            break;
        case graph::Mode::kSeq:
            recompute_seq(vp);
            break;
        default:
            recompute_function(vp);
            break;
    }
    // Stat-plot caches (3D): histogram bins, box summaries, normprob
    // sorted copies, pixel-space point cache — anything render must not
    // compute per strip (§8).
    graph::recompute_stat_plots(vp);

    last_recompute_us_ = static_cast<uint32_t>(time_us_64() - t0);
    printf("graph recompute: %lu us\n", static_cast<unsigned long>(last_recompute_us_));
    trace_.clamp(trace_max_index());
    dirty_ = false;
}

void GraphScreen::recompute_function(const graph::Viewport& vp) {
    auto& fns = y_functions();
    auto& eng = math::engine();

    // Graphing sweeps the shared X variable; preserve any value the user
    // stored in X on the home screen.
    const math::calc_t saved_x = eng.vars()['x'];

    for (int fi = 0; fi < graph::kFunctionSlots; ++fi) {
        active_[fi] = fns.enabled[fi] && fns.expr[fi][0] != 0;
        if (!active_[fi]) {
            continue;
        }
        void* compiled = eng.compile(fns.expr[fi], 'x' - 'a');
        if (compiled == nullptr) {
            active_[fi] = false;
            continue;
        }
        graph::FunctionSource src(eng, compiled);
        src.begin(vp);
        double x = 0.0;
        double y = 0.0;
        bool defined = false;
        for (int px = 0; px < kWidth && src.next(&x, &y, &defined); ++px) {
            plot_y_[fi][px] = defined ? clamp_px(vp.px_y(y)) : kOffscreen;
        }
        eng.free_compiled(compiled);
    }
    eng.vars()['x'] = saved_x;
}

void GraphScreen::recompute_parametric(const graph::Viewport& vp) {
    auto& st = graph::state();
    auto& eng = math::engine();

    // The sweep writes the shared T variable; preserve the user's value.
    const math::calc_t saved_t = eng.vars()['t'];

    for (int p = 0; p < graph::kParametricSlots; ++p) {
        pcount_[p] = 0;
        pactive_[p] =
            st.param.enabled[p] && st.param.x_expr[p][0] != 0 && st.param.y_expr[p][0] != 0;
        if (!pactive_[p]) {
            continue;
        }
        void* xh = eng.compile(st.param.x_expr[p], 't' - 'a');
        void* yh = eng.compile(st.param.y_expr[p], 't' - 'a');
        if (xh == nullptr || yh == nullptr) {
            pactive_[p] = false;
            eng.free_compiled(xh);
            eng.free_compiled(yh);
            continue;
        }
        graph::ParametricSource src(eng, xh, yh, st.t_min, st.t_max, st.t_step);
        src.begin(vp);
        double x = 0.0;
        double y = 0.0;
        bool defined = false;
        int n = 0;
        while (n < kMaxCurvePoints && src.next(&x, &y, &defined)) {
            if (defined) {
                ppx_[p][n] = clamp_px(vp.px_x(x));
                ppy_[p][n] = clamp_px(vp.px_y(y));
            } else {
                ppx_[p][n] = 0;
                ppy_[p][n] = kOffscreen;
            }
            ++n;
        }
        pcount_[p] = static_cast<int16_t>(n);
        eng.free_compiled(xh);
        eng.free_compiled(yh);
    }
    eng.vars()['t'] = saved_t;
}

void GraphScreen::recompute_polar(const graph::Viewport& vp) {
    auto& st = graph::state();
    auto& eng = math::engine();

    // The sweep writes the dedicated theta slot; preserve the user's value.
    const math::calc_t saved_theta = eng.vars().vars[math::Variables::kTheta];

    for (int p = 0; p < graph::kPolarSlots; ++p) {
        pcount_[p] = 0;
        pactive_[p] = st.polar.enabled[p] && st.polar.expr[p][0] != 0;
        if (!pactive_[p]) {
            continue;
        }
        void* rh = eng.compile(st.polar.expr[p], math::Variables::kTheta);
        if (rh == nullptr) {
            pactive_[p] = false;
            continue;
        }
        graph::PolarSource src(eng, rh, st.theta_min, st.theta_max, st.theta_step);
        src.begin(vp);
        double x = 0.0;
        double y = 0.0;
        bool defined = false;
        int n = 0;
        while (n < kMaxCurvePoints && src.next(&x, &y, &defined)) {
            if (defined) {
                ppx_[p][n] = clamp_px(vp.px_x(x));
                ppy_[p][n] = clamp_px(vp.px_y(y));
            } else {
                ppx_[p][n] = 0;
                ppy_[p][n] = kOffscreen;
            }
            ++n;
        }
        pcount_[p] = static_cast<int16_t>(n);
        eng.free_compiled(rh);
    }
    eng.vars().vars[math::Variables::kTheta] = saved_theta;
}

void GraphScreen::recompute_seq(const graph::Viewport& vp) {
    auto& st = graph::state();
    auto& eng = math::engine();

    // The sweep writes the shared N variable; preserve the user's value.
    const math::calc_t saved_n = eng.vars()['n'];
    math::seqexpr::refresh();  // Pick up edits to referenced variables
    math::seqexpr::begin(graph::make_seq_def(st));
    const bool web = st.seq_style == 1;

    for (int s = 0; s < graph::kSeqSlots; ++s) {
        pcount_[s] = 0;
        active_[s] = false;
        const bool on = st.seq.enabled[s] && st.seq.expr[s][0] != 0 && math::seqexpr::defined(s);
        // Web plots need a pure own-lag-1 recurrence (4D.7); others skip.
        pactive_[s] = on && (!web || math::seqexpr::lag1_only(s));
        if (!pactive_[s]) {
            continue;
        }
        if (!web) {
            // Time series: (n, u(n)) into the parameter-step cache.
            graph::SeqSource src(s, st.plot_start, st.n_max, st.plot_step);
            src.begin(vp);
            double x = 0.0;
            double y = 0.0;
            bool defined = false;
            int n = 0;
            while (n < kMaxCurvePoints && src.next(&x, &y, &defined)) {
                if (defined) {
                    ppx_[s][n] = clamp_px(vp.px_x(x));
                    ppy_[s][n] = clamp_px(vp.px_y(y));
                } else {
                    ppx_[s][n] = 0;
                    ppy_[s][n] = kOffscreen;
                }
                ++n;
            }
            pcount_[s] = static_cast<int16_t>(n);
            continue;
        }
        // Web plot: the map curve y=f(x) rides the (otherwise free)
        // function-mode column cache; the cobweb path — (u_k, u_k+1)
        // then across to the diagonal — rides the point cache. render()
        // adds the y=x line from pure viewport math.
        active_[s] = true;
        for (int px = 0; px < kWidth; ++px) {
            const double y = math::seqexpr::map_value(s, vp.data_x(px));
            plot_y_[s][px] = std::isfinite(y) ? clamp_px(vp.px_y(y)) : kOffscreen;
        }
        const long n0 = std::lround(st.n_min);
        const long nmax = std::lround(st.n_max);
        double prev = math::seqexpr::value(s, n0);
        int n = 0;
        for (long k = n0 + 1; k <= nmax && n + 1 < kMaxCurvePoints; ++k) {
            const double cur = math::seqexpr::value(s, k);
            if (!std::isfinite(prev) || !std::isfinite(cur)) {
                break;
            }
            ppx_[s][n] = clamp_px(vp.px_x(prev));
            ppy_[s][n] = clamp_px(vp.px_y(cur));
            ++n;
            ppx_[s][n] = clamp_px(vp.px_x(cur));
            ppy_[s][n] = clamp_px(vp.px_y(cur));
            ++n;
            prev = cur;
        }
        pcount_[s] = static_cast<int16_t>(n);
    }
    eng.vars()['n'] = saved_n;
}

void GraphScreen::zoom_fit() {
    // ZoomFit ('F', task 4.7): function mode refits y over the current
    // x range (TI behavior); parametric/polar refit both axes to the
    // curve's extent. Sweeps world coordinates with the same sources
    // recompute() plots from.
    auto& st = graph::state();
    auto& eng = math::engine();
    const graph::Viewport vp = viewport();

    double xlo = 0.0;
    double xhi = 0.0;
    double ylo = 0.0;
    double yhi = 0.0;
    bool any = false;
    auto add = [&](double x, double y) {
        if (!any) {
            xlo = xhi = x;
            ylo = yhi = y;
            any = true;
            return;
        }
        xlo = x < xlo ? x : xlo;
        xhi = x > xhi ? x : xhi;
        ylo = y < ylo ? y : ylo;
        yhi = y > yhi ? y : yhi;
    };
    auto sweep = [&](graph::PointSource& src) {
        src.begin(vp);
        double x = 0.0;
        double y = 0.0;
        bool defined = false;
        int n = 0;
        while (n < kMaxCurvePoints && src.next(&x, &y, &defined)) {
            if (defined) {
                add(x, y);
            }
            ++n;
        }
    };

    switch (mode()) {
        case graph::Mode::kParametric: {
            const math::calc_t saved_t = eng.vars()['t'];
            for (int p = 0; p < graph::kParametricSlots; ++p) {
                if (!st.param.enabled[p] || st.param.x_expr[p][0] == 0 ||
                    st.param.y_expr[p][0] == 0) {
                    continue;
                }
                void* xh = eng.compile(st.param.x_expr[p], 't' - 'a');
                void* yh = eng.compile(st.param.y_expr[p], 't' - 'a');
                if (xh != nullptr && yh != nullptr) {
                    graph::ParametricSource src(eng, xh, yh, st.t_min, st.t_max, st.t_step);
                    sweep(src);
                }
                eng.free_compiled(xh);
                eng.free_compiled(yh);
            }
            eng.vars()['t'] = saved_t;
            break;
        }
        case graph::Mode::kPolar: {
            const math::calc_t saved_theta = eng.vars().vars[math::Variables::kTheta];
            for (int p = 0; p < graph::kPolarSlots; ++p) {
                if (!st.polar.enabled[p] || st.polar.expr[p][0] == 0) {
                    continue;
                }
                void* rh = eng.compile(st.polar.expr[p], math::Variables::kTheta);
                if (rh != nullptr) {
                    graph::PolarSource src(eng, rh, st.theta_min, st.theta_max, st.theta_step);
                    sweep(src);
                }
                eng.free_compiled(rh);
            }
            eng.vars().vars[math::Variables::kTheta] = saved_theta;
            break;
        }
        case graph::Mode::kSeq: {
            const math::calc_t saved_n = eng.vars()['n'];
            math::seqexpr::refresh();
            math::seqexpr::begin(graph::make_seq_def(st));
            for (int s = 0; s < graph::kSeqSlots; ++s) {
                if (!st.seq.enabled[s] || st.seq.expr[s][0] == 0 || !math::seqexpr::defined(s)) {
                    continue;
                }
                graph::SeqSource src(s, st.plot_start, st.n_max, st.plot_step);
                sweep(src);
            }
            eng.vars()['n'] = saved_n;
            break;
        }
        default: {
            const math::calc_t saved_x = eng.vars()['x'];
            auto& fns = y_functions();
            for (int fi = 0; fi < graph::kFunctionSlots; ++fi) {
                if (!fns.enabled[fi] || fns.expr[fi][0] == 0) {
                    continue;
                }
                void* compiled = eng.compile(fns.expr[fi], 'x' - 'a');
                if (compiled != nullptr) {
                    graph::FunctionSource src(eng, compiled);
                    // FunctionSource walks one x per viewport column, so
                    // kWidth samples; the point cap never binds here.
                    sweep(src);
                }
                eng.free_compiled(compiled);
            }
            eng.vars()['x'] = saved_x;
            break;
        }
    }

    if (!any) {
        return;  // Nothing plottable — leave the window alone.
    }

    auto& w = graph_window();
    // 5% margin; a flat curve (span 0) gets a half-unit each side.
    const double my = (yhi - ylo) > 0 ? (yhi - ylo) * 0.05 : 0.5;
    w.y_min = ylo - my;
    w.y_max = yhi + my;
    if (param_style()) {
        const double mx = (xhi - xlo) > 0 ? (xhi - xlo) * 0.05 : 0.5;
        w.x_min = xlo - mx;
        w.x_max = xhi + mx;
    }
    save_window();
}

void GraphScreen::draw_axes(gfx::Framebuffer& fb) const {
    using namespace platform::colors;
    const auto& w = graph_window();
    const graph::Viewport vp = viewport();

    // Grid lines at x_scl / y_scl (dark gray — must recede behind plots).
    // A scl that's tiny relative to the range would draw thousands of
    // merged lines (slow, illegible); coarsen the step to the largest
    // meaningful grid — the smallest multiple of scl spaced >= kMinGridPx.
    if (w.x_scl > 0) {
        const double per_px = w.x_scl * kWidth / (w.x_max - w.x_min);
        const double step = w.x_scl * thin_factor(per_px, kMinGridPx);
        const double start = std::ceil(w.x_min / step) * step;
        const auto nx = static_cast<int>(std::floor((w.x_max - start) / step));
        for (int i = 0; i <= nx; ++i) {
            fb.draw_vline(vp.px_x(start + i * step), top_, height_, kGridLine);
        }
    }
    if (w.y_scl > 0) {
        const double per_px = w.y_scl * height_ / (w.y_max - w.y_min);
        const double step = w.y_scl * thin_factor(per_px, kMinGridPx);
        const double start = std::ceil(w.y_min / step) * step;
        const auto ny = static_cast<int>(std::floor((w.y_max - start) / step));
        for (int i = 0; i <= ny; ++i) {
            fb.draw_hline(0, vp.px_y(start + i * step), kWidth, kGridLine);
        }
    }

    // Axes (solid white) when in view.
    if (w.y_min <= 0 && w.y_max >= 0) {
        fb.draw_hline(0, vp.px_y(0.0), kWidth, kWhite);
    }
    if (w.x_min <= 0 && w.x_max >= 0) {
        fb.draw_vline(vp.px_x(0.0), top_, height_, kWhite);
    }

    if (graph::state().axis_labels) {
        draw_axis_labels(fb);
    }
}

namespace {
// Tick-label formatting: 4 significant digits keeps irrational scl
// steps short (pi/2 -> "1.571", not full double precision — HW
// feedback 2026-07-18). KIV: symbolic pi / pi-fraction tick labels.
void tick_label(double v, char* buf, size_t buf_len) {
    std::snprintf(buf, buf_len, "%.4g", v);
}
}  // namespace

void GraphScreen::draw_axis_labels(gfx::Framebuffer& fb) const {
    // Numeric tick labels (task 4.4, small font). 'L' toggles them
    // (persisted since PCG3). Labels sit at scl grid lines but are
    // thinned so neighbors stay >= ~48px (x) / ~24px (y) apart; the
    // origin is skipped (the axis crossing says "0" already).
    using namespace platform::colors;
    const auto& w = graph_window();
    const graph::Viewport vp = viewport();
    const auto& font = gfx::small_font();
    const int bottom = top_ + height_;

    if (w.x_scl > 0) {
        const double per_px = w.x_scl * kWidth / (w.x_max - w.x_min);
        // Space labels >= ~48px, but as a multiple of the (possibly
        // coarsened) grid step so labels stay on grid lines.
        const int grid_every = thin_factor(per_px, kMinGridPx);
        const int every = grid_every * thin_factor(per_px * grid_every, 48.0);
        const double step = w.x_scl * every;
        const double start = std::ceil(w.x_min / step) * step;
        // Below the x-axis when it's in view (flipping above if that
        // would clip); along the bottom edge otherwise.
        int ly = bottom - font.height() - 2;
        if (w.y_min <= 0 && w.y_max >= 0) {
            const int axis_py = vp.px_y(0.0);
            ly = axis_py + font.height() + 4 <= bottom ? axis_py + 3 : axis_py - font.height() - 3;
        }
        const auto n = static_cast<int>(std::floor((w.x_max - start) / step));
        for (int i = 0; i <= n; ++i) {
            const double v = start + i * step;
            if (std::fabs(v) < w.x_scl / 2) {
                continue;
            }
            char buf[24];
            tick_label(v, buf, sizeof(buf));
            const int lx = vp.px_x(v) + 2;
            if (lx + font.text_width(buf) < kWidth) {
                font.draw_string(fb, lx, ly, buf, kGrayLine);
            }
        }
    }

    if (w.y_scl > 0) {
        const double per_px = w.y_scl * height_ / (w.y_max - w.y_min);
        const int grid_every = thin_factor(per_px, kMinGridPx);
        const int every = grid_every * thin_factor(per_px * grid_every, 24.0);
        const double step = w.y_scl * every;
        const double start = std::ceil(w.y_min / step) * step;
        const bool axis_in_view = w.x_min <= 0 && w.x_max >= 0;
        const int axis_px = axis_in_view ? vp.px_x(0.0) : 0;
        const auto n = static_cast<int>(std::floor((w.y_max - start) / step));
        for (int i = 0; i <= n; ++i) {
            const double v = start + i * step;
            if (std::fabs(v) < w.y_scl / 2) {
                continue;
            }
            char buf[24];
            tick_label(v, buf, sizeof(buf));
            // Right of the y-axis (left of it if that would clip);
            // left edge when the axis is off-screen.
            int lx = 2;
            if (axis_in_view) {
                lx = axis_px + font.text_width(buf) + 3 < kWidth
                         ? axis_px + 3
                         : axis_px - font.text_width(buf) - 3;
            }
            const int ly = vp.px_y(v) - font.height() / 2;
            if (ly >= top_ && ly + font.height() <= bottom) {
                font.draw_string(fb, lx, ly, buf, kGrayLine);
            }
        }
    }
}

void GraphScreen::draw_function(gfx::Framebuffer& fb, int fi) const {
    // Replay the column cache through the shared segment logic.
    const graph::PlotStyle style{function_color(fi), false};
    graph::Plotter plotter;
    plotter.begin();
    for (int px = 0; px < kWidth; ++px) {
        const int py = plot_y_[fi][px];
        plotter.point(fb, px, py, py != kOffscreen, style);
    }
}

// Replays the parameter-step cache (parametric and polar).
void GraphScreen::draw_param_curve(gfx::Framebuffer& fb, int p) const {
    const graph::PlotStyle style{function_color(p), false};
    graph::Plotter plotter;
    plotter.begin();
    for (int i = 0; i < pcount_[p]; ++i) {
        plotter.point(fb, ppx_[p][i], ppy_[p][i], ppy_[p][i] != kOffscreen, style);
    }
}

void GraphScreen::draw_trace(gfx::Framebuffer& fb) const {
    using namespace platform::colors;
    const auto& font = gfx::main_font();
    const graph::Viewport vp = viewport();

    int px = 0;
    int py = kOffscreen;
    char line[64];

    if (param_style()) {
        const int p = trace_.slot;
        if (pcount_[p] == 0) {
            return;
        }
        const int i = trace_.index;
        px = ppx_[p][i];
        py = ppy_[p][i];
        const auto& st = graph::state();
        const bool polar = mode() == graph::Mode::kPolar;
        const bool seq = mode() == graph::Mode::kSeq;
        // The last cached point may be the sample clamped to the range
        // end (sources close the curve when step doesn't divide range),
        // so cap the readout at max rather than extrapolating the grid.
        double param = 0;
        if (seq) {
            param = std::min(trace_value(), st.n_max);
        } else {
            const double grid = polar ? st.theta_min + i * st.theta_step : st.t_min + i * st.t_step;
            param = std::min(grid, polar ? st.theta_max : st.t_max);
        }
        char tb[24];
        char xb[24];
        char yb[24];
        math::format_number(param, tb, sizeof(tb));
        const double nan = std::numeric_limits<double>::quiet_NaN();
        math::format_number(py == kOffscreen ? nan : vp.data_x(px), xb, sizeof(xb));
        math::format_number(py == kOffscreen ? nan : vp.data_y(py), yb, sizeof(yb));
        if (seq) {
            std::snprintf(line, sizeof(line), "%c  n=%s x=%s y=%s", static_cast<char>('u' + p), tb,
                          xb, yb);
        } else {
            std::snprintf(line, sizeof(line), "%s%d  %s=%s x=%s y=%s", polar ? "r" : "P", p + 1,
                          polar ? kThetaLabel : "t", tb, xb, yb);
        }
    } else {
        px = trace_.index;
        py = plot_y_[trace_.slot][px];
        const double x = vp.data_x(px);
        char xb[24];
        char yb[24];
        math::format_number(x, xb, sizeof(xb));
        math::format_number(
            py == kOffscreen ? std::numeric_limits<double>::quiet_NaN() : vp.data_y(py), yb,
            sizeof(yb));
        std::snprintf(line, sizeof(line), "Y%d  x=%s  y=%s", trace_.slot + 1, xb, yb);
    }

    // Vertical trace line + cursor marker.
    fb.draw_vline(px, top_, height_, kCursor);
    if (py != kOffscreen) {
        fb.fill_rect(px - 2, py - 2, 5, 5, function_color(trace_.slot));
    }

    // Coordinate readout at the bottom of the viewport (D3: bottom).
    const int ty = top_ + height_ - font.height() - 2;
    fb.fill_rect(0, ty - 2, platform::kScreenW, font.height() + 4,
                 platform::Color::from_rgb(20, 20, 20));
    font.draw_string(fb, 4, ty, line, kWhite);
}

int GraphScreen::indep_px(double v) const {
    const auto& st = graph::state();
    switch (mode()) {
        case graph::Mode::kParametric:
        case graph::Mode::kPolar: {
            const bool polar = mode() == graph::Mode::kPolar;
            const double mn = polar ? st.theta_min : st.t_min;
            const double stp = polar ? st.theta_step : st.t_step;
            const int p = analysis_.slot;
            if (stp <= 0) {
                return -1;
            }
            const int i = static_cast<int>(std::lround((v - mn) / stp));
            if (i < 0 || i >= pcount_[p] || ppy_[p][i] == kOffscreen) {
                return -1;
            }
            return ppx_[p][i];
        }
        default:
            return viewport().px_x(v);
    }
}

void GraphScreen::cursor_point(int* px, int* py) const {
    if (param_style()) {
        const int p = trace_.slot;
        if (pcount_[p] == 0) {
            *px = 0;
            *py = kOffscreen;
            return;
        }
        *px = ppx_[p][trace_.index];
        *py = ppy_[p][trace_.index];
        return;
    }
    *px = trace_.index;
    *py = plot_y_[trace_.slot][*px];
}

void GraphScreen::draw_readout_strip(gfx::Framebuffer& fb, const char* line) const {
    const auto& font = gfx::main_font();
    const int ty = top_ + height_ - font.height() - 2;
    fb.fill_rect(0, ty - 2, platform::kScreenW, font.height() + 4,
                 platform::Color::from_rgb(20, 20, 20));
    font.draw_string(fb, 4, ty, line, platform::colors::kWhite);
}

void GraphScreen::draw_analysis(gfx::Framebuffer& fb) const {
    using namespace platform::colors;
    const graph::Viewport vp = viewport();
    const auto& s = analysis_;

    if (s.done) {
        const auto& r = s.result;
        if (r.ok) {
            if (s.op == graph::AnalysisOp::kIntegral && mode() == graph::Mode::kFunction) {
                // Shade between the curve and the x-axis over [a, b],
                // then repaint the curve on top. Reads only the column
                // cache — strip-safe.
                const platform::Color shade = platform::Color::from_rgb(0, 70, 110);
                int c0 = vp.px_x(s.vals[0]);
                int c1 = vp.px_x(s.vals[1]);
                if (c1 < c0) {
                    const int t = c0;
                    c0 = c1;
                    c1 = t;
                }
                c0 = c0 < 0 ? 0 : c0;
                c1 = c1 >= kWidth ? kWidth - 1 : c1;
                const int axis = clamp_px(vp.px_y(0.0));
                for (int c = c0; c <= c1; ++c) {
                    const int py = plot_y_[s.slot][c];
                    if (py == kOffscreen) {
                        continue;
                    }
                    const int y0 = py < axis ? py : axis;
                    const int len = (py < axis ? axis - py : py - axis) + 1;
                    fb.draw_vline(c, y0, len, shade);
                }
                draw_function(fb, s.slot);
            }
            if (s.op == graph::AnalysisOp::kDerivative && std::isfinite(r.aux)) {
                // Tangent line through the point (slope is dy/dx in
                // plot space, so this is mode-independent).
                const auto& w = graph_window();
                const double yl = r.y + r.aux * (w.x_min - r.x);
                const double yr = r.y + r.aux * (w.x_max - r.x);
                fb.draw_line(vp.px_x(w.x_min), clamp_px(vp.px_y(yl)), vp.px_x(w.x_max),
                             clamp_px(vp.px_y(yr)), kCursor);
            }
            fb.fill_rect(vp.px_x(r.x) - 2, clamp_px(vp.px_y(r.y)) - 2, 5, 5, kWhite);
        }
        draw_readout_strip(fb, analysis_line_);
        return;
    }

    // Input phase. Committed bound markers (top ticks) — intersect's
    // early steps pick curves, not bounds, so it has none to mark.
    if (s.op != graph::AnalysisOp::kIntersect) {
        for (int i = 0; i < s.step && i < 2; ++i) {
            const int px = indep_px(s.vals[i]);
            if (px >= 0) {
                fb.draw_vline(px, top_, 7, kGreen);
                fb.fill_rect(px - 1, top_ + 7, 3, 3, kGreen);
            }
        }
    }

    // Cursor riding the active curve (trace visuals).
    int cx = 0;
    int cy = kOffscreen;
    cursor_point(&cx, &cy);
    fb.draw_vline(cx, top_, height_, kCursor);
    if (cy != kOffscreen) {
        fb.fill_rect(cx - 2, cy - 2, 5, 5, function_color(trace_.slot));
    }

    // Prompt strip with the live cursor coordinates.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    char xb[24];
    char yb[24];
    math::format_number(cy == kOffscreen ? nan : vp.data_x(cx), xb, sizeof(xb));
    math::format_number(cy == kOffscreen ? nan : vp.data_y(cy), yb, sizeof(yb));
    const char* prefix = "Y";
    if (mode() == graph::Mode::kParametric) {
        prefix = "P";
    } else if (mode() == graph::Mode::kPolar) {
        prefix = "r";
    }
    char line[96];
    std::snprintf(line, sizeof(line), "%s%d  %s  x=%s y=%s", prefix, trace_.slot + 1,
                  s.prompt(mode()), xb, yb);
    draw_readout_strip(fb, line);
}

bool GraphScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    if (analysis_.active || analysis_.done) {
        return handle_analysis_key(ev);
    }
    // Global F-key scheme (2026-07-18 remap, TI-84-shaped):
    // F1 editor, F2 window, F3 mode, F4 trace, F5 table toggle,
    // Alt+F5 split; -/= zoom out/in.
    switch (ev.key) {
        case Key::kF1:
            push_mode_editor();
            return true;
        case Key::kF2:
            ui::screen_manager().push(&window_screen());
            return true;
        case Key::kF3:
            ui::screen_manager().push(&mode_screen());
            return true;
        case Key::kF4:  // Toggle trace
            if (trace_.active) {
                trace_.active = false;
            } else {
                start_trace();
            }
            return true;
        case Key::kF5:
            if (ev.alt_held) {  // Alt+F5: split graph|table
                ui::screen_manager().push(&split_screen());
            } else {
                ui::screen_manager().push(&table_screen());
            }
            return true;
        case Key::kF6:  // CALC menu (4B; F6 = Shift+F1 on the unit)
            if (mode() == graph::Mode::kSeq) {
                return true;  // CALC ops don't apply to sequences (4D v1)
            }
            ui::screen_manager().push(&calc_menu());
            return true;
        case Key::kMinus:
            zoom_out();
            dirty_ = true;
            return true;
        case Key::kEquals:
        case Key::kPlus:  // Shift+= — same zoom direction
            zoom_in();
            dirty_ = true;
            return true;
        case Key::kLeft:
            if (trace_.active) {
                trace_.step(-1, trace_max_index());
            }
            return true;
        case Key::kRight:
            if (trace_.active) {
                trace_.step(+1, trace_max_index());
            }
            return true;
        case Key::kUp:
        case Key::kDown:
            if (trace_.active) {  // Switch to the next active slot
                for (int step = 0; step < slot_count(); ++step) {
                    trace_.slot = (trace_.slot + 1) % slot_count();
                    if (slot_active(trace_.slot)) {
                        break;
                    }
                }
                trace_.clamp(trace_max_index());
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            // 'S' = ZStandard, 'T' = ZTrig presets, 'F' = ZoomFit.
            if (ev.ch == 's' || ev.ch == 'S') {
                zoom_standard();
                dirty_ = true;
                return true;
            }
            if (ev.ch == 't' || ev.ch == 'T') {
                zoom_trig();
                dirty_ = true;
                return true;
            }
            if (ev.ch == 'f' || ev.ch == 'F') {
                zoom_fit();
                dirty_ = true;
                return true;
            }
            if (ev.ch == 'z' || ev.ch == 'Z') {  // ZoomStat (3D, D27)
                // Only the data-space bounds below matter here; the pixel
                // cache this rebuilds is immediately superseded by the
                // dirty_ = true recompute() a few lines down, once the
                // window (and vp) actually changes.
                graph::recompute_stat_plots(viewport());
                double xlo = 0;
                double xhi = 0;
                double ylo = 0;
                double yhi = 0;
                if (graph::stat_plots_bounds(&xlo, &xhi, &ylo, &yhi)) {
                    auto& w = graph_window();
                    const double mx = (xhi - xlo) > 0 ? (xhi - xlo) * 0.05 : 0.5;
                    const double my = (yhi - ylo) > 0 ? (yhi - ylo) * 0.05 : 0.5;
                    w.x_min = xlo - mx;
                    w.x_max = xhi + mx;
                    w.y_min = ylo - my;
                    w.y_max = yhi + my;
                    save_window();
                    dirty_ = true;
                }
                return true;
            }
            if (ev.ch == 'l' || ev.ch == 'L') {
                graph::state().axis_labels = !graph::state().axis_labels;
                save_graph_state();
                dirty_ = true;  // Replot is cheap and forces the redraw
                return true;
            }
            return false;
    }
}

void GraphScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    if (dirty_) {
        recompute();
    }

    fb.clear(kBlack);

    // Confine plot drawing to the plot rows: curve segments heading
    // past the window otherwise land in the status-bar band (rows
    // 0-15), which nothing overdraws in full-screen mode (HW
    // 2026-07-18). Intersect with the enclosing pane (split view) and
    // restore it for the chrome below.
    const int pane_x0 = fb.pane_x0();
    const int pane_y0 = fb.pane_y0();
    const int pane_x1 = fb.pane_x1();
    const int pane_y1 = fb.pane_y1();
    fb.set_pane_clip(pane_x0, top_ > pane_y0 ? top_ : pane_y0, pane_x1,
                     top_ + height_ < pane_y1 ? top_ + height_ : pane_y1);

    draw_axes(fb);
    // Stat plots under the curves so regression overlays stay on top.
    graph::draw_stat_plots(fb, viewport());
    const bool seq_web = mode() == graph::Mode::kSeq && graph::state().seq_style == 1;
    if (seq_web) {
        // The y=x diagonal the cobweb bounces off (pure viewport math).
        const auto& w = graph_window();
        const graph::Viewport vp = viewport();
        const double lo = w.x_min > w.y_min ? w.x_min : w.y_min;
        const double hi = w.x_max < w.y_max ? w.x_max : w.y_max;
        if (lo < hi) {
            fb.draw_line(vp.px_x(lo), clamp_px(vp.px_y(lo)), vp.px_x(hi), clamp_px(vp.px_y(hi)),
                         kGrayLine);
        }
    }
    for (int s = 0; s < slot_count(); ++s) {
        if (!slot_active(s)) {
            continue;
        }
        if (seq_web) {
            draw_function(fb, s);  // The map curve (column cache)
            draw_param_curve(fb, s);
        } else if (param_style()) {
            draw_param_curve(fb, s);
        } else {
            draw_function(fb, s);
        }
    }
    if (trace_.active) {
        draw_trace(fb);
    }
    if (analysis_.active || analysis_.done) {
        draw_analysis(fb);
    }

    // Hint uses the pre-compile enabled state (Phase 1 behavior): a
    // slot with a syntax error suppresses it too.
    bool any = false;
    switch (mode()) {
        case graph::Mode::kParametric: {
            const auto& pf = graph::state().param;
            for (int p = 0; p < graph::kParametricSlots; ++p) {
                any = any || (pf.enabled[p] && pf.x_expr[p][0] != 0 && pf.y_expr[p][0] != 0);
            }
            break;
        }
        case graph::Mode::kPolar: {
            const auto& pf = graph::state().polar;
            for (int p = 0; p < graph::kPolarSlots; ++p) {
                any = any || (pf.enabled[p] && pf.expr[p][0] != 0);
            }
            break;
        }
        case graph::Mode::kSeq: {
            const auto& sf = graph::state().seq;
            for (int s = 0; s < graph::kSeqSlots; ++s) {
                any = any || (sf.enabled[s] && sf.expr[s][0] != 0);
            }
            break;
        }
        default:
            any = y_functions().any_enabled();
            break;
    }
    if (!any) {
        const char* hint = param_style() ? "No curves. Press F1 for the editor."
                                         : "No functions. Press F1 for Y=.";
        font.draw_string(fb, 40, top_ + height_ / 2, hint, kGrayLine);
    }

    fb.set_pane_clip(pane_x0, pane_y0, pane_x1, pane_y1);

    // Chrome only in the full-screen layout — inside a split pane the
    // top rows are plot area and the split draws its own bar.
    if (top_ >= ui::kStatusBarH) {
        const char* title = "GRAPH FUNC";
        if (mode() == graph::Mode::kParametric) {
            title = "GRAPH PARAM";
        } else if (mode() == graph::Mode::kPolar) {
            title = "GRAPH POLAR";
        } else if (mode() == graph::Mode::kSeq) {
            title = graph::state().seq_style == 1 ? "GRAPH SEQ WEB" : "GRAPH SEQ";
        }
        ui::draw_status_bar(fb, title);
    }

    const char* editor_key = "Y=";
    if (mode() == graph::Mode::kParametric) {
        editor_key = "PAR";
    } else if (mode() == graph::Mode::kPolar) {
        editor_key = "R=";
    } else if (mode() == graph::Mode::kSeq) {
        editor_key = "u=";
    }
    const char* const keys[6] = {editor_key, "WIN", "MODE", "TRC", "TBL", "CALC"};
    ui::draw_softkeys(fb, keys);
}

GraphScreen& graph_screen() {
    static GraphScreen instance;
    return instance;
}

}  // namespace apps
