#include "graph/function_source.hpp"

#include <cmath>

namespace graph {

void FunctionSource::begin(const Viewport& vp) {
    vp_ = &vp;
    col_ = 0;
}

bool FunctionSource::next(double* x_data, double* y_data, bool* defined) {
    if (vp_ == nullptr || col_ >= vp_->width) {
        return false;
    }
    const double x = vp_->data_x(vp_->left + col_);
    const double y = eng_.eval_compiled(handle_, x);
    ++col_;
    *x_data = x;
    *y_data = y;
    *defined = std::isfinite(y);
    return true;
}

}  // namespace graph
