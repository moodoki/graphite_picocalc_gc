#pragma once

#include <cstddef>

#include "graph/graph_state.hpp"

namespace apps {

// Table columns for the active graph mode (tasks 2.13/2.16/2.17).
// Function mode:   x  | Y1 ..  (one column per enabled slot)
// Parametric mode: T  | X1T Y1T ..  (two columns per enabled pair)
// Polar mode:      th | r1 ..
//
// Lives apart from TableScreen so it stays free of gfx/platform
// dependencies and host-testable (spec §7.3 puts it in apps/).
constexpr int kMaxTableColumns = 2 * graph::kParametricSlots;  // parametric worst case

int table_column_count(const graph::GraphState& state);
void table_column_label(const graph::GraphState& state, int col, char* buf, size_t buf_len);
const char* table_independent_label(const graph::GraphState& state);

// Evaluate all enabled functions at a single independent value.
// Fills results[] with one value per column (order matches
// table_column_label). Returns the number of columns filled;
// unparseable expressions yield NaN for their column(s).
int evaluate_table_row(const graph::GraphState& state, double independent_value, double* results,
                       int max_results);

}  // namespace apps
