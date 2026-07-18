#pragma once

#include "math/engine.hpp"
#include "graph/plotter.hpp"

namespace graph {

// PointSource for parametric mode (task 2.4): sweeps t from t_min to
// t_max in t_step increments and evaluates the compiled (x(t), y(t))
// pair. The step count is fixed up front with an integer counter (no
// float accumulation), endpoint included when it lands on the grid.
class ParametricSource : public PointSource {
public:
    // Handles = Engine::compile() results for x(t) and y(t). Not owned.
    ParametricSource(math::Engine& eng, void* x_handle, void* y_handle, double t_min, double t_max,
                     double t_step);

    void begin(const Viewport& vp) override;
    bool next(double* x_data, double* y_data, bool* defined) override;

private:
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members) short-lived iterator
    math::Engine& eng_;
    void* x_handle_;
    void* y_handle_;
    double t_min_;
    double t_max_;
    double t_step_;
    int steps_ = 0;      // Grid points emitted = steps_ + 1
    bool tail_ = false;  // Extra sample clamped to t_max when the grid stops short
    int i_ = 0;
};

}  // namespace graph
