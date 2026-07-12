#include "graph/polar_source.hpp"

#include <cmath>

#include "math/types.hpp"

namespace graph {

namespace {
constexpr double kPi = 3.14159265358979323846;
// Same endpoint slack as ParametricSource: a range that's an exact
// multiple of the step includes its endpoint.
constexpr double kEndpointSlack = 1e-9;
}  // namespace

PolarSource::PolarSource(math::Engine& eng, void* r_handle, double theta_min, double theta_max,
                         double theta_step)
    : eng_(eng), r_handle_(r_handle), theta_min_(theta_min), theta_step_(theta_step) {
    if (theta_step > 0 && theta_max >= theta_min) {
        steps_ =
            static_cast<int>(std::floor((theta_max - theta_min) / theta_step + kEndpointSlack));
    }
}

void PolarSource::begin(const Viewport& /*vp*/) {
    i_ = 0;
}

bool PolarSource::next(double* x_data, double* y_data, bool* defined) {
    if (i_ > steps_) {
        return false;
    }
    const double theta = theta_min_ + i_ * theta_step_;
    ++i_;
    const double r = eng_.eval_compiled(r_handle_, math::Variables::kTheta, theta);
    const double theta_rad =
        math::angle_mode() == math::AngleMode::kDegrees ? theta * kPi / 180.0 : theta;
    *x_data = r * std::cos(theta_rad);
    *y_data = r * std::sin(theta_rad);
    *defined = std::isfinite(r);
    return true;
}

}  // namespace graph
