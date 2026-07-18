#pragma once

namespace apps {

// Shared navigation actions for the global F-key scheme (2026-07-18
// remap): every screen binds F1 to the mode-appropriate editor and F4
// to trace, so the dispatch lives here instead of being copied per
// screen.

// F1: push the editor matching the current graph mode (Y=/PAR/POL).
void push_mode_editor();

// F4 (from outside the graph screen): switch to the graph with trace
// active, TI-style.
void goto_graph_trace();

}  // namespace apps
