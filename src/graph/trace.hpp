#pragma once

namespace graph {

// Mode-aware trace cursor (task 2.7; generalizes Phase 1's pixel-column
// trace). The cursor walks indexed positions along a cached curve —
// pixel columns in function mode, parameter steps in parametric/polar —
// and the owning screen maps index -> point + readout.
struct TraceCursor {
    bool active = false;
    int slot = 0;   // Function slot or parametric pair
    int index = 0;  // Pixel column (function) or parameter step

    // Move by dir (usually +/-1), clamped to [0, max_index].
    void step(int dir, int max_index);

    // Re-clamp after a recompute changes the walkable range.
    void clamp(int max_index);
};

}  // namespace graph
