// Host-side tests for the `calc` module's C++ side (Phase 6B.3-6B.5,
// src/scripting/calc_api.{h,cpp}).
//
// This is where the calc module is really tested. The MicroPython glue in
// mp_calc_module.c is argument conversion and object construction — thin by
// design, and untestable off-device — while everything with a decision in it
// lives here: which pipeline stage handles an expression, what a complex
// result looks like coming back, which variable names are refused, and whether
// the reentrancy guard holds. calc_api.cpp deliberately depends on nothing but
// math/, and this suite is the reason.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.hpp"
#include "graph/graph_state.hpp"
#include "math/engine.hpp"
#include "math/types.hpp"
#include "scripting/calc_api.h"

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

void check_near(double got, double want, const char* what) {
    ++g_checks;
    if (!(std::fabs(got - want) < 1e-9)) {
        std::printf("FAIL: %s — got %.17g, wanted %.17g\n", what, got, want);
        ++g_failures;
    }
}

// For results of a numeric search rather than an exact computation. A
// minimiser locates the *argument* of a smooth minimum to about sqrt(eps),
// ~1.5e-8, because the function is flat there — that is Brent working
// correctly, not a loose answer, so holding it to check_near's 1e-9 would
// be testing the wrong thing.
void check_close(double got, double want, double tol, const char* what) {
    ++g_checks;
    if (!(std::fabs(got - want) < tol)) {
        std::printf("FAIL: %s — got %.17g, wanted %.17g (tol %g)\n", what, got, want, tol);
        ++g_failures;
    }
}

void check_str(const char* got, const char* want, const char* what) {
    ++g_checks;
    if (std::strcmp(got, want) != 0) {
        std::printf("FAIL: %s — got \"%s\", wanted \"%s\"\n", what, got, want);
        ++g_failures;
    }
}

// ---- Wrappers that give each call its own buffers, as the glue does ----

struct EvalOut {
    CalcStatus status = kCalcOk;
    CalcKind kind = kCalcReal;
    double re = 0;
    double im = 0;
    char text[kCalcTextMax] = {};
    const char* err = nullptr;
};

EvalOut eval(const char* expr) {
    EvalOut o;
    o.status = calc_api_eval(expr, &o.kind, &o.re, &o.im, o.text, sizeof(o.text), &o.err);
    return o;
}

struct CasOut {
    CalcStatus status = kCalcOk;
    CalcKind kind = kCalcText;
    double re = 0;
    char text[kCalcTextMax] = {};
    const char* err = nullptr;
};

CasOut cas(const char* op, const char* expr, const char* var = nullptr,
           const char* arg3 = nullptr, const char* arg4 = nullptr) {
    CasOut o;
    o.status =
        calc_api_cas(op, expr, var, arg3, arg4, &o.kind, &o.re, o.text, sizeof(o.text), &o.err);
    return o;
}

// ---- 6B.3: the evaluation pipeline ----

void test_eval_scalar() {
    math::set_angle_mode(math::AngleMode::kRadians);
    math::set_number_mode(math::NumberMode::kReal);

    const EvalOut a = eval("2+3*4");
    check(a.status == kCalcOk, "2+3*4 evaluates");
    check(a.kind == kCalcReal, "2+3*4 is a real scalar");
    check_near(a.re, 14, "2+3*4 == 14");

    const EvalOut b = eval("sin(pi/4)");
    check(b.status == kCalcOk, "sin(pi/4) evaluates");
    check_near(b.re, 0.7071067811865476, "sin(pi/4)");

    // The spec's own headline example (§4.2).
    const EvalOut c = eval("2 + 3 * sin(pi/4)");
    check_near(c.re, 2 + 3 * 0.7071067811865476, "2 + 3*sin(pi/4)");
}

void test_eval_rejects_junk() {
    const EvalOut a = eval("2+*3");
    check(a.status == kCalcFailed, "a syntax error fails");
    check(a.err != nullptr && a.err[0] != 0, "and says why");

    const EvalOut b = eval("");
    check(b.status == kCalcFailed, "an empty expression fails");

    const EvalOut c = eval(nullptr);
    check(c.status == kCalcFailed, "a null expression fails rather than crashing");
}

void test_eval_complex() {
    // HomeResult::scalar_value carries only the real part; the imaginary part
    // comes back from Ans. If this ever regresses, a complex result silently
    // becomes its real part — which is exactly the failure worth a test.
    math::set_number_mode(math::NumberMode::kRectangular);
    const EvalOut a = eval("2+3i");
    check(a.status == kCalcOk, "2+3i evaluates in a+bi mode");
    check(a.kind == kCalcComplex, "2+3i comes back complex");
    check_near(a.re, 2, "re(2+3i)");
    check_near(a.im, 3, "im(2+3i)");

    // A real result immediately afterwards must NOT inherit the previous
    // imaginary part — the VM's set_real clears it, and this pins that.
    const EvalOut b = eval("5");
    check(b.kind == kCalcReal, "a real result after a complex one is real");
    check_near(b.im, 0, "and its imaginary part is zero");

    math::set_number_mode(math::NumberMode::kReal);
    const EvalOut c = eval("sqrt(-1)");
    check(c.status == kCalcFailed, "sqrt(-1) fails in REAL mode, as on the home screen");
}

