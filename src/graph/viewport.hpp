#pragma once

namespace graph {

// Data<->pixel coordinate transform for a rectangular graph area
// (Phase 2 task 2.1, extracted from Phase 1's GraphScreen).
//
// Formulas are kept exactly as Phase 1 had them: x spreads across
// (width - 1) steps so the first column is x_min and the last is x_max;
// y maps [y_min, y_max] across `height` rows starting at `top`.
class Viewport {
public:
    // Window bounds in data space.
    double x_min = -10.0;
    double x_max = 10.0;
    double y_min = -10.0;
    double y_max = 10.0;

    // Graph area in pixels.
    int left = 0;
    int top = 0;
    int width = 320;
    int height = 280;

    // Map data coordinates to pixel coordinates (within the graph area).
    int px_x(double x_data) const;
    int px_y(double y_data) const;

    // Inverse: pixel to data (for trace, cursor readout).
    double data_x(int px) const;
    double data_y(int py) const;

    // Is a data point within the visible window?
    bool visible(double x_data, double y_data) const;
};

}  // namespace graph
