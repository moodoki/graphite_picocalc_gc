#pragma once

#include "gfx/framebuffer.hpp"
#include "graph/graph_state.hpp"
#include "graph/viewport.hpp"

namespace graph {

// Statistical plots (tasks 3D.9-3D.13, D27): renderers for the three
// StatPlotConfig slots in GraphState, drawing l1..l6 data into the
// graph viewport alongside function plots.
//
// Split per the strip-safety rule (§8): recompute_stat_plots() caches
// everything expensive (histogram bins, box five-number summaries,
// normprob sorted copies + quantiles) and runs from GraphScreen's
// recompute path; draw_stat_plots() is pure drawing from those caches
// plus chunked list streaming.

bool any_stat_plot_enabled();

// Rebuild caches for the enabled plots. Call whenever the graph
// screen recomputes (window change, activation) — list edits are
// picked up then. `vp` is needed to build the pixel-space point cache
// (perf fix, 2026-07-22) — safe to pass a soon-to-be-stale viewport
// (e.g. before a window change takes effect) since the caller's own
// dirty flag guarantees a fresh recompute with the new vp follows.
void recompute_stat_plots(const Viewport& vp);

// Draw every enabled (and valid) plot.
void draw_stat_plots(gfx::Framebuffer& fb, const Viewport& vp);

// Union data bounds of the enabled plots (ZoomStat). False when no
// enabled plot has usable data.
bool stat_plots_bounds(double* x_lo, double* x_hi, double* y_lo, double* y_hi);

}  // namespace graph
