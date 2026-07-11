#include "apps/graph_screen.hpp"

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
#include "apps/y_editor.hpp"
#include "graph/function_source.hpp"
#include "graph/plotter.hpp"

namespace apps {

void GraphScreen::on_activate() {
    dirty_ = true;
}

graph::Viewport GraphScreen::viewport() const {
    const auto& w = graph_window();
    graph::Viewport vp;
    vp.x_min = w.x_min;
    vp.x_max = w.x_max;
    vp.y_min = w.y_min;
    vp.y_max = w.y_max;
    vp.left = 0;
    vp.top = kTop;
    vp.width = kWidth;
    vp.height = kHeight;
    return vp;
}

void GraphScreen::recompute() {
    const graph::Viewport vp = viewport();
    auto& fns = y_functions();
    auto& eng = math::engine();

    // Graphing sweeps the shared X variable; preserve any value the user
    // stored in X on the home screen.
    const math::calc_t saved_x = eng.vars()['x'];
    const uint64_t t0 = time_us_64();

    for (int fi = 0; fi < kNumFuncs; ++fi) {
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
            if (defined) {
                // Clamp far-offscreen values so line joins stay sane, but
                // mark truly-NaN as offscreen.
                const int py = vp.px_y(y);
                constexpr int kClamp = graph::Plotter::kClampPy;
                plot_y_[fi][px] =
                    static_cast<int16_t>(py < -kClamp ? -kClamp : (py > kClamp ? kClamp : py));
            } else {
                plot_y_[fi][px] = kOffscreen;
            }
        }
        eng.free_compiled(compiled);
    }
    eng.vars()['x'] = saved_x;
    last_recompute_us_ = static_cast<uint32_t>(time_us_64() - t0);
    printf("graph recompute: %lu us\n", static_cast<unsigned long>(last_recompute_us_));
    dirty_ = false;
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
            fb.draw_vline(vp.px_x(start + i * w.x_scl), kTop, kHeight, kGridLine);
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
        fb.draw_vline(vp.px_x(0.0), kTop, kHeight, kWhite);
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

void GraphScreen::draw_trace(gfx::Framebuffer& fb) const {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    const graph::Viewport vp = viewport();
    const int px = trace_px_;
    const int py = plot_y_[trace_func_][px];
    const double x = vp.data_x(px);

    // Vertical trace line + cursor marker.
    fb.draw_vline(px, kTop, kHeight, kCursor);
    if (py != kOffscreen) {
        fb.fill_rect(px - 2, py - 2, 5, 5, function_color(trace_func_));
    }

    // Coordinate readout at the bottom of the viewport (D3: bottom).
    const double y = vp.data_y(py);
    char xb[24];
    char yb[24];
    math::format_number(x, xb, sizeof(xb));
    math::format_number(py == kOffscreen ? std::numeric_limits<double>::quiet_NaN() : y, yb,
                        sizeof(yb));
    char line[56];
    std::snprintf(line, sizeof(line), "Y%d  x=%s  y=%s", trace_func_ + 1, xb, yb);
    const int ty = kTop + kHeight - font.height() - 2;
    fb.fill_rect(0, ty - 2, platform::kScreenW, font.height() + 4,
                 platform::Color::from_rgb(20, 20, 20));
    font.draw_string(fb, 4, ty, line, kWhite);
}

bool GraphScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kF1:  // Toggle trace
            trace_ = !trace_;
            if (trace_) {
                // Start on the first active function.
                for (int i = 0; i < kNumFuncs; ++i) {
                    if (active_[i]) {
                        trace_func_ = i;
                        break;
                    }
                }
            }
            return true;
        case Key::kF2:
            zoom_in();
            dirty_ = true;
            return true;
        case Key::kF3:
            zoom_out();
            dirty_ = true;
            return true;
        case Key::kF5:
            ui::screen_manager().push(&y_editor_screen());
            return true;
        case Key::kLeft:
            if (trace_ && trace_px_ > 0) {
                --trace_px_;
            }
            return true;
        case Key::kRight:
            if (trace_ && trace_px_ < kWidth - 1) {
                ++trace_px_;
            }
            return true;
        case Key::kUp:
        case Key::kDown:
            if (trace_) {  // Switch to the next active function
                for (int step = 0; step < kNumFuncs; ++step) {
                    trace_func_ = (trace_func_ + 1) % kNumFuncs;
                    if (active_[trace_func_]) {
                        break;
                    }
                }
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
    draw_axes(fb);
    for (int fi = 0; fi < kNumFuncs; ++fi) {
        if (active_[fi]) {
            draw_function(fb, fi);
        }
    }
    if (trace_) {
        draw_trace(fb);
    }

    if (!y_functions().any_enabled()) {
        font.draw_string(fb, 40, kTop + kHeight / 2, "No functions. Press F5 for Y=.", kGrayLine);
    }

    const char* const keys[6] = {"TRC", "Z+", "Z-", "", "Y=", "DIAG"};
    ui::draw_softkeys(fb, keys);
}

GraphScreen& graph_screen() {
    static GraphScreen instance;
    return instance;
}

}  // namespace apps
