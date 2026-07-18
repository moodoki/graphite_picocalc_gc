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

namespace apps {

namespace {
// Clamp a pixel coordinate for the int16 caches (far-offscreen values
// keep line joins sane; truly undefined points use kOffscreen).
int16_t clamp_px(int v) {
    constexpr int kClamp = graph::Plotter::kClampPy;
    return static_cast<int16_t>(v < -kClamp ? -kClamp : (v > kClamp ? kClamp : v));
}

graph::Mode mode() {
    return graph::state().mode;
}

// Parametric and polar both sweep a parameter into the shared
// point-per-step cache; function mode uses the column cache.
bool param_style() {
    return mode() != graph::Mode::kFunction;
}
}  // namespace

void GraphScreen::on_activate() {
    dirty_ = true;
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
        default:
            recompute_function(vp);
            break;
    }

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
        void* compiled = eng.compile(fns.expr[fi]);
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
        void* xh = eng.compile(st.param.x_expr[p]);
        void* yh = eng.compile(st.param.y_expr[p]);
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
        void* rh = eng.compile(st.polar.expr[p]);
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

void GraphScreen::draw_axes(gfx::Framebuffer& fb) const {
    using namespace platform::colors;
    const auto& w = graph_window();
    const graph::Viewport vp = viewport();

    // Grid lines at x_scl / y_scl (dark gray — must recede behind plots).
    if (w.x_scl > 0) {
        const double start = std::ceil(w.x_min / w.x_scl) * w.x_scl;
        const auto nx = static_cast<int>(std::floor((w.x_max - start) / w.x_scl));
        for (int i = 0; i <= nx; ++i) {
            fb.draw_vline(vp.px_x(start + i * w.x_scl), top_, height_, kGridLine);
        }
    }
    if (w.y_scl > 0) {
        const double start = std::ceil(w.y_min / w.y_scl) * w.y_scl;
        const auto ny = static_cast<int>(std::floor((w.y_max - start) / w.y_scl));
        for (int i = 0; i <= ny; ++i) {
            fb.draw_hline(0, vp.px_y(start + i * w.y_scl), kWidth, kGridLine);
        }
    }

    // Axes (solid white) when in view.
    if (w.y_min <= 0 && w.y_max >= 0) {
        fb.draw_hline(0, vp.px_y(0.0), kWidth, kWhite);
    }
    if (w.x_min <= 0 && w.x_max >= 0) {
        fb.draw_vline(vp.px_x(0.0), top_, height_, kWhite);
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
        // The last cached point may be the sample clamped to the range
        // end (sources close the curve when step doesn't divide range),
        // so cap the readout at max rather than extrapolating the grid.
        const double grid = polar ? st.theta_min + i * st.theta_step : st.t_min + i * st.t_step;
        const double param = std::min(grid, polar ? st.theta_max : st.t_max);
        char tb[24];
        char xb[24];
        char yb[24];
        math::format_number(param, tb, sizeof(tb));
        const double nan = std::numeric_limits<double>::quiet_NaN();
        math::format_number(py == kOffscreen ? nan : vp.data_x(px), xb, sizeof(xb));
        math::format_number(py == kOffscreen ? nan : vp.data_y(py), yb, sizeof(yb));
        std::snprintf(line, sizeof(line), "%s%d  %s=%s x=%s y=%s", polar ? "r" : "P", p + 1,
                      polar ? "th" : "t", tb, xb, yb);
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

bool GraphScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
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
            // 'S' = ZStandard, 'T' = ZTrig presets.
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
    for (int s = 0; s < slot_count(); ++s) {
        if (!slot_active(s)) {
            continue;
        }
        if (param_style()) {
            draw_param_curve(fb, s);
        } else {
            draw_function(fb, s);
        }
    }
    if (trace_.active) {
        draw_trace(fb);
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
        }
        ui::draw_status_bar(fb, title);
    }

    const char* editor_key = "Y=";
    if (mode() == graph::Mode::kParametric) {
        editor_key = "PAR";
    } else if (mode() == graph::Mode::kPolar) {
        editor_key = "R=";
    }
    const char* const keys[6] = {editor_key, "WIN", "MODE", "TRC", "TBL", ""};
    ui::draw_softkeys(fb, keys);
}

GraphScreen& graph_screen() {
    static GraphScreen instance;
    return instance;
}

}  // namespace apps
