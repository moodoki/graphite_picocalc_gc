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

// SD persistence (/picocalc/yfuncs.txt, /picocalc/window.dat).
void load_graph_state();
void save_functions();
void save_window();

// Zoom presets / operations on the shared window.
void zoom_standard();
void zoom_trig();
void zoom_in();
void zoom_out();

}  // namespace apps
