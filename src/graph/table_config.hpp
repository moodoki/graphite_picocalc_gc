#pragma once

namespace graph {

// Table view configuration (spec §7.1). Lives in graph/ (not apps/) so
// GraphState can hold it without the graph layer referencing upward
// into apps (§14 layering).
struct TableConfig {
    double start = 0.0;     // First value of the independent var
    double step = 1.0;      // Increment (auto mode)
    bool ask_mode = false;  // true = user enters each x; false = auto
};

}  // namespace graph