void test_eval_non_scalar_is_text() {
    const EvalOut a = eval("{1,2,3}+1");
    check(a.status == kCalcOk, "a list expression evaluates");
    check(a.kind == kCalcText, "a list result comes back as text");
    check(std::strchr(a.text, '2') != nullptr, "and the text holds the values");

    const EvalOut b = eval("[[1,2][3,4]]");
    check(b.status == kCalcOk, "a matrix expression evaluates");
    check(b.kind == kCalcText, "a matrix result comes back as text");
}

void test_eval_reaches_the_cas_stage() {
    // Step 1 of the pipeline. diff() is not something the numeric evaluator
    // can do, so a correct answer here proves cas::evaluate_home ran first.
    const EvalOut a = eval("diff(x^3,x)");
    check(a.status == kCalcOk, "eval() handles an inline CAS call");
    check(a.kind == kCalcText, "and returns it symbolically");
    check(std::strstr(a.text, "x^2") != nullptr, "diff(x^3,x) mentions x^2");

    // A CAS call that folds to a constant returns a number, not "5".
    const EvalOut b = eval("simplify(2+3)");
    check(b.kind == kCalcReal, "a constant CAS result is a number");
    check_near(b.re, 5, "simplify(2+3) == 5");

    // Negative constants are kNeg over kNum, not kNum — the case is_num()
    // alone would miss.
    const EvalOut c = eval("simplify(2-5)");
    check(c.kind == kCalcReal, "a negative constant result is a number too");
    check_near(c.re, -3, "simplify(2-5) == -3");
}

void test_eval_reaches_the_solve_stage() {
    // Step 2. solve() with numeric bounds is the numeric solver's shape
    // (D28), which cas::evaluate_home declines — so this only works if
    // solveexpr::substitute ran.
    const EvalOut a = eval("solve(x^2-4,x,0,10)");
    check(a.status == kCalcOk, "eval() handles a bracketed numeric solve");
    check(a.kind == kCalcReal, "which yields a number");
    check_near(a.re, 2, "solve(x^2-4,x,0,10) == 2");
}

void test_eval_reaches_the_convert_stage() {
    // Step 3.
    const EvalOut a = eval("convert(1,\"mi\",\"km\")");
    check(a.status == kCalcOk, "eval() handles a unit conversion");
    check_near(a.re, 1.609344, "1 mi == 1.609344 km");

    // And it composes, which is the point of substitution over a special case.
    const EvalOut b = eval("2*convert(1,\"mi\",\"km\")");
    check_near(b.re, 2 * 1.609344, "a conversion composes with arithmetic");
}

void test_eval_solve_set_renders_like_the_home_screen() {
    math::set_number_mode(math::NumberMode::kReal);
    const EvalOut a = eval("solve(x^2-4=0,x)");
    check(a.status == kCalcOk, "a symbolic solve evaluates");
    check(a.kind == kCalcText, "and comes back as text");
    check(a.text[0] == 'x' && a.text[1] == ' ' && a.text[2] == '=', "shaped as \"x = {...}\"");
    check(std::strchr(a.text, '{') != nullptr && std::strchr(a.text, '}') != nullptr,
          "with the set in braces");
}

// ---- 6B.3: variables ----

int g_persist_calls = 0;
void count_persist(CalcPersistTarget what, int) {
    if (what == kCalcPersistVars) {
        ++g_persist_calls;
    }
}

void test_store_and_recall() {
    calc_api_set_persist_hook(&count_persist);
    g_persist_calls = 0;

    const char* err = nullptr;
    check(calc_api_store("a", 42, 0, &err) == kCalcOk, "store to a");
    check(g_persist_calls == 1, "a successful store persists exactly once");

    double re = 0;
    double im = 0;
    int is_complex = 0;
    check(calc_api_recall("a", &re, &im, &is_complex, &err) == kCalcOk, "recall a");
    check_near(re, 42, "a == 42");
    check(is_complex == 0, "a is real");

    // The variable the evaluator sees is the same one.
    const EvalOut e = eval("a+1");
    check_near(e.re, 43, "the evaluator sees the stored value");

    // ... and a store through the evaluator is visible to recall.
    eval("7->b");
    check(calc_api_recall("b", &re, &im, &is_complex, &err) == kCalcOk, "recall b");
    check_near(re, 7, "b == 7 after an evaluator store");

    calc_api_set_persist_hook(nullptr);
}

void test_store_complex_round_trip() {
    calc_api_set_persist_hook(nullptr);
    const char* err = nullptr;
    check(calc_api_store("c", 3, 4, &err) == kCalcOk, "store a complex value");

    double re = 0;
    double im = 0;
    int is_complex = 0;
    check(calc_api_recall("c", &re, &im, &is_complex, &err) == kCalcOk, "recall it");
    check(is_complex == 1, "c reads back complex");
    check_near(re, 3, "re(c)");
    check_near(im, 4, "im(c)");

    // Overwriting with a real value must clear the imaginary part, or the
    // slot stays complex forever.
    check(calc_api_store("c", 1, 0, &err) == kCalcOk, "overwrite with a real");
    check(calc_api_recall("c", &re, &im, &is_complex, &err) == kCalcOk, "recall again");
    check(is_complex == 0, "c is real again");
    check_near(im, 0, "and its imaginary part is gone");
}

