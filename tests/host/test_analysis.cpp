// Host-side tests for Phase 4B graph analysis: the numeric calculus
// primitives (extremum / derivative / integral), the mode-aware
// analyze_* operations, and the interactive session state machine.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "math/engine.hpp"
#include "math/numeric_solve.hpp"
#include "graph/analysis.hpp"
#include "graph/analysis_cursor.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void check_near(double got, double expected, const char* what, double tol = 1e-8) {
    ++g_checks;
    if (std::isnan(got) || std::fabs(got - expected) > tol) {
        std::printf("FAIL: %s -> %.15g (expected %.15g)\n", what, got, expected);
        ++g_failures;
    }
}

// ---- math primitives ----

void test_extremum() {
    using namespace math;

    // Spec acceptance (4B.4): max of -x^2+4 at x=0.
    auto r = numeric_extremum("4-x^2", 'x' - 'a', -2, 2, true);
    check(r.converged, "max converged");
    check_near(r.x, 0.0, "max location", 1e-7);
    check_near(r.value, 4.0, "max value", 1e-10);

    r = numeric_extremum("x^2-2*x", 'x' - 'a', 0, 3, false);
    check(r.converged, "min converged");
    check_near(r.x, 1.0, "min location", 1e-7);
    check_near(r.value, -1.0, "min value", 1e-10);

    r = numeric_extremum("sin(x)", 'x' - 'a', 0, M_PI, true);
    check_near(r.x, M_PI / 2, "sin max at pi/2", 1e-7);

    // Reversed bounds are accepted.
    r = numeric_extremum("x^2", 'x' - 'a', 1, -1, false);
    check_near(r.x, 0.0, "reversed bounds", 1e-7);

    // Monotonic: converges to the boundary.
    r = numeric_extremum("x", 'x' - 'a', 0, 1, false);
    check(r.converged && r.x < 1e-4, "monotonic hits boundary");

    // Undefined everywhere in the interval.
    r = numeric_extremum("ln(x)", 'x' - 'a', -2, -1, false);
    check(!r.converged && r.error != nullptr, "undefined interval errors");

    // Syntax error propagates.
    r = numeric_extremum("x^)", 'x' - 'a', 0, 1, false);
    check(!r.converged && std::strcmp(r.error, "Syntax error") == 0, "extremum syntax error");

    // The variable's value is restored.
    engine().vars()['x'] = 7;
    numeric_extremum("x^2", 'x' - 'a', -1, 1, false);
    check_near(engine().vars()['x'], 7.0, "extremum restores variable");
}

void test_derivative() {
    using namespace math;

    // Spec acceptance (4B.6): d/dx x^2 at 3 = 6.
    auto d = numeric_derivative("x^2", 'x' - 'a', 3);
    check(d.ok, "deriv ok");
    check_near(d.value, 6.0, "d/dx x^2 at 3", 1e-8);

    d = numeric_derivative("sin(x)", 'x' - 'a', 0);
    check_near(d.value, 1.0, "d/dx sin at 0", 1e-9);

    d = numeric_derivative("exp(x)", 'x' - 'a', 1);
    check_near(d.value, std::exp(1.0), "d/dx e^x at 1", 1e-7);

    // Explicit step.
    d = numeric_derivative("x^3", 'x' - 'a', 2, 1e-3);
    check_near(d.value, 12.0, "d/dx x^3 at 2 explicit h", 1e-7);

    // ln is undefined left of 0, so the central difference is NaN.
    d = numeric_derivative("ln(x)", 'x' - 'a', 0);
    check(!d.ok, "deriv undefined at domain edge");
}

