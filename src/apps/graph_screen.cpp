#include "apps/graph_screen.hpp"

#include <cmath>
#include <cstdio>

#include "pico/time.h"

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "apps/graph_model.hpp"
#include "apps/y_editor.hpp"

namespace apps {

namespace {
constexpr double kDiscontinuityFrac = 0.5;  // Skip lines jumping >50% of h
}  // namespace

void GraphScreen::on_activate() {
    dirty_ = true;
}

int GraphScreen::value_to_py(double y) const {
    const auto& w = graph_window();
    const double frac = (w.y_max - y) / (w.y_max - w.y_min);
    return kTop + static_cast<int>(frac * kHeight);
}

void GraphScreen::recompute() {
    const auto& w = graph_window();
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
        for (int px = 0; px < kWidth; ++px) {
            const double x = w.x_min + px * (w.x_max - w.x_min) / (kWidth - 1);
            const double y = eng.eval_compiled(compiled, x);
            if (std::isfinite(y)) {
                const int py = value_to_py(y);
                // Clamp far-offscreen values so line joins stay sane, but
                // mark truly-NaN as offscreen.
                plot_y_[fi][px] =
                    static_cast<int16_t>(py < -1000 ? -1000 : (py > 1000 ? 1000 : py));
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

    // Grid lines at x_scl / y_scl (light gray).
    if (w.x_scl > 0) {
        const double start = std::ceil(w.x_min / w.x_scl) * w.x_scl;
        for (double gx = start; gx <= w.x_max; gx += w.x_scl) {
            const int px = static_cast<int>((gx - w.x_min) / (w.x_max - w.x_min) * (kWidth - 1));
            fb.draw_vline(px, kTop, kHeight, kGrayLine);
        }
    }
    if (w.y_scl > 0) {
        const double start = std::ceil(w.y_min / w.y_scl) * w.y_scl;
        for (double gy = start; gy <= w.y_max; gy += w.y_scl) {
            const int py = value_to_py(gy);
            fb.draw_hline(0, py, kWidth, kGrayLine);
        }
    }

    // Axes (solid white) when in view.
    if (w.y_min <= 0 && w.y_max >= 0) {
        const int py = value_to_py(0.0);
        fb.draw_hline(0, py, kWidth, kWhite);
    }
    if (w.x_min <= 0 && w.x_max >= 0) {
        const int px = static_cast<int>((0.0 - w.x_min) / (w.x_max - w.x_min) * (kWidth - 1));
        fb.draw_vline(px, kTop, kHeight, kWhite);
    }
}

void GraphScreen::draw_function(gfx::Framebuffer& fb, int fi) const {
    const platform::Color color = function_color(fi);
    const int limit = static_cast<int>(kHeight * kDiscontinuityFrac);
    int prev_py = kOffscreen;
    for (int px = 0; px < kWidth; ++px) {
        const int py = plot_y_[fi][px];
        if (py == kOffscreen) {
            prev_py = kOffscreen;
            continue;
        }
        if (prev_py != kOffscreen && std::abs(py - prev_py) < limit) {
            fb.draw_line(px - 1, prev_py, px, py, color);
        } else {
            fb.set_pixel(px, py, color);
        }
        prev_py = py;
    }
}

void GraphScreen::draw_trace(gfx::Framebuffer& fb) const {
    using namespace platform::colors;
    const auto& font = gfx::main_font();
    const auto& w = graph_window();

    const int px = trace_px_;
    const int py = plot_y_[trace_func_][px];
    const double x = w.x_min + px * (w.x_max - w.x_min) / (kWidth - 1);

    // Vertical trace line + cursor marker.
    fb.draw_vline(px, kTop, kHeight, kCursor);
    if (py != kOffscreen) {
        fb.fill_rect(px - 2, py - 2, 5, 5, function_color(trace_func_));
    }

    // Coordinate readout at the bottom of the viewport (D3: bottom).
    const double y =
        w.y_min + (w.y_max - w.y_min) * (kTop + kHeight - py) / static_cast<double>(kHeight);
    char xb[24];
    char yb[24];
    math::format_number(x, xb, sizeof(xb));
    math::format_number(py == kOffscreen ? (0.0 / 0.0) : y, yb, sizeof(yb));
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
        case Key::kF6:
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

    const char* keys[6] = {"TRC", "Z+", "Z-", "", "Y=", "Y="};
    ui::draw_softkeys(fb, keys);
}

GraphScreen& graph_screen() {
    static GraphScreen instance;
    return instance;
}

}  // namespace apps