void test_variable_names_are_strict() {
    const char* err = nullptr;
    double re = 0;
    double im = 0;
    int is_complex = 0;

    // The whole point: math::Variables::operator[] would map every one of
    // these to Ans and report success.
    check(calc_api_store("A", 1, 0, &err) == kCalcFailed, "uppercase A is refused");
    check(calc_api_store("ab", 1, 0, &err) == kCalcFailed, "a two-letter name is refused");
    check(calc_api_store("", 1, 0, &err) == kCalcFailed, "an empty name is refused");
    check(calc_api_store(nullptr, 1, 0, &err) == kCalcFailed, "a null name is refused");
    check(calc_api_store("1", 1, 0, &err) == kCalcFailed, "a digit is refused");
    check(calc_api_recall("A", &re, &im, &is_complex, &err) == kCalcFailed,
          "recall is just as strict");

    // Ans specifically must not be reachable by accident, only by name.
    check(calc_api_store("ans", 5, 0, &err) == kCalcOk, "\"ans\" is accepted");
    check(calc_api_recall("ans", &re, &im, &is_complex, &err) == kCalcOk, "and recalls");
    check_near(re, 5, "ans == 5");

    check(calc_api_store("theta", 9, 0, &err) == kCalcOk, "\"theta\" is accepted");
    check(calc_api_recall("theta", &re, &im, &is_complex, &err) == kCalcOk, "and recalls");
    check_near(re, 9, "theta == 9");

    // Every rejection carries a message; a bare failure code would reach
    // Python as an exception with nothing in it.
    err = nullptr;
    calc_api_store("A", 1, 0, &err);
    check(err != nullptr && err[0] != 0, "a rejected name explains itself");
}

// ---- The reentrancy guard ----

int g_reentrant_status = -1;

void reenter(CalcPersistTarget, int) {
    // Stands in for a __del__ finalizer running inside a binding: the guard
    // is held by the store that called us.
    const char* err = nullptr;
    double re = 0;
    double im = 0;
    int is_complex = 0;
    g_reentrant_status = calc_api_recall("a", &re, &im, &is_complex, &err);
}

void test_reentrancy_guard() {
    calc_api_set_persist_hook(&reenter);
    g_reentrant_status = -1;
    const char* err = nullptr;
    check(calc_api_store("a", 1, 0, &err) == kCalcOk, "the outer call still succeeds");
    check(g_reentrant_status == kCalcBusy, "a re-entrant call is refused, not run");
    calc_api_set_persist_hook(nullptr);

    // And the guard is released afterwards — a one-way latch would take the
    // whole module down after the first GC.
    double re = 0;
    double im = 0;
    int is_complex = 0;
    check(calc_api_recall("a", &re, &im, &is_complex, &err) == kCalcOk,
          "the guard clears once the outer call returns");
}

// ---- The stack-headroom guard ----

std::size_t g_last_need = 0;
int refuse_stack(std::size_t need) {
    g_last_need = need;
    return 0;
}
int allow_stack(std::size_t need) {
    g_last_need = need;
    return 1;
}

void test_stack_guard() {
    // On a board this is what stands between a script and the D48 hang:
    // calc.eval("solve(x^2-4,x,0,10)") reached solveexpr::substitute from
    // inside the VM and ran core 0's stack off the end of SCRATCH_Y.
    calc_api_set_stack_hook(&refuse_stack);
    g_last_need = 0;

    const EvalOut a = eval("1+1");
    check(a.status == kCalcFailed, "eval refuses when the stack is short");
    check(a.err != nullptr, "and says so");
    check(g_last_need > 0, "having asked for a specific amount");

    check(cas("diff", "x^2", "x").status == kCalcFailed, "so does a CAS binding");

    int count = 0;
    char packed[kCalcTextMax] = {};
    const char* err = nullptr;
    check(calc_api_solve("x^2-4=0", "x", &count, packed, sizeof(packed), &err) == kCalcFailed,
          "so does solve");

    // The solve() substitution asks for MORE than a plain eval, because
    // substitute() owns the deepest frame in the firmware.
    calc_api_set_stack_hook(&allow_stack);
    g_last_need = 0;
    eval("1+1");
    const std::size_t plain_need = g_last_need;
    eval("solve(x^2-4,x,0,10)");
    check(g_last_need > plain_need, "the solve path asks for more headroom than a plain eval");

    // No hook at all means no restriction — the host harness's own case.
    calc_api_set_stack_hook(nullptr);
    check(eval("1+1").status == kCalcOk, "with no hook installed, nothing is refused");
}

// ---- 6B.4: CAS bindings ----

