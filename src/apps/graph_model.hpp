#pragma once

#include "platform/display.hpp"
#include "graph/graph_state.hpp"

namespace apps {

constexpr int kNumFuncs = graph::kFunctionSlots;  // Y1..Y7

// The structs moved to graph/ with task 2.2 (GraphState is the single
// source of truth); these aliases keep Phase 1 call sites unchanged.
using GraphWindow = graph::GraphWindow;
using YFunctions = graph::YFunctions;

// Live references into graph::state().
GraphWindow& graph_window();
YFunctions& y_functions();

// Distinct plot color per function slot (spec 7.3 palette).
platform::Color function_color(int index);
// Darkened companion for shaded regions (4D.11): fnInt fills,
// inequality shading, Shade(lower,upper) — recedes behind the curve.
platform::Color function_color_dim(int index);

// SD persistence (task 2.23): unified /picocalc/graphstate.dat, with
// one-time migration from Phase 1's yfuncs.txt/window.dat on load.
// save_functions/save_window are legacy names for the same full save.
void load_graph_state();
void save_graph_state();
void save_functions();
void save_window();

// Zoom presets / operations on the shared window.
void zoom_standard();
void zoom_trig();
void zoom_in();
void zoom_out();
// 4D.9-10: ZDecimal (0.1 units per pixel, centered on the origin) and
// ZSquare (y range adjusted so a unit spans the same pixel distance on
// both axes). Both take the plot area's pixel size (pane-aware).
void zoom_decimal(int width, int height);
void zoom_square(int width, int height);

}  // namespace apps
