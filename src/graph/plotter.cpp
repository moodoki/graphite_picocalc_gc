#include "graph/plotter.hpp"

#include <cmath>
#include <cstdlib>

namespace graph {

void Plotter::begin() {
    pen_down_ = false;
}

void Plotter::point(gfx::Framebuffer& fb, int px, int py, bool defined, const PlotStyle& style) {
    if (!defined) {
        pen_down_ = false;
        return;
    }
    if (pen_down_ && std::abs(py - prev_py_) < kDiscontinuityThreshold) {
        fb.draw_line(prev_px_, prev_py_, px, py, style.color);
        if (style.thick) {
            fb.draw_line(prev_px_, prev_py_ + 1, px, py + 1, style.color);
        }
    } else {
        fb.set_pixel(px, py, style.color);
        if (style.thick) {
            fb.set_pixel(px, py + 1, style.color);
        }
    }
    prev_px_ = px;
    prev_py_ = py;
    pen_down_ = true;
}

void Plotter::plot(gfx::Framebuffer& fb, const Viewport& vp, PointSource& source,
                   const PlotStyle& style) {
    begin();
    source.begin(vp);
    double x = 0.0;
    double y = 0.0;
    bool defined = false;
    while (source.next(&x, &y, &defined)) {
        if (!defined || !std::isfinite(x) || !std::isfinite(y)) {
            point(fb, 0, 0, false, style);
            continue;
        }
        int py = vp.px_y(y);
        py = py < -kClampPy ? -kClampPy : (py > kClampPy ? kClampPy : py);
        point(fb, vp.px_x(x), py, true, style);
    }
}

}  // namespace graph