void test_cas_bindings() {
    math::set_number_mode(math::NumberMode::kReal);

    const CasOut d = cas("diff", "x^3-2*x", "x");
    check(d.status == kCalcOk, "diff runs");
    check(d.kind == kCalcText, "and is symbolic");
    check(std::strstr(d.text, "x^2") != nullptr, "diff(x^3-2*x,x) mentions x^2");

    const CasOut i = cas("integ", "sin(x)", "x");
    check(i.status == kCalcOk, "integ runs");
    check(std::strstr(i.text, "cos") != nullptr, "integ(sin(x),x) mentions cos");

    const CasOut f = cas("factor", "x^2-4", "x");
    check(f.status == kCalcOk, "factor runs");
    check(std::strchr(f.text, '(') != nullptr, "and produces a product");

    const CasOut e = cas("expand", "(x+1)^2");
    check(e.status == kCalcOk, "expand runs with no variable argument");
    check(std::strstr(e.text, "x^2") != nullptr, "expand((x+1)^2) mentions x^2");

    const CasOut s = cas("simplify", "x+x");
    check(s.status == kCalcOk, "simplify runs");
    check(std::strchr(s.text, 'x') != nullptr, "and still mentions x");
}

void test_cas_definite_integral_is_a_number() {
    // The one CAS op whose result is a value rather than an expression. It
    // reaches Python as a float, which is what makes it usable.
    const CasOut a = cas("integ", "x", "x", "0", "2");
    check(a.status == kCalcOk, "a definite integral runs");
    check(a.kind == kCalcReal, "and comes back as a number");
    check_near(a.re, 2, "integ(x,x,0,2) == 2");

    // Bounds may be expressions, which is why they cross as text.
    const CasOut b = cas("integ", "sin(x)", "x", "0", "pi");
    check(b.status == kCalcOk, "a bound may be an expression");
    check(b.kind == kCalcReal, "still a number");
    check_near(b.re, 2, "integ(sin(x),x,0,pi) == 2");
}

void test_cas_variable_names_are_strict() {
    // Not pedantry: the variable is interpolated into a call string that the
    // CAS parser then reads, and solve_expr.cpp only ever looks at the first
    // character of its own variable argument.
    check(cas("diff", "x^2", "X").status == kCalcFailed, "an uppercase CAS variable is refused");
    check(cas("diff", "x^2", "xy").status == kCalcFailed, "a two-letter CAS variable is refused");
    check(cas("diff", "x^2", "x,y").status == kCalcFailed, "a variable cannot smuggle a comma");
    check(cas("diff", "", "x").status == kCalcFailed, "an empty expression is refused");
}

void test_solve_binding() {
    math::set_number_mode(math::NumberMode::kReal);
    int count = 0;
    char packed[kCalcTextMax] = {};
    const char* err = nullptr;

    check(calc_api_solve("x^2-4=0", "x", &count, packed, sizeof(packed), &err) == kCalcOk,
          "solve runs");
    check(count == 2, "x^2-4=0 has two solutions");
    const char* first = packed;
    const char* second = packed + std::strlen(first) + 1;
    check(std::strcmp(first, "-2") == 0 || std::strcmp(first, "2") == 0, "first solution is +-2");
    check(std::strcmp(second, "-2") == 0 || std::strcmp(second, "2") == 0,
          "second solution is +-2");
    check(std::strcmp(first, second) != 0, "and they differ");

    // One solution.
    check(calc_api_solve("x-3=0", "x", &count, packed, sizeof(packed), &err) == kCalcOk,
          "a linear solve runs");
    check(count == 1, "x-3=0 has one solution");
    check_str(packed, "3", "x == 3");

    // None, in REAL mode.
    count = -1;
    check(calc_api_solve("x^2+1=0", "x", &count, packed, sizeof(packed), &err) == kCalcFailed,
          "no real solutions is a failure, not an empty list");
    check(count == 0, "and the count is cleared");
}

void test_solve_is_complex_aware() {
    // The mode-dependent answer §4.2 advertises: the same equation, two
    // different correct results.
    math::set_number_mode(math::NumberMode::kRectangular);
    int count = 0;
    char packed[kCalcTextMax] = {};
    const char* err = nullptr;
    check(calc_api_solve("x^2+1=0", "x", &count, packed, sizeof(packed), &err) == kCalcOk,
          "x^2+1=0 solves in a+bi mode");
    check(count == 2, "with two solutions");
    const char* first = packed;
    const char* second = packed + std::strlen(first) + 1;
    check(std::strchr(first, 'i') != nullptr, "the first is imaginary");
    check(std::strchr(second, 'i') != nullptr, "so is the second");
    math::set_number_mode(math::NumberMode::kReal);
}

void test_solve_packing_refuses_to_truncate() {
    // A buffer too small to hold the set must fail rather than hand back a
    // shortened expression that still parses as something else.
    int count = 0;
    char tiny[4] = {};
    const char* err = nullptr;
    const CalcStatus st = calc_api_solve("x^2-4=0", "x", &count, tiny, sizeof(tiny), &err);
    check(st == kCalcFailed, "a too-small solution buffer fails");
    check(err != nullptr, "and says so");
}

// ---- 6B.6: graphing ----

int g_graph_persists = 0;
void count_graph_persist(CalcPersistTarget what, int) {
    if (what == kCalcPersistGraph) {
        ++g_graph_persists;
    }
}

int plot(const char* expr) {
    int slot = 0;
    const char* err = nullptr;
    return calc_api_plot(expr, &slot, &err) == kCalcOk ? slot : -1;
}

