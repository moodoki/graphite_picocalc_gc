#pragma once

#include <cstdint>

#include "ui/screen.hpp"
#include "graph/analysis_cursor.hpp"
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

    // Pane geometry (task 2.19): render into rows [top, top+height)
    // instead of the full-screen default. Width stays 320 — the
    // horizontal split (D16) keeps the column caches and trace
    // x-mapping identical to full screen. Forces a replot (cached py
    // values depend on the height).
    void set_pane(int top, int height) {
        top_ = top;
        height_ = height;
        dirty_ = true;
    }
    void reset_pane() { set_pane(kTopDefault, kHeightDefault); }

    // Activate trace on the first active slot (F4 TRACE binding; also
    // used by other screens jumping here in trace mode, TI-style).
    void start_trace();

    // Start an interactive CALC-menu analysis (Phase 4B). Called by
    // CalcMenuScreen after it pops back; no-op when nothing plots.
    void begin_analysis(graph::AnalysisOp op);

    // Trace sync with the table pane (task 2.20, nearest-row).
    bool trace_active() const { return trace_.active; }
    double trace_value() const;          // Current independent value
    void sync_trace_to_value(double v);  // Move the cursor near v

private:
    // Full-screen geometry (content between status bar and softkeys).
    static constexpr int kTopDefault = 16;
    static constexpr int kHeightDefault = 280;
    static constexpr int kWidth = 320;

    int top_ = kTopDefault;
    int height_ = kHeightDefault;

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

    // Exact y under the trace cursor in function mode (issue #40). The
    // readout used to come from data_y(plot_y_[slot][px]), a round trip
    // through a pixel row: at a zoom where one row spans a hundredth of
    // a unit, every column near the peak of a sine reported "1". Worse,
    // plot_y_ is clamped (clamp_px), so a defined point outside the
    // window read back as the clamp boundary rather than its value.
    // Refreshed outside render() by refresh_trace_readout(); false means
    // "undefined here", which is what prints blank.
    double trace_y_ = 0.0;
    bool trace_y_ok_ = false;

    // ZBox (4D.9): free 2-D pixel cursor, two ENTER-committed corners
    // become the new window. Pure state — render only draws it.
    struct ZBoxSession {
        bool active = false;
        int step = 0;  // 0 = picking corner 1, 1 = picking corner 2
        int x0 = 0;
        int y0 = 0;
        int cx = 0;
        int cy = 0;
    } zbox_;

    // Shade(lower, upper) between two curves (4D.11, function mode).
    // 'H' starts the two-pick flow (UP/DOWN choose, ENTER commit) and
    // toggles an existing shade off. Not persisted (TI ClrDraw-style).
    int shade_lo_ = -1;
    int shade_hi_ = -1;
    int shade_pick_ = 0;  // 0 idle, 1 picking lower, 2 picking upper

    // Interactive CALC session (4B). The cursor rides trace_ (slot +
    // index) while inputs are collected; results are computed on the
    // final ENTER and only drawn from cached state (strip-safe).
    graph::AnalysisSession analysis_;
    char analysis_line_[96] = {};

    // Last replot time in microseconds (task 5.6 profiling hook).
    uint32_t last_recompute_us_ = 0;

    void recompute();
    void zoom_fit();
    void recompute_function(const graph::Viewport& vp);
    void recompute_parametric(const graph::Viewport& vp);
    void recompute_polar(const graph::Viewport& vp);
    void recompute_seq(const graph::Viewport& vp);
    graph::Viewport viewport() const;

    // Active-mode slot helpers for trace navigation.
    int slot_count() const;
    bool slot_active(int s) const;
    int trace_max_index() const;

    // Every key this screen handles, wrapped by on_key so the trace
    // readout is refreshed exactly once per event however the cursor
    // moved — there are eight paths that move it and a ninth would
    // otherwise silently report a stale y (issue #40).
    bool handle_key(const platform::KeyEvent& ev);

    // Recompute trace_y_ from the EVALUATOR, not from the pixel cache.
    // Must never be called from render(): it parses an expression, and
    // compiling under render is what D47 forbids. Cheap and a no-op
    // unless a function-mode trace is active.
    void refresh_trace_readout();

    void draw_axes(gfx::Framebuffer& fb) const;
    void draw_axis_labels(gfx::Framebuffer& fb) const;
    void draw_function(gfx::Framebuffer& fb, int fi, bool thick = false) const;
    void draw_param_curve(gfx::Framebuffer& fb, int p) const;
    void draw_trace(gfx::Framebuffer& fb) const;

    // Zoom/shading (4D.9-11).
    bool handle_zbox_key(const platform::KeyEvent& ev);
    void draw_zbox(gfx::Framebuffer& fb) const;
    bool handle_shade_pick_key(const platform::KeyEvent& ev);
    void draw_shades(gfx::Framebuffer& fb) const;
    void start_shade_pick();

    // CALC-session helpers (4B).
    bool handle_analysis_key(const platform::KeyEvent& ev);
    void finish_analysis();
    int indep_px(double v) const;  // Pixel column for an indep value (-1 = none)
    void cursor_point(int* px, int* py) const;
    void draw_analysis(gfx::Framebuffer& fb) const;
    void draw_readout_strip(gfx::Framebuffer& fb, const char* line) const;
};

GraphScreen& graph_screen();

}  // namespace apps
