#include "graph/parametric_source.hpp"

#include <cmath>

namespace graph {

namespace {
constexpr int kTSlot = 't' - 'a';
// Absorbs float error so a range that's an exact multiple of the step
// includes its endpoint (e.g. [0, 2pi] at 2pi/63 emits 64 points).
constexpr double kEndpointSlack = 1e-9;
}  // namespace

ParametricSource::ParametricSource(math::Engine& eng, void* x_handle, void* y_handle, double t_min,
                                   double t_max, double t_step)
    : eng_(eng), x_handle_(x_handle), y_handle_(y_handle), t_min_(t_min), t_step_(t_step) {
    if (t_step > 0 && t_max >= t_min) {
        steps_ = static_cast<int>(std::floor((t_max - t_min) / t_step + kEndpointSlack));
    }
}

void ParametricSource::begin(const Viewport& /*vp*/) {
    i_ = 0;
}

bool ParametricSource::next(double* x_data, double* y_data, bool* defined) {
    if (i_ > steps_) {
        return false;
    }
    const double t = t_min_ + i_ * t_step_;
    ++i_;
    const double x = eng_.eval_compiled(x_handle_, kTSlot, t);
    const double y = eng_.eval_compiled(y_handle_, kTSlot, t);
    *x_data = x;
    *y_data = y;
    *defined = std::isfinite(x) && std::isfinite(y);
    return true;
}

}  // namespace graph