void test_plot_d68_semantics() {
    calc_api_set_persist_hook(&count_graph_persist);
    graph::GraphState& st = graph::state();

    // Something the user typed, which a script is about to destroy. This is
    // D68's known cost and the test says so out loud.
    std::snprintf(st.y.expr[3], config::kMaxExprLen, "%s", "x^2");
    st.y.enabled[3] = true;

    calc_api_begin_run();
    g_graph_persists = 0;

    check(plot("sin(x)") == 1, "the first plot of a run lands in Y1");
    check(st.y.expr[3][0] == 0, "and clears what the user had in Y4");
    check(st.y.enabled[0], "Y1 is enabled");
    check(std::strcmp(st.y.expr[0], "sin(x)") == 0, "with the script's expression");
    check(g_graph_persists == 1, "and it is persisted");

    check(plot("cos(x)") == 2, "the second appends to Y2");
    check(plot("tan(x)") == 3, "the third to Y3");
    check(std::strcmp(st.y.expr[0], "sin(x)") == 0, "without disturbing Y1");

    // A new run starts clean, which is the whole point of the latch.
    calc_api_begin_run();
    check(plot("x") == 1, "the next run's first plot clears and starts at Y1 again");
    check(st.y.expr[1][0] == 0, "so Y2 from the previous run is gone");

    calc_api_set_persist_hook(nullptr);
}

void test_plot_eighth_fails() {
    calc_api_begin_run();
    for (int i = 1; i <= 7; ++i) {
        check(plot("x") == i, "seven plots fill Y1-Y7");
    }
    int slot = 0;
    const char* err = nullptr;
    check(calc_api_plot("x", &slot, &err) == kCalcFailed, "the eighth fails");
    check(err != nullptr, "and says why");
    check(graph::state().y.expr[6][0] != 0, "leaving the seven in place");
}

void test_plot_rejects_junk() {
    calc_api_begin_run();
    int slot = 0;
    const char* err = nullptr;
    check(calc_api_plot("", &slot, &err) == kCalcFailed, "an empty expression is refused");
    check(calc_api_plot(nullptr, &slot, &err) == kCalcFailed, "so is a null one");

    char huge[config::kMaxExprLen + 8];
    std::memset(huge, 'x', sizeof(huge) - 1);
    huge[sizeof(huge) - 1] = 0;
    check(calc_api_plot(huge, &slot, &err) == kCalcFailed,
          "an over-long expression is refused, not truncated into the slot");
}

void test_plot_forces_function_mode() {
    graph::state().mode = graph::Mode::kPolar;
    calc_api_begin_run();
    plot("sin(x)");
    check(graph::state().mode == graph::Mode::kFunction,
          "plotting switches to FUNC, or the graph would come up blank");
}

void test_window() {
    const char* err = nullptr;
    check(calc_api_window(-5, 5, -2, 2, &err) == kCalcOk, "a valid window is accepted");
    check_near(graph::state().window.x_min, -5, "x_min");
    check_near(graph::state().window.y_max, 2, "y_max");

    check(calc_api_window(5, -5, -2, 2, &err) == kCalcFailed, "an inverted x range is refused");
    check(calc_api_window(-5, 5, 2, 2, &err) == kCalcFailed, "an empty y range is refused");
    check_near(graph::state().window.x_min, -5, "and the window is unchanged");
}

void test_show_graph_is_a_latch() {
    calc_api_begin_run();
    check(calc_api_take_show_graph() == 0, "nothing requested at the start of a run");
    calc_api_show_graph();
    check(calc_api_take_show_graph() == 1, "the request is visible once");
    check(calc_api_take_show_graph() == 0, "and reading it clears it");

    calc_api_show_graph();
    calc_api_begin_run();
    check(calc_api_take_show_graph() == 0, "a new run clears a stale request");
}

void test_graph_analysis() {
    calc_api_set_stack_hook(nullptr);
    calc_api_begin_run();
    calc_api_window(-10, 10, -10, 10, nullptr);
    check(plot("x^2-4") == 1, "plot the test function");

    double a = 0;
    double b = 0;
    int two = 0;
    const char* err = nullptr;

    check(calc_api_graph_analyze("zero", 1, 0, 5, &a, &b, &two, &err) == kCalcOk, "zero runs");
    check(two == 1, "and reports a point");
    check_close(a, 2, 1e-9, "x^2-4 has a root at x=2");

    check(calc_api_graph_analyze("min", 1, -5, 5, &a, &b, &two, &err) == kCalcOk, "min runs");
    check_close(a, 0, 1e-6, "the minimum is at x=0");
    check_close(b, -4, 1e-9, "where y is -4");

    check(calc_api_graph_analyze("value", 1, 3, 3, &a, &b, &two, &err) == kCalcOk, "value runs");
    check_near(b, 5, "x^2-4 at x=3 is 5");

    check(calc_api_graph_analyze("deriv", 1, 2, 2, &a, &b, &two, &err) == kCalcOk, "deriv runs");
    check(two == 0, "and reports a single number");
    check_near(a, 4, "d/dx(x^2-4) at x=2 is 4");

    check(calc_api_graph_analyze("integral", 1, 0, 3, &a, &b, &two, &err) == kCalcOk,
          "integral runs");
    check_near(a, 9.0 - 12.0, "integral of x^2-4 from 0 to 3 is -3");

    // Rejections.
    check(calc_api_graph_analyze("zero", 9, 0, 5, &a, &b, &two, &err) == kCalcFailed,
          "slot 9 is refused");
    check(calc_api_graph_analyze("zero", 0, 0, 5, &a, &b, &two, &err) == kCalcFailed,
          "slot 0 is refused");
    check(calc_api_graph_analyze("nope", 1, 0, 5, &a, &b, &two, &err) == kCalcFailed,
          "an unknown op is refused");
    check(calc_api_graph_analyze("zero", 5, 0, 5, &a, &b, &two, &err) == kCalcFailed,
          "an empty slot is refused");
}

