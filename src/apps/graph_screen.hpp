#pragma once

#include <cstdint>

#include "ui/screen.hpp"
#include "graph/viewport.hpp"

namespace apps {

// Function graphing viewport (tasks 4.2-4.7). Plots all enabled
// Y-functions with axes, grid, discontinuity handling, trace, and zoom.
//
// Plot points are precomputed into a column cache when the window or
// functions change, so per-strip rendering is cheap.
class GraphScreen : public ui::Screen {
public:
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

    // Cached screen-y per function per column (kOffscreen = no point).
    int16_t plot_y_[7][kWidth] = {};
    bool active_[7] = {};  // Which slots are enabled + non-empty
    bool dirty_ = true;

    bool trace_ = false;
    int trace_func_ = 0;
    int trace_px_ = kWidth / 2;

    // Last replot time in microseconds (task 5.6 profiling hook).
    uint32_t last_recompute_us_ = 0;

    void recompute();
    graph::Viewport viewport() const;

    void draw_axes(gfx::Framebuffer& fb) const;
    void draw_function(gfx::Framebuffer& fb, int fi) const;
    void draw_trace(gfx::Framebuffer& fb) const;
};

GraphScreen& graph_screen();

}  // namespace apps
