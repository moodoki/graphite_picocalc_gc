// Host-side tests for the extracted graph subsystem (Phase 2 task 2.1).
// Locks the Viewport transforms to Phase 1's GraphScreen formulas so the
// refactor is provably behavior-preserving.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "graph/function_source.hpp"
#include "graph/graph_mode.hpp"
#include "apps/table_model.hpp"
#include "graph/parametric_source.hpp"
#include "graph/polar_source.hpp"
#include "graph/trace.hpp"
#include "math/types.hpp"
#include "graph/graph_state.hpp"
#include "graph/viewport.hpp"
#include "math/engine.hpp"

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

    // Mode descriptors (task 2.2, spec §1/§3).
    {
        using graph::Mode;
        const auto& fd = graph::descriptor_for(Mode::kFunction);
        expect(fd.mode == Mode::kFunction && fd.independent_var == 'x' && fd.slot_count == 7,
               "function descriptor: x, 7 slots");
        expect(fd.slot_prefix[0] == 'Y', "function slot prefix Y");
        const auto& pd = graph::descriptor_for(Mode::kParametric);
        expect(pd.mode == Mode::kParametric && pd.independent_var == 't' && pd.slot_count == 6,
               "parametric descriptor: t, 6 pairs");
        expect(pd.slot_prefix[0] == 0, "parametric has no slot prefix (X/Y pairs)");
        const auto& od = graph::descriptor_for(Mode::kPolar);
        expect(od.mode == Mode::kPolar && od.independent_var == 0 && od.slot_count == 6,
               "polar descriptor: theta slot, 6 slots");
        expect(od.slot_prefix[0] == 'r', "polar slot prefix r");
    }

    // GraphState defaults (task 2.2, spec §5.2/§6.2/§7.1/§9).
    {
        graph::GraphState st;
        expect(st.mode == graph::Mode::kFunction, "default mode is function");
        expect(st.window.x_min == -10.0 && st.window.x_max == 10.0 && st.window.x_scl == 1.0,
               "default window is ZStandard");
        expect(!st.y.any_enabled(), "no functions enabled by default");
        expect(st.t_min == 0.0 && std::fabs(st.t_max - 2.0 * M_PI) < 1e-12,
               "t range defaults to [0, 2pi]");
        expect(std::fabs(st.t_step - 2.0 * M_PI / 63.0) < 1e-12, "t_step defaults to 2pi/63");
        expect(st.theta_min == 0.0 && std::fabs(st.theta_max - 2.0 * M_PI) < 1e-12 &&
                   st.theta_step == 0.05,
               "theta range defaults to [0, 2pi] step 0.05");
        expect(st.table.start == 0.0 && st.table.step == 1.0 && !st.table.ask_mode,
               "table config defaults");
        st.y.enabled[2] = true;
        st.y.expr[2][0] = 'x';
        expect(st.y.any_enabled(), "any_enabled sees slot 3");
    }

    // FunctionSource (task 2.3): one evaluated point per pixel column.
    {
        auto& eng = math::engine();
        const graph::Viewport vp = phase1_viewport();
        double x = 0.0;
        double y = 0.0;
        bool defined = false;

        void* h = eng.compile("x^2");
        expect(h != nullptr, "compile x^2");
        graph::FunctionSource parabola(eng, h);
        parabola.begin(vp);
        int n = 0;
        bool first_ok = false;
        bool all_defined = true;
        while (parabola.next(&x, &y, &defined)) {
            if (n == 0) {
                first_ok = defined && x == -10.0 && std::fabs(y - 100.0) < 1e-9;
            }
            all_defined = all_defined && defined;
            ++n;
        }
        expect(n == 320, "one point per pixel column");
        expect(first_ok, "first point is (-10, 100)");
        expect(all_defined, "x^2 defined everywhere");
        eng.free_compiled(h);

        // D51: patching tinyexpr rather than only the home-screen evaluator is
        // what makes the fix reach here. `(-2)^x` at x_min is (-2)^-10 =
        // +1/1024; before the fix the negation was hoisted out of the power
        // and every column came back with the wrong sign.
        h = eng.compile("(-2)^x");
        expect(h != nullptr, "compile (-2)^x");
        graph::FunctionSource negbase(eng, h);
        negbase.begin(vp);
        bool neg_first_ok = false;
        if (negbase.next(&x, &y, &defined)) {
            neg_first_ok = defined && x == -10.0 && std::fabs(y - 1.0 / 1024.0) < 1e-12;
        }
        expect(neg_first_ok, "(-2)^x at x=-10 is +1/1024, not -1/1024");
        eng.free_compiled(h);

        h = eng.compile("sqrt(x)");
        expect(h != nullptr, "compile sqrt(x)");
        graph::FunctionSource half(eng, h);
        half.begin(vp);
        n = 0;
        int undefined = 0;
        while (half.next(&x, &y, &defined)) {
            if (!defined) {
                ++undefined;
            }
            ++n;
        }
        expect(n == 320 && undefined > 100 && undefined < 200,
               "sqrt(x) undefined on the negative half only");
        eng.free_compiled(h);
    }

    // Parameterized swept variable (task 2.4): eval_compiled can drive
    // any slot, and the X sweep is unchanged.
    {
        auto& eng = math::engine();
        void* h = eng.compile("t^2+1");
        expect(h != nullptr, "compile t^2+1");
        expect(std::fabs(eng.eval_compiled(h, 't' - 'a', 3.0) - 10.0) < 1e-12,
               "sweep writes the t slot");
        eng.free_compiled(h);

        h = eng.compile("theta*2");
        expect(h != nullptr, "compile theta*2");
        expect(std::fabs(eng.eval_compiled(h, math::Variables::kTheta, 1.5) - 3.0) < 1e-12,
               "sweep writes the theta slot");
        eng.free_compiled(h);

        h = eng.compile("x+1");
        expect(std::fabs(eng.eval_compiled(h, 41.0) - 42.0) < 1e-12,
               "2-arg eval_compiled still sweeps X");
        eng.free_compiled(h);
    }

    // ParametricSource (task 2.4): unit circle over [0, 2pi].
    {
        auto& eng = math::engine();
        const graph::Viewport vp = phase1_viewport();
        void* xh = eng.compile("cos(t)");
        void* yh = eng.compile("sin(t)");
        expect(xh != nullptr && yh != nullptr, "compile cos(t)/sin(t)");

        const double two_pi = 2.0 * M_PI;
        graph::ParametricSource circle(eng, xh, yh, 0.0, two_pi, two_pi / 63.0);
        circle.begin(vp);
        double x = 0.0;
        double y = 0.0;
        bool defined = false;
        int n = 0;
        bool on_circle = true;
        bool first_ok = false;
        double last_x = 0.0;
        while (circle.next(&x, &y, &defined)) {
            if (n == 0) {
                first_ok = defined && std::fabs(x - 1.0) < 1e-12 && std::fabs(y) < 1e-12;
            }
            on_circle = on_circle && defined && std::fabs(x * x + y * y - 1.0) < 1e-9;
            last_x = x;
            ++n;
        }
        expect(n == 64, "63 steps over 2pi emit 64 points (endpoint included)");
        expect(first_ok, "starts at (1, 0)");
        expect(on_circle, "every point lies on the unit circle");
        expect(std::fabs(last_x - 1.0) < 1e-9, "ends back at t = 2pi");
        eng.free_compiled(xh);
        eng.free_compiled(yh);

        // Degenerate step: only the start point, undefined (null
        // handles eval to NaN), and no hang.
        graph::ParametricSource none(eng, nullptr, nullptr, 0.0, 1.0, 0.0);
        none.begin(vp);
        int emitted = 0;
        bool any_defined = false;
        while (none.next(&x, &y, &defined)) {
            any_defined = any_defined || defined;
            ++emitted;
        }
        expect(emitted == 1 && !any_defined, "zero step emits one undefined point");
    }

    // PolarSource (task 2.8): cardioid in radians, circle in degrees.
    {
        auto& eng = math::engine();
        const graph::Viewport vp = phase1_viewport();
        const double two_pi = 2.0 * M_PI;
        double x = 0.0;
        double y = 0.0;
        bool defined = false;

        void* rh = eng.compile("1+cos(theta)");
        expect(rh != nullptr, "compile cardioid");
        graph::PolarSource cardioid(eng, rh, 0.0, two_pi, two_pi / 63.0);
        cardioid.begin(vp);
        int n = 0;
        bool first_ok = false;
        bool all_defined = true;
        while (cardioid.next(&x, &y, &defined)) {
            if (n == 0) {
                // theta=0: r=2 -> (2, 0).
                first_ok = defined && std::fabs(x - 2.0) < 1e-9 && std::fabs(y) < 1e-9;
            }
            all_defined = all_defined && defined;
            ++n;
        }
        expect(n == 64, "cardioid sweep emits 64 points");
        expect(first_ok && all_defined, "cardioid starts at (2, 0), defined throughout");
        eng.free_compiled(rh);

        // Degree mode: theta range in degrees; conversion must follow.
        math::set_angle_mode(math::AngleMode::kDegrees);
        rh = eng.compile("1");
        graph::PolarSource circle(eng, rh, 0.0, 360.0, 90.0);
        circle.begin(vp);
        n = 0;
        bool quarter_ok = true;
        while (circle.next(&x, &y, &defined)) {
            // 0, 90, 180, 270, 360 degrees -> unit-circle axis points.
            const double ax = std::fabs(x);
            const double ay = std::fabs(y);
            quarter_ok = quarter_ok && defined &&
                         ((ax > 1.0 - 1e-9 && ay < 1e-9) || (ay > 1.0 - 1e-9 && ax < 1e-9));
            ++n;
        }
        expect(n == 5, "degree circle emits 0/90/180/270/360");
        expect(quarter_ok, "degree-mode theta converts to Cartesian correctly");
        eng.free_compiled(rh);
        math::set_angle_mode(math::AngleMode::kRadians);
    }

    // TraceCursor (task 2.7): clamped stepping.
    {
        graph::TraceCursor tc;
        tc.index = 5;
        tc.step(-1, 63);
        expect(tc.index == 4, "step left");
        tc.step(-10, 63);
        expect(tc.index == 0, "clamps at 0");
        tc.index = 62;
        tc.step(+1, 63);
        tc.step(+1, 63);
        expect(tc.index == 63, "clamps at max_index");
        tc.clamp(9);
        expect(tc.index == 9, "re-clamp after range shrink");
    }

    // Table model (task 2.13): mode-aware columns + row evaluation.
    {
        char label[8];
        double results[apps::kMaxTableColumns];

        // Function mode: enabled slots in order (Y1, Y3), gap skipped.
        graph::GraphState st;
        std::snprintf(st.y.expr[0], sizeof(st.y.expr[0]), "x^2");
        st.y.enabled[0] = true;
        std::snprintf(st.y.expr[2], sizeof(st.y.expr[2]), "x+1");
        st.y.enabled[2] = true;
        expect(apps::table_column_count(st) == 2, "function mode: 2 columns");
        apps::table_column_label(st, 0, label, sizeof(label));
        expect(label[0] == 'Y' && label[1] == '1', "column 0 is Y1");
        apps::table_column_label(st, 1, label, sizeof(label));
        expect(label[0] == 'Y' && label[1] == '3', "column 1 is Y3 (gap skipped)");
        expect(apps::evaluate_table_row(st, 3.0, results, apps::kMaxTableColumns) == 2 &&
                   std::fabs(results[0] - 9.0) < 1e-12 && std::fabs(results[1] - 4.0) < 1e-12,
               "row at x=3 -> [9, 4]");
        expect(std::strcmp(apps::table_independent_label(st), "x") == 0, "independent is x");

        // Parametric: T | X1T Y1T.
        graph::GraphState pt;
        pt.mode = graph::Mode::kParametric;
        std::snprintf(pt.param.x_expr[0], sizeof(pt.param.x_expr[0]), "cos(t)");
        std::snprintf(pt.param.y_expr[0], sizeof(pt.param.y_expr[0]), "sin(t)");
        pt.param.enabled[0] = true;
        expect(apps::table_column_count(pt) == 2, "parametric pair -> 2 columns");
        apps::table_column_label(pt, 0, label, sizeof(label));
        expect(std::strcmp(label, "X1T") == 0, "column 0 is X1T");
        apps::table_column_label(pt, 1, label, sizeof(label));
        expect(std::strcmp(label, "Y1T") == 0, "column 1 is Y1T");
        expect(apps::evaluate_table_row(pt, 0.0, results, apps::kMaxTableColumns) == 2 &&
                   std::fabs(results[0] - 1.0) < 1e-12 && std::fabs(results[1]) < 1e-12,
               "row at t=0 -> [1, 0]");

        // Polar: th | r1; sweep writes the theta slot.
        graph::GraphState po;
        po.mode = graph::Mode::kPolar;
        std::snprintf(po.polar.expr[0], sizeof(po.polar.expr[0]), "2*theta");
        po.polar.enabled[0] = true;
        expect(apps::table_column_count(po) == 1, "polar -> 1 column");
        apps::table_column_label(po, 0, label, sizeof(label));
        expect(std::strcmp(label, "r1") == 0, "column 0 is r1");
        expect(apps::evaluate_table_row(po, 2.0, results, apps::kMaxTableColumns) == 1 &&
                   std::fabs(results[0] - 4.0) < 1e-12,
               "row at theta=2 -> [4]");
        const char theta[2] = {gfx::kGlyphTheta, 0};
        expect(std::strcmp(apps::table_independent_label(po), theta) == 0, "independent is theta");

        // Syntax error in a slot -> NaN column, others unaffected.
        std::snprintf(st.y.expr[0], sizeof(st.y.expr[0]), "x^^2");
        expect(apps::evaluate_table_row(st, 3.0, results, apps::kMaxTableColumns) == 2 &&
                   std::isnan(results[0]) && std::fabs(results[1] - 4.0) < 1e-12,
               "bad expression yields NaN column");
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