void test_graph_analysis_is_stack_guarded() {
    // The deepest binding there is: analyze_integral recurses through
    // integrate_panel to depth 12.
    calc_api_set_stack_hook(&refuse_stack);
    double a = 0;
    double b = 0;
    int two = 0;
    const char* err = nullptr;
    check(calc_api_graph_analyze("integral", 1, 0, 3, &a, &b, &two, &err) == kCalcFailed,
          "graph analysis refuses when the stack is short");
    calc_api_set_stack_hook(nullptr);
}

// ---- 6B.17: lists ----

int g_list_saves = 0;
int g_matrix_saves = 0;
void count_data_persist(CalcPersistTarget what, int index) {
    (void)index;
    if (what == kCalcPersistList) {
        ++g_list_saves;
    } else if (what == kCalcPersistMatrix) {
        ++g_matrix_saves;
    }
}

void set_list(int n, const double* v, int count) {
    const char* err = nullptr;
    check(calc_api_list_resize(n, count, &err) == kCalcOk, "resize the list");
    if (count > 0) {
        check(calc_api_list_write(n, 0, count, v, &err) == kCalcOk, "fill it");
    }
}

void test_list_round_trip() {
    const char* err = nullptr;
    const double v[] = {1.5, 2.5, 3.5};
    calc_api_begin_run();
    set_list(1, v, 3);

    int n = 0;
    check(calc_api_list_size(1, &n, &err) == kCalcOk, "size reads back");
    check(n == 3, "three elements");

    double out[3] = {};
    check(calc_api_list_read(1, 0, 3, out, &err) == kCalcOk, "read them back");
    check_near(out[0], 1.5, "l1[0]");
    check_near(out[2], 3.5, "l1[2]");

    // An empty list is legal, and reading zero from it must not fail.
    check(calc_api_list_resize(1, 0, &err) == kCalcOk, "resize to empty");
    check(calc_api_list_size(1, &n, &err) == kCalcOk && n == 0, "and it is empty");
}

void test_list_append_grows() {
    const char* err = nullptr;
    calc_api_begin_run();
    check(calc_api_list_resize(2, 0, &err) == kCalcOk, "start empty");
    for (int i = 0; i < 50; ++i) {
        check(calc_api_list_append(2, i * 2.0, &err) == kCalcOk, "append");
    }
    int n = 0;
    calc_api_list_size(2, &n, &err);
    check(n == 50, "fifty appends give fifty elements");
    double out[2] = {};
    calc_api_list_read(2, 49, 1, out, &err);
    check_near(out[0], 98, "the last one is right");
}

void test_list_persistence_is_deferred() {
    // D82: a logging loop must not cost one SD write per sample.
    calc_api_set_persist_hook(&count_data_persist);
    calc_api_begin_run();
    g_list_saves = 0;

    const char* err = nullptr;
    calc_api_list_resize(3, 0, &err);
    for (int i = 0; i < 100; ++i) {
        calc_api_list_append(3, i, &err);
    }
    check(g_list_saves == 0, "100 appends save nothing");

    calc_api_flush_run();
    check(g_list_saves == 1, "the flush saves the touched list exactly once");

    // A second flush with nothing new must not re-save.
    calc_api_flush_run();
    check(g_list_saves == 1, "and does not repeat itself");

    // Two lists touched, two saves.
    calc_api_begin_run();
    g_list_saves = 0;
    calc_api_list_append(1, 1, &err);
    calc_api_list_append(4, 2, &err);
    calc_api_flush_run();
    check(g_list_saves == 2, "two lists touched, two saves");

    calc_api_set_persist_hook(nullptr);
}

void test_list_indices_are_checked() {
    const char* err = nullptr;
    int n = 0;
    check(calc_api_list_size(0, &n, &err) == kCalcFailed, "list 0 is refused");
    check(calc_api_list_size(7, &n, &err) == kCalcFailed, "list 7 is refused");
    check(err != nullptr, "and says the range");
    check(calc_api_list_append(0, 1, &err) == kCalcFailed, "append checks too");

    double buf[4] = {};
    calc_api_begin_run();
    set_list(1, buf, 2);
    check(calc_api_list_read(1, 0, 5, buf, &err) == kCalcFailed, "reading past the end fails");
    check(calc_api_list_read(1, -1, 1, buf, &err) == kCalcFailed, "so does a negative index");
}