void test_integral() {
    using namespace math;

    // Spec acceptance (4B.7): int_0^pi sin = 2.
    auto r = numeric_integral("sin(x)", 'x' - 'a', 0, M_PI);
    check(r.converged, "integral converged");
    check_near(r.value, 2.0, "int sin 0..pi", 1e-9);
    check(r.evals > 0, "evals counted");

    r = numeric_integral("x^2", 'x' - 'a', 0, 1);
    check_near(r.value, 1.0 / 3.0, "int x^2 0..1", 1e-10);

    // Reversed bounds negate.
    r = numeric_integral("x^2", 'x' - 'a', 1, 0);
    check_near(r.value, -1.0 / 3.0, "reversed bounds negate", 1e-10);

    r = numeric_integral("exp(x)", 'x' - 'a', -1, 1);
    check_near(r.value, std::exp(1.0) - std::exp(-1.0), "int e^x -1..1", 1e-9);

    // Oscillatory integrand subdivides.
    r = numeric_integral("sin(10*x)", 'x' - 'a', 0, 1);
    check_near(r.value, (1.0 - std::cos(10.0)) / 10.0, "int sin(10x) 0..1", 1e-8);

    // Endpoint singularity: Kronrod nodes avoid the endpoints; the
    // depth cap bounds the work (accuracy relaxed accordingly).
    r = numeric_integral("1/sqrt(x)", 'x' - 'a', 0, 1);
    check(r.converged, "endpoint singularity converges");
    check_near(r.value, 2.0, "int 1/sqrt(x) 0..1", 5e-3);

    // Empty interval.
    r = numeric_integral("x", 'x' - 'a', 2, 2);
    check(r.converged && r.value == 0, "a == b is zero");

    // An integrand undefined over a whole subinterval fails cleanly
    // (sqrt is NaN left of 0, and no amount of bisection fixes it).
    r = numeric_integral("sqrt(x)", 'x' - 'a', -1, 1);
    check(!r.converged && r.error != nullptr, "undefined region errors");

    r = numeric_integral("x^)", 'x' - 'a', 0, 1);
    check(!r.converged && std::strcmp(r.error, "Syntax error") == 0, "integral syntax error");
}

// ---- graph analysis: function mode ----

graph::GraphState make_function_state(const char* y1, const char* y2 = nullptr) {
    graph::GraphState st;
    st.mode = graph::Mode::kFunction;
    std::snprintf(st.y.expr[0], sizeof(st.y.expr[0]), "%s", y1);
    st.y.enabled[0] = true;
    if (y2 != nullptr) {
        std::snprintf(st.y.expr[1], sizeof(st.y.expr[1]), "%s", y2);
        st.y.enabled[1] = true;
    }
    return st;
}

void test_analyze_function() {
    using namespace graph;

    const GraphState st = make_function_state("x^2-2", "x");

    auto r = analyze_value(st, 0, 3);
    check(r.ok, "value ok");
    check_near(r.x, 3.0, "value x");
    check_near(r.y, 7.0, "value y");

    r = analyze_zero(st, 0, 0, 2, 1);
    check(r.ok, "zero ok");
    check_near(r.indep, std::sqrt(2.0), "zero at sqrt2", 1e-8);
    check_near(r.y, 0.0, "zero y ~ 0", 1e-6);

    // No zero inside the bounds.
    const GraphState nz = make_function_state("x^2+1");
    r = analyze_zero(nz, 0, -1, 1, 0);
    check(!r.ok && r.error != nullptr, "no zero errors");

    const GraphState par = make_function_state("4-x^2");
    r = analyze_extremum(par, 0, -2, 2, true);
    check(r.ok, "max ok");
    check_near(r.x, 0.0, "max x", 1e-7);
    check_near(r.y, 4.0, "max y", 1e-10);

    // Spec acceptance (4B.5): x and x^2 intersect at 0 and 1.
    const GraphState ix = make_function_state("x", "x^2");
    r = analyze_intersect(ix, 0, 1, 0.8);
    check(r.ok, "intersect ok");
    check_near(r.indep, 1.0, "intersect near 1", 1e-7);
    r = analyze_intersect(ix, 0, 1, 0.2);
    check(r.ok && std::fabs(r.indep) < 1e-6, "intersect near 0");
    r = analyze_intersect(ix, 0, 0, 0.5);
    check(!r.ok && std::strcmp(r.error, "Same curve") == 0, "same curve refused");

    r = analyze_derivative(st, 0, 3);
    check(r.ok, "deriv ok");
    check_near(r.aux, 6.0, "dy/dx of x^2-2 at 3", 1e-6);

    const GraphState sn = make_function_state("sin(x)");
    r = analyze_integral(sn, 0, 0, M_PI);
    check(r.ok, "integral ok");
    check_near(r.aux, 2.0, "int sin 0..pi", 1e-8);

    // Empty slot.
    r = analyze_value(st, 3, 1.0);
    check(!r.ok && std::strcmp(r.error, "No function") == 0, "empty slot errors");

    // The engine's X survives an analysis pass.
    math::engine().vars()['x'] = 5;
    analyze_zero(st, 0, 0, 2, 1);
    check_near(math::engine().vars()['x'], 5.0, "x restored");
}

