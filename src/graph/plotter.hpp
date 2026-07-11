#pragma once

#include "platform/display.hpp"
#include "gfx/framebuffer.hpp"
#include "graph/viewport.hpp"

namespace graph {

struct PlotStyle {
    platform::Color color{};
    bool thick = false;  // 2px line for emphasis (trace target)
};

// A source of points to plot. Each mode implements this by producing
// (x_data, y_data) pairs as its parameter advances.
class PointSource {
public:
    virtual ~PointSource() = default;

    // Reset iteration to the start of the parameter range.
    virtual void begin(const Viewport& vp) = 0;

    // Produce the next data-space point. Returns false when the
    // parameter range is exhausted. Sets *defined = false when the
    // function is undefined at this parameter (e.g. 1/0), which
    // triggers a pen-up (no connecting segment).
    virtual bool next(double* x_data, double* y_data, bool* defined) = 0;
};

// The plotting loop: transforms points to pixel space and draws
// connected segments with discontinuity detection (Phase 1 heuristic).
class Plotter {
public:
    // Consume a PointSource (mode-agnostic path).
    void plot(gfx::Framebuffer& fb, const Viewport& vp, PointSource& source,
              const PlotStyle& style);

    // Pixel-space incremental path. plot() feeds it internally; callers
    // that replay cached pixel columns (function mode's column cache)
    // use it directly so both paths share the segment logic.
    void begin();
    void point(gfx::Framebuffer& fb, int px, int py, bool defined, const PlotStyle& style);

    // Far-offscreen pixel ys are clamped to +/- this so line joins stay
    // sane (Phase 1 behavior).
    static constexpr int kClampPy = 1000;

private:
    // Pen-up if adjacent points jump more than this many pixel rows
    // (Phase 1: half the 280px graph height).
    static constexpr int kDiscontinuityThreshold = 140;

    int prev_px_ = 0;
    int prev_py_ = 0;
    bool pen_down_ = false;
};

}  // namespace graph