void test_list_stats() {
    const char* err = nullptr;
    const double v[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    calc_api_begin_run();
    set_list(5, v, 8);

    double x = 0;
    check(calc_api_list_stat(5, "mean", &x, &err) == kCalcOk, "mean runs");
    check_near(x, 5.0, "mean of the classic 8-sample set");
    check(calc_api_list_stat(5, "sum", &x, &err) == kCalcOk && std::fabs(x - 40) < 1e-9, "sum");
    check(calc_api_list_stat(5, "min", &x, &err) == kCalcOk && std::fabs(x - 2) < 1e-9, "min");
    check(calc_api_list_stat(5, "max", &x, &err) == kCalcOk && std::fabs(x - 9) < 1e-9, "max");
    check(calc_api_list_stat(5, "n", &x, &err) == kCalcOk && std::fabs(x - 8) < 1e-9, "n");
    // Sample (n-1) standard deviation, which is TI's Sx.
    check(calc_api_list_stat(5, "stddev", &x, &err) == kCalcOk, "stddev runs");
    check_close(x, 2.13808993529939, 1e-9, "sample stddev");

    check(calc_api_list_stat(5, "nope", &x, &err) == kCalcFailed, "an unknown statistic fails");
    calc_api_list_resize(6, 0, &err);
    check(calc_api_list_stat(6, "mean", &x, &err) == kCalcFailed, "so does an empty list");
}

// ---- 6B.7: matrices ----

CalcStatus put_matrix(const double* v, int rows, int cols) {
    const char* err = nullptr;
    CalcStatus st = calc_api_mat_begin(rows, cols, &err);
    for (int r = 0; r < rows && st == kCalcOk; ++r) {
        st = calc_api_mat_write_row(r, cols, v + r * cols, &err);
    }
    return st;
}

void test_matrix_ops() {
    const char* err = nullptr;
    const double m[] = {1, 2, 3, 4};
    double scalar = 0;
    int rows = 0;
    int cols = 0;

    check(put_matrix(m, 2, 2) == kCalcOk, "a 2x2 goes in");
    check(calc_api_mat_op("det", -1, &scalar, &rows, &cols, &err) == kCalcOk, "det runs");
    check_near(scalar, -2, "det([[1,2],[3,4]]) == -2");
    check(rows == 0, "and reports a scalar, not a shape");

    check(put_matrix(m, 2, 2) == kCalcOk, "again");
    check(calc_api_mat_op("inverse", -1, &scalar, &rows, &cols, &err) == kCalcOk, "inverse runs");
    check(rows == 2 && cols == 2, "and is 2x2");
    double row[2] = {};
    calc_api_mat_read_row(0, 2, row, &err);
    check_near(row[0], -2, "inverse[0][0]");
    check_near(row[1], 1, "inverse[0][1]");
    calc_api_mat_read_row(1, 2, row, &err);
    check_near(row[0], 1.5, "inverse[1][0]");
    check_near(row[1], -0.5, "inverse[1][1]");

    // A singular matrix is 0, NOT an error — matrix.hpp says so explicitly,
    // and a binding that raised here would be inventing a failure.
    const double sing[] = {1, 2, 2, 4};
    check(put_matrix(sing, 2, 2) == kCalcOk, "a singular 2x2 goes in");
    check(calc_api_mat_op("det", -1, &scalar, &rows, &cols, &err) == kCalcOk,
          "det of a singular matrix succeeds");
    check_near(scalar, 0, "and is zero");

    // Non-square inverse must fail.
    const double wide[] = {1, 2, 3, 4, 5, 6};
    check(put_matrix(wide, 2, 3) == kCalcOk, "a 2x3 goes in");
    check(calc_api_mat_op("inverse", -1, &scalar, &rows, &cols, &err) == kCalcFailed,
          "inverting a non-square fails");

    check(put_matrix(wide, 2, 3) == kCalcOk, "again");
    check(calc_api_mat_op("transpose", -1, &scalar, &rows, &cols, &err) == kCalcOk, "transpose");
    check(rows == 3 && cols == 2, "2x3 transposes to 3x2");
}

void test_matrix_eigenvalues() {
    const char* err = nullptr;
    // [[2,1],[1,2]] has eigenvalues 3 and 1.
    const double sym[] = {2, 1, 1, 2};
    double scalar = 0;
    int rows = 0;
    int cols = 0;
    check(put_matrix(sym, 2, 2) == kCalcOk, "a symmetric 2x2 goes in");
    check(calc_api_mat_op("eigenvalues", -1, &scalar, &rows, &cols, &err) == kCalcOk,
          "eigenvalues runs");
    check(rows * cols == 2, "two of them");
    // eigenvalues() returns them as an Array; read every row so the test does
    // not depend on whether it came back as 1x2 or 2x1.
    double vals[2] = {};
    int seen = 0;
    for (int r = 0; r < rows && seen < 2; ++r) {
        double buf[2] = {};
        calc_api_mat_read_row(r, cols, buf, &err);
        for (int c = 0; c < cols && seen < 2; ++c) {
            vals[seen++] = buf[c];
        }
    }
    const double hi = vals[0] > vals[1] ? vals[0] : vals[1];
    const double lo = vals[0] > vals[1] ? vals[1] : vals[0];
    check_close(hi, 3.0, 1e-6, "the larger eigenvalue is 3");
    check_close(lo, 1.0, 1e-6, "the smaller is 1");
}

void test_matrix_slots() {
    calc_api_set_persist_hook(&count_data_persist);
    calc_api_begin_run();
    g_matrix_saves = 0;
    const char* err = nullptr;
    const double m[] = {5, 6, 7, 8};

    check(put_matrix(m, 2, 2) == kCalcOk, "a matrix goes in");
    check(calc_api_mat_store(0, &err) == kCalcOk, "store it to [A]");
    check(g_matrix_saves == 0, "which does not save immediately");
    calc_api_flush_run();
    check(g_matrix_saves == 1, "the flush saves it once");

    int rows = 0;
    int cols = 0;
    check(calc_api_mat_load(0, &rows, &cols, &err) == kCalcOk, "load [A] back");
    check(rows == 2 && cols == 2, "with its shape");
    double scalar = 0;
    check(calc_api_mat_op("det", -1, &scalar, &rows, &cols, &err) == kCalcOk, "det of it");
    check_near(scalar, -2, "det([[5,6],[7,8]]) == -2");

    check(calc_api_mat_store(10, &err) == kCalcFailed, "slot 10 is refused");
    check(calc_api_mat_load(-1, &rows, &cols, &err) == kCalcFailed, "slot -1 is refused");
    calc_api_set_persist_hook(nullptr);
}

void test_matrix_rejects_junk() {
    const char* err = nullptr;
    double scalar = 0;
    int rows = 0;
    int cols = 0;
    check(calc_api_mat_begin(0, 2, &err) == kCalcFailed, "zero rows is refused");
    check(calc_api_mat_begin(100, 2, &err) == kCalcFailed, "past kMaxDim is refused");

    check(calc_api_mat_begin(2, 2, &err) == kCalcOk, "a fresh 2x2");
    const double r[] = {1, 2, 3};
    check(calc_api_mat_write_row(0, 3, r, &err) == kCalcFailed,
          "a row of the wrong width is refused, not truncated");
    check(calc_api_mat_write_row(5, 2, r, &err) == kCalcFailed, "so is a row past the end");

    const double m[] = {1, 2, 3, 4};
    check(put_matrix(m, 2, 2) == kCalcOk, "a good one");
    check(calc_api_mat_op("nope", -1, &scalar, &rows, &cols, &err) == kCalcFailed,
          "an unknown op is refused");
}

void test_matrix_eigen_is_stack_guarded() {
    calc_api_set_stack_hook(&refuse_stack);
    const char* err = nullptr;
    double scalar = 0;
    int rows = 0;
    int cols = 0;
    const double m[] = {2, 1, 1, 2};
    put_matrix(m, 2, 2);
    check(calc_api_mat_op("eigenvalues", -1, &scalar, &rows, &cols, &err) == kCalcFailed,
          "eigenvalues refuses when the stack is short");
    check(calc_api_mat_op("det", -1, &scalar, &rows, &cols, &err) == kCalcFailed,
          "so does an ordinary matrix op");
    calc_api_set_stack_hook(nullptr);
}

// ---- 6B.5: complex ----

void test_complex_helpers() {
    check_near(calc_api_c_abs(3, 4), 5, "|3+4i| == 5");
    check_near(calc_api_c_abs(-3, -4), 5, "|-3-4i| == 5");
    check_near(calc_api_c_abs(0, 0), 0, "|0| == 0");

    // Radians, always, in -pi..pi — independent of angle mode, which is the
    // reason these exist rather than deferring to Python's builtins.
    math::set_angle_mode(math::AngleMode::kDegrees);
    check_near(calc_api_c_arg(0, 1), 1.5707963267948966, "arg(i) is radians even in DEG mode");
    math::set_angle_mode(math::AngleMode::kRadians);
    check_near(calc_api_c_arg(1, 0), 0, "arg(1) == 0");
    check_near(calc_api_c_arg(-1, 0), 3.141592653589793, "arg(-1) == pi");

    double re = 0;
    double im = 0;
    calc_api_c_conj(3, 2, &re, &im);
    check_near(re, 3, "conj keeps the real part");
    check_near(im, -2, "and negates the imaginary one");
}

}  // namespace

