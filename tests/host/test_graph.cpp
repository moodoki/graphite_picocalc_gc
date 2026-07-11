// Host-side tests for the extracted graph subsystem (Phase 2 task 2.1).
// Locks the Viewport transforms to Phase 1's GraphScreen formulas so the
// refactor is provably behavior-preserving.

#include <cmath>
#include <cstdio>

#include "graph/viewport.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void expect(bool cond, const char* what) {
    ++g_checks;
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

// Phase 1 GraphScreen geometry: full-width plot between status bar and
// softkeys.
graph::Viewport phase1_viewport() {
    graph::Viewport vp;
    vp.x_min = -10.0;
    vp.x_max = 10.0;
    vp.y_min = -10.0;
    vp.y_max = 10.0;
    vp.left = 0;
    vp.top = 16;
    vp.width = 320;
    vp.height = 280;
    return vp;
}

}  // namespace

int main() {
    // Data -> pixel endpoints (Phase 1: x spans width-1 steps).
    {
        const graph::Viewport vp = phase1_viewport();
        expect(vp.px_x(-10.0) == 0, "px_x(x_min) = 0");
        expect(vp.px_x(10.0) == 319, "px_x(x_max) = width-1");
        expect(vp.px_x(0.0) == 159, "px_x(0) = 159 (truncated center)");
        expect(vp.px_y(10.0) == 16, "px_y(y_max) = top");
        expect(vp.px_y(-10.0) == 296, "px_y(y_min) = top+height");
        expect(vp.px_y(0.0) == 156, "px_y(0) = top + height/2");
    }

    // Pixel -> data round trips.
    {
        const graph::Viewport vp = phase1_viewport();
        expect(vp.data_x(0) == -10.0, "data_x(0) = x_min");
        expect(vp.data_x(319) == 10.0, "data_x(width-1) = x_max");
        expect(std::fabs(vp.data_y(296) - (-10.0)) < 1e-12, "data_y(bottom) = y_min");
        expect(std::fabs(vp.data_y(16) - 10.0) < 1e-12, "data_y(top) = y_max");

        // Column -> x -> column round trip. px_x truncates, so float
        // error may land one pixel low; Phase 1 behaved the same.
        bool ok = true;
        for (int px = 0; px < 320; ++px) {
            const int back = vp.px_x(vp.data_x(px));
            if (back != px && back != px - 1) {
                ok = false;
                break;
            }
        }
        expect(ok, "px_x(data_x(px)) round trip within truncation error");
    }

    // Formula parity with Phase 1 draw_axes/value_to_py on an
    // asymmetric window (regression guard for the extraction).
    {
        graph::Viewport vp = phase1_viewport();
        vp.x_min = -3.0;
        vp.x_max = 7.5;
        vp.y_min = -0.5;
        vp.y_max = 2.0;
        bool ok_x = true;
        for (double x = -3.0; x <= 7.5; x += 0.37) {
            const int expected = static_cast<int>((x - vp.x_min) / (vp.x_max - vp.x_min) * 319);
            if (vp.px_x(x) != expected) {
                ok_x = false;
                break;
            }
        }
        expect(ok_x, "px_x matches Phase 1 grid-line formula");
        bool ok_y = true;
        for (double y = -0.5; y <= 2.0; y += 0.13) {
            const double frac = (vp.y_max - y) / (vp.y_max - vp.y_min);
            const int expected = 16 + static_cast<int>(frac * 280);
            if (vp.px_y(y) != expected) {
                ok_y = false;
                break;
            }
        }
        expect(ok_y, "px_y matches Phase 1 value_to_py");
        const double y_readout = vp.y_min + (vp.y_max - vp.y_min) * (16 + 280 - 200) / 280.0;
        expect(std::fabs(vp.data_y(200) - y_readout) < 1e-12,
               "data_y matches Phase 1 trace readout");
    }

    // visible()
    {
        const graph::Viewport vp = phase1_viewport();
        expect(vp.visible(0.0, 0.0), "origin visible");
        expect(vp.visible(-10.0, 10.0), "window corner visible (inclusive)");
        expect(!vp.visible(10.1, 0.0), "x beyond x_max not visible");
        expect(!vp.visible(0.0, -10.1), "y below y_min not visible");
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
