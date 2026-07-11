#include "graph/viewport.hpp"

namespace graph {

int Viewport::px_x(double x_data) const {
    return left + static_cast<int>((x_data - x_min) / (x_max - x_min) * (width - 1));
}

int Viewport::px_y(double y_data) const {
    const double frac = (y_max - y_data) / (y_max - y_min);
    return top + static_cast<int>(frac * height);
}

double Viewport::data_x(int px) const {
    return x_min + (px - left) * (x_max - x_min) / (width - 1);
}

double Viewport::data_y(int py) const {
    return y_min + (y_max - y_min) * (top + height - py) / static_cast<double>(height);
}

bool Viewport::visible(double x_data, double y_data) const {
    return x_data >= x_min && x_data <= x_max && y_data >= y_min && y_data <= y_max;
}

}  // namespace graph
