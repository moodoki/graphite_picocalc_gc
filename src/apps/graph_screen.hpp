#pragma once

#include <cstdint>

#include "ui/screen.hpp"
#include "graph/graph_state.hpp"
#include "graph/trace.hpp"
#include "graph/viewport.hpp"

namespace apps {

// Graphing viewport (tasks 4.2-4.7; parametric since 2.7). Plots the
// active mode's functions with axes, grid, discontinuity handling,
// trace, and zoom.
//
// Plot points are precomputed into pixel caches when the window or
// functions change, so per-strip rendering is cheap: function mode
// caches screen-y per pixel column; parametric caches (px, py) per
// parameter step.
class GraphScreen : public ui::Screen {
public:
    GraphScreen() { trace_.index = kWidth / 2; }

    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

    // Force a replot (e.g. after window edits from WindowScreen).
    void invalidate() { dirty_ = true; }

private:
    // Graph viewport geometry (content between status bar and softkeys).
    static constexpr int kTop = 16;
    static constexpr int kHeight = 280;
    static constexpr int kWidth = 320;

    static constexpr int16_t kOffscreen = INT16_MIN;

    // Parametric point budget per pair (~8 KB total cache). The default
    // Tstep (2pi/63) uses 64; very small Tstep values get truncated.
    static constexpr int kMaxCurvePoints = 340;

    // Function mode: cached screen-y per column (kOffscreen = no point).
    int16_t plot_y_[graph::kFunctionSlots][kWidth] = {};
    bool active_[graph::kFunctionSlots] = {};

    // Parametric mode: cached pixel points per parameter step
    // (ppy == kOffscreen = undefined at that step, pen up).
    int16_t ppx_[graph::kParametricSlots][kMaxCurvePoints] = {};
    int16_t ppy_[graph::kParametricSlots][kMaxCurvePoints] = {};
    int16_t pcount_[graph::kParametricSlots] = {};
    bool pactive_[graph::kParametricSlots] = {};

    bool dirty_ = true;
    graph::TraceCursor trace_;

    // Last replot time in microseconds (task 5.6 profiling hook).
    uint32_t last_recompute_us_ = 0;

    void recompute();
    void recompute_function(const graph::Viewport& vp);
    void recompute_parametric(const graph::Viewport& vp);
    void recompute_polar(const graph::Viewport& vp);
    graph::Viewport viewport() const;

    // Active-mode slot helpers for trace navigation.
    int slot_count() const;
    bool slot_active(int s) const;
    int trace_max_index() const;

    void draw_axes(gfx::Framebuffer& fb) const;
    void draw_function(gfx::Framebuffer& fb, int fi) const;
    void draw_param_curve(gfx::Framebuffer& fb, int p) const;
    void draw_trace(gfx::Framebuffer& fb) const;
};

GraphScreen& graph_screen();

}  // namespace apps
