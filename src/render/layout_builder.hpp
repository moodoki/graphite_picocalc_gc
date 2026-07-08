#pragma once

#include "render/layout_node.hpp"

namespace render {

// Parse a calculator expression string into a layout tree sized with
// `m`. Nodes are pool-allocated (call render::pool_reset() before, or
// let this do it). Returns nullptr only if the input is empty; on pool
// exhaustion or parse trouble it falls back to a plain TextNode of the
// remaining text, so the result is always renderable.
//
// Supported 2D constructs (Phase 1, task 3.5):
//   a/b        -> FractionNode when both operands are "simple" (D2)
//   a^b        -> SuperscriptNode (right-associative)
//   ( expr )   -> ParenNode (auto-scaling)
//   everything else -> Text within an HBox
LayoutNode* build_layout(const char* expr, const Metrics& m);

}  // namespace render