// ---- graph analysis: parametric mode ----

graph::GraphState make_param_state(const char* xe, const char* ye) {
    graph::GraphState st;
    st.mode = graph::Mode::kParametric;
    std::snprintf(st.param.x_expr[0], sizeof(st.param.x_expr[0]), "%s", xe);
    std::snprintf(st.param.y_expr[0], sizeof(st.param.y_expr[0]), "%s", ye);
    st.param.enabled[0] = true;
    return st;
}

void test_analyze_parametric() {
    using namespace graph;

    const GraphState st = make_param_state("cos(t)", "sin(t)");

    auto r = analyze_value(st, 0, M_PI / 2);
    check(r.ok, "param value ok");
    check_near(r.x, 0.0, "param value x", 1e-12);
    check_near(r.y, 1.0, "param value y", 1e-12);

    // Spec acceptance (4B.8): parametric slope. On the unit circle at
    // t = pi/4 the slope is -cot(pi/4) = -1.
    r = analyze_derivative(st, 0, M_PI / 4);
    check(r.ok, "param deriv ok");
    check_near(r.aux, -1.0, "param dy/dx at pi/4", 1e-6);

    r = analyze_zero(st, 0, 3, 3.3, 3.1);
    check(r.ok, "param zero ok");
    check_near(r.indep, M_PI, "param zero at pi", 1e-8);

    r = analyze_extremum(st, 0, 0, M_PI, true);
    check(r.ok, "param max ok");
    check_near(r.indep, M_PI / 2, "param max at pi/2", 1e-6);
    check_near(r.y, 1.0, "param max y", 1e-10);

    // int Y(t) X'(t) dt over the top half circle = -pi/2 (signed area).
    r = analyze_integral(st, 0, 0, M_PI);
    check(r.ok, "param integral ok");
    check_near(r.aux, -M_PI / 2, "param integral", 1e-6);
}

// ---- graph analysis: polar mode ----

graph::GraphState make_polar_state(const char* re) {
    graph::GraphState st;
    st.mode = graph::Mode::kPolar;
    std::snprintf(st.polar.expr[0], sizeof(st.polar.expr[0]), "%s", re);
    st.polar.enabled[0] = true;
    return st;
}

void test_analyze_polar() {
    using namespace graph;

    const GraphState circ = make_polar_state("2*cos(theta)");

    auto r = analyze_value(circ, 0, 0);
    check(r.ok, "polar value ok");
    check_near(r.aux, 2.0, "polar value r");
    check_near(r.x, 2.0, "polar value x");
    check_near(r.y, 0.0, "polar value y", 1e-12);

    r = analyze_zero(circ, 0, 1, 2, 1.5);
    check(r.ok, "polar zero ok");
    check_near(r.indep, M_PI / 2, "polar zero at pi/2", 1e-8);

    // Unit circle: slope at theta = pi/4 is -1; area is pi.
    const GraphState unit = make_polar_state("1");
    r = analyze_derivative(unit, 0, M_PI / 4);
    check(r.ok, "polar deriv ok");
    check_near(r.aux, -1.0, "polar dy/dx at pi/4", 1e-6);

    // Spec acceptance (4B.8): polar area. r = 2cos(theta) over
    // [0, pi] traces a circle of radius 1 -> area pi.
    r = analyze_integral(circ, 0, 0, M_PI);
    check(r.ok, "polar area ok");
    check_near(r.aux, M_PI, "polar area of circle", 1e-8);

    // Degree mode: same geometry, theta swept in degrees.
    math::set_angle_mode(math::AngleMode::kDegrees);
    r = analyze_integral(circ, 0, 0, 180);
    check(r.ok, "polar area ok (deg)");
    check_near(r.aux, M_PI, "polar area of circle (deg)", 1e-6);
    r = analyze_derivative(unit, 0, 45);
    check(r.ok, "polar deriv ok (deg)");
    check_near(r.aux, -1.0, "polar dy/dx at 45deg", 1e-6);
    r = analyze_value(unit, 0, 90);
    check_near(r.y, 1.0, "polar value y (deg)", 1e-9);
    math::set_angle_mode(math::AngleMode::kRadians);
}

