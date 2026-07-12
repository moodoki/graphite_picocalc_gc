#pragma once

#include "math/engine.hpp"
#include "graph/plotter.hpp"

namespace graph {

// PointSource for polar mode (task 2.8): sweeps theta from theta_min
// to theta_max in theta_step increments, evaluates the compiled
// r(theta), and converts to Cartesian x = r*cos(theta),
// y = r*sin(theta). The sweep writes the engine's dedicated theta slot.
//
// Angle mode applies (spec §6.3): in degree mode the theta range is in
// degrees and the Cartesian conversion interprets it accordingly (the
// expression's own trig is already angle-mode aware).
class PolarSource : public PointSource {
public:
    // r_handle = an Engine::compile() result. Not owned.
    PolarSource(math::Engine& eng, void* r_handle, double theta_min, double theta_max,
                double theta_step);

    void begin(const Viewport& vp) override;
    bool next(double* x_data, double* y_data, bool* defined) override;

private:
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members) short-lived iterator
    math::Engine& eng_;
    void* r_handle_;
    double theta_min_;
    double theta_step_;
    int steps_ = 0;  // Points emitted = steps_ + 1
    int i_ = 0;
};

}  // namespace graph