int main() {
    test_eval_scalar();
    test_eval_rejects_junk();
    test_eval_complex();
    test_eval_non_scalar_is_text();
    test_eval_reaches_the_cas_stage();
    test_eval_reaches_the_solve_stage();
    test_eval_reaches_the_convert_stage();
    test_eval_solve_set_renders_like_the_home_screen();

    test_store_and_recall();
    test_store_complex_round_trip();
    test_variable_names_are_strict();
    test_reentrancy_guard();
    test_stack_guard();

    test_cas_bindings();
    test_cas_definite_integral_is_a_number();
    test_cas_variable_names_are_strict();
    test_solve_binding();
    test_solve_is_complex_aware();
    test_solve_packing_refuses_to_truncate();

    test_complex_helpers();

    test_list_round_trip();
    test_list_append_grows();
    test_list_persistence_is_deferred();
    test_list_indices_are_checked();
    test_list_stats();

    test_matrix_ops();
    test_matrix_eigenvalues();
    test_matrix_slots();
    test_matrix_rejects_junk();
    test_matrix_eigen_is_stack_guarded();

    test_plot_d68_semantics();
    test_plot_eighth_fails();
    test_plot_rejects_junk();
    test_plot_forces_function_mode();
    test_window();
    test_show_graph_is_a_latch();
    test_graph_analysis();
    test_graph_analysis_is_stack_guarded();

    std::printf("test_calc_api: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