// ---- interactive session state machine ----

void test_session() {
    using namespace graph;

    const GraphState st = make_function_state("x^2-2", "x");

    AnalysisSession s;
    check(!s.active && !s.done, "session starts idle");

    // Zero: three-step bound/guess flow.
    s.begin(AnalysisOp::kZero, 0);
    check(s.active && !s.done, "session active after begin");
    check(std::strcmp(s.prompt(Mode::kFunction), "Left Bound?") == 0, "zero prompt 1");
    check(!s.slot_locked(), "slot free at step 0");
    check(!s.commit(0, 0.0), "left bound commit");
    check(std::strcmp(s.prompt(Mode::kFunction), "Right Bound?") == 0, "zero prompt 2");
    check(s.slot_locked(), "slot locked after first bound");
    check(!s.commit(0, 2.0), "right bound commit");
    check(std::strcmp(s.prompt(Mode::kFunction), "Guess?") == 0, "zero prompt 3");
    check(s.commit(0, 1.0), "guess completes inputs");
    s.compute(st);
    check(s.done && !s.active, "session done after compute");
    check(s.result.ok, "session zero ok");
    check_near(s.result.indep, std::sqrt(2.0), "session zero root", 1e-8);

    // Intersect: curve picking (P4-6: cursor-cycle).
    s.begin(AnalysisOp::kIntersect, 0);
    check(s.curve_pick(), "intersect starts on curve pick");
    check(std::strcmp(s.prompt(Mode::kFunction), "First curve?") == 0, "intersect prompt 1");
    check(!s.commit(0, 0.0), "first curve pick");
    check(std::strcmp(s.prompt(Mode::kFunction), "Second curve?") == 0, "intersect prompt 2");
    check(!s.commit(0, 0.0), "same curve refused");
    check(s.step == 1, "step unchanged on refusal");
    check(!s.commit(1, 0.0), "second curve pick");
    check(!s.curve_pick(), "guess step is not a pick");
    check(s.commit(1, 0.8), "guess completes intersect");
    s.compute(st);
    check(s.result.ok, "session intersect ok");
    // x^2 - 2 meets x at 2 and -1; Newton from the guess finds 2.
    check_near(s.result.indep, 2.0, "session intersect root", 1e-7);
    check(s.slot == 0 && s.slot2 == 1, "session slots recorded");

    // Value: single step; prompt is mode-aware.
    s.begin(AnalysisOp::kValue, 1);
    check(std::strcmp(s.prompt(Mode::kFunction), "X?") == 0, "value prompt func");
    check(std::strcmp(s.prompt(Mode::kParametric), "T?") == 0, "value prompt param");
    check(std::strcmp(s.prompt(Mode::kPolar), "th?") == 0, "value prompt polar");
    check(s.commit(1, 0.5), "value single commit");
    s.compute(st);
    check(s.result.ok, "session value ok");
    check_near(s.result.y, 0.5, "session value y (slot 2 = x)");

    // Integral: two limits.
    s.begin(AnalysisOp::kIntegral, 0);
    check(std::strcmp(s.prompt(Mode::kFunction), "Lower Limit?") == 0, "integral prompt 1");
    check(!s.commit(0, 0.0), "lower limit");
    check(std::strcmp(s.prompt(Mode::kFunction), "Upper Limit?") == 0, "integral prompt 2");
    check(s.commit(0, 1.0), "upper limit completes");
    s.compute(st);
    check(s.result.ok, "session integral ok");
    check_near(s.result.aux, 1.0 / 3.0 - 2.0, "session integral value", 1e-9);

    // Cancel.
    s.begin(AnalysisOp::kMinimum, 0);
    s.cancel();
    check(!s.active && !s.done, "cancel resets");

    // Op names (menu labels).
    check(std::strcmp(analysis_op_name(AnalysisOp::kDerivative), "dy/dx") == 0, "op name dy/dx");
    check(std::strcmp(analysis_op_name(AnalysisOp::kMinimum), "Minimum") == 0, "op name min");
}

}  // namespace

int main() {
    test_extremum();
    test_derivative();
    test_integral();
    test_analyze_function();
    test_analyze_parametric();
    test_analyze_polar();
    test_session();

    std::printf("test_analysis: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
