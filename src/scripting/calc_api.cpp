#include "scripting/calc_api.h"

#include <cstdio>
#include <cstring>

#include "config.hpp"
#include "math/cas/cas_eval.hpp"
#include "math/cas/expr.hpp"
#include "math/cas/serialize.hpp"
#include "math/complex.hpp"
#include "math/engine.hpp"
#include "math/solve_expr.hpp"
#include "math/types.hpp"
#include "math/unified_home.hpp"
#include "math/units.hpp"
#include "math/var_store.hpp"
#include "graph/analysis.hpp"
#include "graph/graph_state.hpp"

// The C++ side of the `calc` module (Phase 6B.3-6B.5). Every function here is
// a LEAF as far as MicroPython is concerned: it is called from
// mp_calc_module.c and never calls back, so no longjmp can pass through this
// file. calc_api.h says why that matters.
//
// It also means this translation unit depends on nothing but math/, which is
// what lets tests/host/test_calc_api.cpp exercise the pipeline, the variable
// name rules and the reentrancy guard with no interpreter and no hardware.

namespace {

// The reentrancy guard (phase6-spec.md §4.7 point 3). math::unified's
// compile()/run() share bss state — one Program buffer, one 64-slot operand
// stack — and a second run() started while the first is live corrupts it
// silently rather than failing. Nothing in the expression language can
// re-enter, but MicroPython's GC can: an allocation inside a binding may
// collect, a __del__ finalizer may run arbitrary Python during the collection,
// and that Python can call calc.eval again.
//
// A flag rather than an RAII guard, and set in one wrapper per entry point
// rather than inline, so "cleared on every path" is checkable by looking at
// one line instead of every return.
bool g_in_call = false;

CalcPersistFn g_persist = nullptr;
CalcStackRoomFn g_stack_room = nullptr;

// D68's per-run latch: has this script plotted anything yet? False at the
// start of every top-level exec(), which is what makes a script's graph
// depend on the script and not on what the previous one left in Y1-Y7.
bool g_plotted_this_run = false;

// calc.show_graph() sets this; the screen that ran the script acts on it
// once exec() returns. See calc_api.h for why it is not immediate.
bool g_show_graph_requested = false;

void persist(CalcPersistTarget what) {
    if (g_persist != nullptr) {
        g_persist(what);
    }
}

// Stack a path needs below the binding, in bytes. Both come from the
// `stack: peak N of 4096` instrument on hardware, not from adding up frames —
// D47 and D48 are two occasions when the arithmetic was wrong and the board
// was right.
//
// kEvalStackNeed covers the CAS probe and the unified evaluator, which is
// every calc.eval() call. kSolveStackNeed additionally covers
// solveexpr::substitute -> numeric_solve -> tinyexpr; substitute still owns
// the deepest frame in the firmware even after its argument buffers moved to
// bss, so the solve path is checked separately rather than making every
// eval() pay for it.
// Measured on the Pico 1, 2026-08-15, with -DPICOCALC_STACK_PROBE=ON (which
// reports free stack at each binding) and the `stack: peak` instrument. From
// a top-level `py` line the VM leaves **2,239 bytes** free at the binding:
//
//   path                            peak of 4,096   consumed below the binding
//   calc.eval("1+1")                     2,848            991
//   calc.diff / factor / solve            3,240          1,279
//   calc.eval("solve(f,x,lo,hi)")         3,544          1,687
//
// The requirements are those plus ~320 bytes, which is the margin fault.cpp's
// kLiveMargin already assumes an ISR frame can want.
//
// What this costs, measured rather than estimated: a plain calc.eval works at
// top level (peak 2,828) and inside ONE function (peak 3,412, 684 B spare),
// and is refused inside two. The solve() path needs another ~400 bytes below
// that, so it is effectively top-level only. A clean exception is the whole
// point — running it anyway was measured, the same day, to hang the board.
constexpr std::size_t kEvalStackNeed = 1600;
constexpr std::size_t kSolveStackNeed = 2000;

// Graph analysis is measured on its own because analyze_integral recurses:
// integrate_panel is 136 bytes a frame with a depth cap of 12, so the
// arithmetic says 1,632 bytes of recursion before the expression evaluator
// underneath it.
//
// The arithmetic is a floor, and the measurement is what counts. A shallow
// integrand hides the problem entirely — integral of x^2-4 peaked at 2,488 of
// 4,096. The number below comes from one that actually subdivides:
//
//   calc.graph_integral("Y1", 0.01, 1) on sin(1/x)   peak 3,532   consumed 1,675
//
// 2,000 is that plus ~325, and it fits the 2,239 bytes a binding has. Anything
// added to 6B.7-6B.10 gets its own measurement against an input chosen to be
// hostile, not a convenient one.
constexpr std::size_t kAnalysisStackNeed = 2000;

// True when the path is safe to enter. No hook installed means yes: the host
// test harness runs on a stack where none of this is a question.
bool stack_room(std::size_t need) {
    return g_stack_room == nullptr || g_stack_room(need) != 0;
}

// A variable reference resolved from a Python-supplied name.
struct VarRef {
    int index = -1;
    const char* err = nullptr;
};

// Exactly one character 'a'..'z', or "theta"/"ans". Everything else is an
// error, deliberately: math::Variables::operator[] maps an unrecognized name
// to Ans (engine.hpp:27) and solve_expr.cpp:92 reads only the first character
// of its variable argument, so both would accept calc.store("A", 1) and do
// something the caller did not ask for.
VarRef resolve_var(const char* name) {
    VarRef ref;
    if (name == nullptr || name[0] == 0) {
        ref.err = "Variable name is empty";
        return ref;
    }
    if (std::strcmp(name, "theta") == 0) {
        ref.index = math::Variables::kTheta;
        return ref;
    }
    if (std::strcmp(name, "ans") == 0) {
        ref.index = math::Variables::kAns;
        return ref;
    }
    if (name[1] != 0 || name[0] < 'a' || name[0] > 'z') {
        ref.err = "Variable must be a-z, theta or ans";
        return ref;
    }
    ref.index = name[0] - 'a';
    return ref;
}

// The same check, for a CAS operation's variable argument. Narrower than
// resolve_var: theta and ans are not things you differentiate with respect to.
bool valid_cas_var(const char* var, const char** err) {
    if (var == nullptr) {
        return true;  // the op's own default ('x') applies
    }
    if (var[0] < 'a' || var[0] > 'z' || var[1] != 0) {
        *err = "Variable must be a-z";
        return false;
    }
    return true;
}

// Copy `src` into a caller-provided buffer, failing rather than truncating.
bool copy_out(const char* src, char* out, size_t out_cap, const char** err) {
    if (out == nullptr || out_cap == 0) {
        *err = "No output buffer";
        return false;
    }
    const size_t len = std::strlen(src);
    if (len + 1 > out_cap) {
        *err = "Result too long";
        return false;
    }
    std::memcpy(out, src, len + 1);
    return true;
}

// A CAS result as either a number or text. A bare numeric literal becomes a
// Python float, which is what a definite integral or a constant-folded
// simplify() should give back; anything else is serialized NOW, because cas
// Expr nodes only live until the next top-level CAS operation
// (cas_eval.hpp) and the caller may well run one.
//
// The negation check is not pedantry: the parser builds -2 as kNeg over
// kNum(2), so is_num() alone would send half the constants down the text path.
bool cas_result(const math::cas::Expr* e, CalcKind* kind, double* re, char* out, size_t out_cap,
                const char** err) {
    if (e == nullptr) {
        *err = "No result";
        return false;
    }
    if (e->is_num()) {
        *kind = kCalcReal;
        *re = e->num_val;
        return true;
    }
    if (e->is_neg() && e->child != nullptr && e->child->is_num()) {
        *kind = kCalcReal;
        *re = -e->child->num_val;
        return true;
    }
    char buf[kCalcTextMax];
    const size_t len = math::cas::expr_to_string(e, buf, sizeof(buf));
    if (len + 1 >= sizeof(buf)) {
        *err = "Result too long";  // truncated: see the note in solve_impl
        return false;
    }
    *kind = kCalcText;
    return copy_out(buf, out, out_cap, err);
}

// ---- Implementations, all called with the guard already held ----

CalcStatus eval_impl(const char* expr, CalcKind* kind, double* re, double* im, char* text,
                     size_t text_cap, const char** err) {
    if (expr == nullptr || expr[0] == 0) {
        *err = "Empty expression";
        return kCalcFailed;
    }
    if (!stack_room(kEvalStackNeed)) {
        *err = "Not enough stack for eval";
        return kCalcFailed;
    }
    *im = 0;

    // The four steps HomeScreen::evaluate_input runs, in its order. All four
    // are standalone math:: functions, which is what phase6-spec.md §4.7
    // established: calc.eval reproduces the home screen's pipeline by calling
    // the same things, not by re-deriving one.

    // 1. Inline CAS (home_screen.cpp:430). Returns kNone for anything that is
    //    not a recognized CAS call, including solve() carrying numeric bounds,
    //    which belongs to step 2.
    const bool allow_complex = math::number_mode() != math::NumberMode::kReal;
    const math::cas::HomeResult cr = math::cas::evaluate_home(expr, allow_complex);
    if (cr.kind == math::cas::HomeKind::kError) {
        *err = cr.error;
        return kCalcFailed;
    }
    if (cr.kind == math::cas::HomeKind::kExpr) {
        return cas_result(cr.result, kind, re, text, text_cap, err) ? kCalcOk : kCalcFailed;
    }
    if (cr.kind == math::cas::HomeKind::kSolutions) {
        // "x = {-2,2}", the shape the home screen shows (home_screen.cpp:441).
        // calc.solve() is the binding that hands back a real Python list;
        // eval() of a solve() call reproduces what the user would see typing
        // the same thing.
        char buf[kCalcTextMax];
        size_t w = 0;
        const int n = std::snprintf(buf, sizeof(buf), "%c = {", cr.var);
        w += n > 0 ? static_cast<size_t>(n) : 0;
        for (int i = 0; i < cr.count && w + 2 < sizeof(buf); ++i) {
            if (i > 0) {
                buf[w++] = ',';
            }
            w += math::cas::expr_to_string(cr.solutions[i], buf + w, sizeof(buf) - w);
        }
        if (w + 1 < sizeof(buf)) {
            buf[w++] = '}';
        }
        buf[w] = 0;
        *kind = kCalcText;
        return copy_out(buf, text, text_cap, err) ? kCalcOk : kCalcFailed;
    }

    // 2/3. solve() and convert() calls become numeric literals, in that order
    //      (home_screen.cpp:471 and :483). Both rewrite in place.
    char buf[kCalcTextMax];
    if (std::snprintf(buf, sizeof(buf), "%s", expr) >= static_cast<int>(sizeof(buf))) {
        *err = "Expression too long";
        return kCalcFailed;
    }
    if (math::solveexpr::contains_solve(buf)) {
        // Checked separately, and before the call rather than inside it:
        // substitute() has the deepest frame in the firmware and its callees
        // are recursive. Refusing here is a Python exception; not refusing was
        // measured, on 2026-08-15, to be a hang.
        if (!stack_room(kSolveStackNeed)) {
            *err = "Not enough stack for solve()";
            return kCalcFailed;
        }
        if (!math::solveexpr::substitute(buf, sizeof(buf), err)) {
            return kCalcFailed;
        }
    }
    if (math::unitexpr::contains_convert(buf) &&
        !math::unitexpr::substitute(buf, sizeof(buf), err)) {
        return kCalcFailed;
    }

    // 4. The unified evaluator (home_screen.cpp:525). to_frac is false: a
    //    script wants a value, and ">frac" is a display suffix.
    const math::unified::HomeResult ur = math::unified::evaluate_home(buf, false);
    if (ur.kind == math::unified::HomeKind::kError) {
        *err = ur.error;
        return kCalcFailed;
    }
    if (ur.kind != math::unified::HomeKind::kScalar) {
        // List, matrix, or "Done (n lists)". evaluate_home hands back
        // FORMATTED TEXT, never a live Value, so there is no Array* whose
        // lifetime could outlast the next run() — the hazard §4.7 point 2
        // flagged is avoided by construction rather than by copying carefully.
        *kind = kCalcText;
        return copy_out(ur.text, text, text_cap, err) ? kCalcOk : kCalcFailed;
    }

    // HomeResult::scalar_value is the REAL PART ONLY (unified_home.cpp:134).
    // The imaginary part is recoverable from Ans, which the VM writes for
    // every scalar result, store or no store (unified_eval.hpp:207), using
    // set_real when the result is real — so a stale imaginary part from an
    // earlier evaluation cannot be mistaken for this one's.
    const math::Variables& vars = math::engine().vars();
    if (vars.is_complex(math::Variables::kAns)) {
        *kind = kCalcComplex;
        *re = vars.vars[math::Variables::kAns];
        *im = vars.imag[math::Variables::kAns];
    } else {
        *kind = kCalcReal;
        *re = ur.scalar_value;
    }
    return kCalcOk;
}

CalcStatus store_impl(const char* name, double re, double im, const char** err) {
    const VarRef ref = resolve_var(name);
    if (ref.index < 0) {
        *err = ref.err;
        return kCalcFailed;
    }
    math::Variables& vars = math::engine().vars();
    if (im == 0) {
        vars.set_real(ref.index, re);  // clears the imaginary part
    } else {
        vars.set_complex(ref.index, re, im);
    }
    persist(kCalcPersistVars);
    return kCalcOk;
}

CalcStatus recall_impl(const char* name, double* re, double* im, int* is_complex,
                       const char** err) {
    const VarRef ref = resolve_var(name);
    if (ref.index < 0) {
        *err = ref.err;
        return kCalcFailed;
    }
    const math::Variables& vars = math::engine().vars();
    *re = vars.vars[ref.index];
    *im = vars.imag[ref.index];
    *is_complex = vars.is_complex(ref.index) ? 1 : 0;
    return kCalcOk;
}

// Compose the call syntax math::cas::evaluate_home already parses, rather than
// reaching into derivative.cpp / integrate.cpp / factor.cpp separately. One
// CAS entry point means the bindings inherit its argument rules, its
// simplification and its error messages instead of drifting from them, and
// each binding costs a snprintf.
CalcStatus cas_impl(const char* op, const char* expr, const char* var, const char* arg3,
                    const char* arg4, CalcKind* kind, double* re, char* out, size_t out_cap,
                    const char** err) {
    if (expr == nullptr || expr[0] == 0) {
        *err = "Empty expression";
        return kCalcFailed;
    }
    if (!valid_cas_var(var, err)) {
        return kCalcFailed;
    }
    if (!stack_room(kEvalStackNeed)) {
        *err = "Not enough stack for CAS";
        return kCalcFailed;
    }
    char call[kCalcTextMax];
    int n = 0;
    if (var == nullptr) {
        n = std::snprintf(call, sizeof(call), "%s(%s)", op, expr);
    } else if (arg3 == nullptr) {
        n = std::snprintf(call, sizeof(call), "%s(%s,%s)", op, expr, var);
    } else if (arg4 == nullptr) {
        n = std::snprintf(call, sizeof(call), "%s(%s,%s,%s)", op, expr, var, arg3);
    } else {
        n = std::snprintf(call, sizeof(call), "%s(%s,%s,%s,%s)", op, expr, var, arg3, arg4);
    }
    if (n < 0 || n >= static_cast<int>(sizeof(call))) {
        *err = "Expression too long";
        return kCalcFailed;
    }

    const bool allow_complex = math::number_mode() != math::NumberMode::kReal;
    const math::cas::HomeResult cr = math::cas::evaluate_home(call, allow_complex);
    if (cr.kind == math::cas::HomeKind::kError) {
        *err = cr.error;
        return kCalcFailed;
    }
    if (cr.kind != math::cas::HomeKind::kExpr) {
        // kNone means evaluate_home did not recognize the call at all, which
        // at this point is a malformed argument rather than a wrong op name.
        *err = cr.kind == math::cas::HomeKind::kNone ? "Bad argument" : "Wrong result kind";
        return kCalcFailed;
    }
    return cas_result(cr.result, kind, re, out, out_cap, err) ? kCalcOk : kCalcFailed;
}

CalcStatus solve_impl(const char* expr, const char* var, int* count, char* out, size_t out_cap,
                      const char** err) {
    *count = 0;
    if (expr == nullptr || expr[0] == 0) {
        *err = "Empty expression";
        return kCalcFailed;
    }
    if (!valid_cas_var(var, err)) {
        return kCalcFailed;
    }
    if (!stack_room(kEvalStackNeed)) {
        *err = "Not enough stack for solve";
        return kCalcFailed;
    }
    char call[kCalcTextMax];
    const int n = var == nullptr ? std::snprintf(call, sizeof(call), "solve(%s)", expr)
                                 : std::snprintf(call, sizeof(call), "solve(%s,%s)", expr, var);
    if (n < 0 || n >= static_cast<int>(sizeof(call))) {
        *err = "Expression too long";
        return kCalcFailed;
    }
    const bool allow_complex = math::number_mode() != math::NumberMode::kReal;
    const math::cas::HomeResult cr = math::cas::evaluate_home(call, allow_complex);
    if (cr.kind == math::cas::HomeKind::kError) {
        *err = cr.error;
        return kCalcFailed;
    }
    if (cr.kind != math::cas::HomeKind::kSolutions) {
        *err = "No solution";
        return kCalcFailed;
    }

    // Pack every solution now, while the pool is still ours — see calc_api.h.
    const int n_sol = cr.count < kCalcMaxSolutions ? cr.count : kCalcMaxSolutions;
    size_t w = 0;
    for (int i = 0; i < n_sol; ++i) {
        if (w >= out_cap) {
            *err = "Result too long";
            return kCalcFailed;
        }
        const size_t len = math::cas::expr_to_string(cr.solutions[i], out + w, out_cap - w);
        // expr_to_string TRUNCATES rather than failing, and reports the
        // truncated length, so "it fit exactly" and "it was cut off" look
        // identical from here. Rejecting both beats handing Python a silently
        // shortened expression.
        if (len + 1 >= out_cap - w) {
            *err = "Result too long";
            return kCalcFailed;
        }
        w += len + 1;
    }
    *count = n_sol;
    return kCalcOk;
}

}  // namespace

void calc_api_set_persist_hook(CalcPersistFn fn) {
    g_persist = fn;
}

void calc_api_set_stack_hook(CalcStackRoomFn fn) {
    g_stack_room = fn;
}

// Each entry point sets the guard, calls exactly one _impl, and clears it.

CalcStatus calc_api_eval(const char* expr, CalcKind* kind, double* re, double* im, char* text,
                         size_t text_cap, const char** err) {
    if (g_in_call) {
        return kCalcBusy;
    }
    g_in_call = true;
    const CalcStatus st = eval_impl(expr, kind, re, im, text, text_cap, err);
    g_in_call = false;
    return st;
}

CalcStatus calc_api_store(const char* name, double re, double im, const char** err) {
    if (g_in_call) {
        return kCalcBusy;
    }
    g_in_call = true;
    const CalcStatus st = store_impl(name, re, im, err);
    g_in_call = false;
    return st;
}

CalcStatus calc_api_recall(const char* name, double* re, double* im, int* is_complex,
                           const char** err) {
    if (g_in_call) {
        return kCalcBusy;
    }
    g_in_call = true;
    const CalcStatus st = recall_impl(name, re, im, is_complex, err);
    g_in_call = false;
    return st;
}

CalcStatus calc_api_cas(const char* op, const char* expr, const char* var, const char* arg3,
                        const char* arg4, CalcKind* kind, double* re, char* out, size_t out_cap,
                        const char** err) {
    if (g_in_call) {
        return kCalcBusy;
    }
    g_in_call = true;
    const CalcStatus st = cas_impl(op, expr, var, arg3, arg4, kind, re, out, out_cap, err);
    g_in_call = false;
    return st;
}

CalcStatus calc_api_solve(const char* expr, const char* var, int* count, char* out, size_t out_cap,
                          const char** err) {
    if (g_in_call) {
        return kCalcBusy;
    }
    g_in_call = true;
    const CalcStatus st = solve_impl(expr, var, count, out, out_cap, err);
    g_in_call = false;
    return st;
}

// ---- 6B.6: graphing ----

namespace {

// A Y= slot from a 1-based label, or -1. Callers use "Y1".."Y7" or 1..7;
// the check is the same either way and the message names both.
int graph_slot(int one_based, const char** err) {
    if (one_based < 1 || one_based > graph::kFunctionSlots) {
        *err = "Slot must be Y1-Y7";
        return -1;
    }
    return one_based - 1;
}

CalcStatus plot_impl(const char* expr, int* slot, const char** err) {
    if (expr == nullptr || expr[0] == 0) {
        *err = "Empty expression";
        return kCalcFailed;
    }
    if (std::strlen(expr) >= config::kMaxExprLen) {
        *err = "Expression too long";
        return kCalcFailed;
    }
    graph::GraphState& st = graph::state();

    // D68: the first plot of a run wipes the slate, so what the script draws
    // is what the script asked for. Plotting also forces FUNC mode — a
    // script that writes Y1 and leaves the calculator in POLAR would show a
    // blank screen and no reason why.
    int target = 0;
    if (!g_plotted_this_run) {
        for (int i = 0; i < graph::kFunctionSlots; ++i) {
            st.y.expr[i][0] = 0;
            st.y.enabled[i] = false;
        }
        st.mode = graph::Mode::kFunction;
        g_plotted_this_run = true;
    } else {
        while (target < graph::kFunctionSlots && st.y.expr[target][0] != 0) {
            ++target;
        }
        if (target >= graph::kFunctionSlots) {
            *err = "All 7 graph slots used";
            return kCalcFailed;
        }
    }
    std::snprintf(st.y.expr[target], config::kMaxExprLen, "%s", expr);
    st.y.enabled[target] = true;
    *slot = target + 1;
    persist(kCalcPersistGraph);
    return kCalcOk;
}

CalcStatus window_impl(double x_min, double x_max, double y_min, double y_max, const char** err) {
    if (!(x_min < x_max) || !(y_min < y_max)) {
        *err = "Window needs min < max";
        return kCalcFailed;
    }
    graph::GraphState& st = graph::state();
    st.window.x_min = x_min;
    st.window.x_max = x_max;
    st.window.y_min = y_min;
    st.window.y_max = y_max;
    persist(kCalcPersistGraph);
    return kCalcOk;
}

CalcStatus analyze_impl(const char* op, int one_based, double lo, double hi, double* a, double* b,
                        int* two_values, const char** err) {
    const int slot = graph_slot(one_based, err);
    if (slot < 0) {
        return kCalcFailed;
    }
    const graph::GraphState& st = graph::state();
    if (st.y.expr[slot][0] == 0) {
        *err = "Slot is empty";
        return kCalcFailed;
    }
    graph::AnalysisResult r;
    *two_values = 0;
    if (std::strcmp(op, "zero") == 0) {
        r = graph::analyze_zero(st, slot, lo, hi, 0.5 * (lo + hi));
        *two_values = 1;
    } else if (std::strcmp(op, "min") == 0 || std::strcmp(op, "max") == 0) {
        r = graph::analyze_extremum(st, slot, lo, hi, op[1] == 'a');
        *two_values = 1;
    } else if (std::strcmp(op, "value") == 0) {
        r = graph::analyze_value(st, slot, lo);
        *two_values = 1;
    } else if (std::strcmp(op, "deriv") == 0) {
        r = graph::analyze_derivative(st, slot, lo);
    } else if (std::strcmp(op, "integral") == 0) {
        r = graph::analyze_integral(st, slot, lo, hi);
    } else {
        *err = "Unknown analysis";
        return kCalcFailed;
    }
    if (!r.ok) {
        *err = r.error != nullptr ? r.error : "No result";
        return kCalcFailed;
    }
    if (*two_values != 0) {
        *a = r.x;
        *b = r.y;
    } else {
        *a = r.aux;  // slope, or the definite integral
        *b = 0;
    }
    return kCalcOk;
}

}  // namespace

void calc_api_begin_run(void) {
    g_plotted_this_run = false;
    g_show_graph_requested = false;
}

void calc_api_show_graph(void) {
    g_show_graph_requested = true;
}

int calc_api_take_show_graph(void) {
    const bool want = g_show_graph_requested;
    g_show_graph_requested = false;
    return want ? 1 : 0;
}

CalcStatus calc_api_plot(const char* expr, int* slot, const char** err) {
    if (g_in_call) {
        return kCalcBusy;
    }
    g_in_call = true;
    const CalcStatus st = plot_impl(expr, slot, err);
    g_in_call = false;
    return st;
}

CalcStatus calc_api_window(double x_min, double x_max, double y_min, double y_max,
                           const char** err) {
    if (g_in_call) {
        return kCalcBusy;
    }
    g_in_call = true;
    const CalcStatus st = window_impl(x_min, x_max, y_min, y_max, err);
    g_in_call = false;
    return st;
}

CalcStatus calc_api_graph_analyze(const char* op, int slot, double lo, double hi, double* a,
                                  double* b, int* two_values, const char** err) {
    if (g_in_call) {
        return kCalcBusy;
    }
    // The deepest path any binding reaches. analyze_integral recurses through
    // integrate_panel, 136 bytes a frame to a depth cap of 12 — 1,632 bytes
    // of recursion before the expression evaluator underneath it. Measured
    // separately from kEvalStackNeed for that reason.
    if (!stack_room(kAnalysisStackNeed)) {
        *err = "Not enough stack for graph analysis";
        return kCalcFailed;
    }
    g_in_call = true;
    const CalcStatus st = analyze_impl(op, slot, lo, hi, a, b, two_values, err);
    g_in_call = false;
    return st;
}

// Routed through math:: rather than libm so the conventions are the
// calculator's — notably c_arg is always radians in -pi..pi, whatever the
// angle mode says (math/complex.hpp:31). That is the reason these exist
// alongside Python's own abs() on a complex.
double calc_api_c_abs(double re, double im) {
    return math::c_abs(math::Complex{re, im});
}

double calc_api_c_arg(double re, double im) {
    return math::c_arg(math::Complex{re, im});
}

void calc_api_c_conj(double re, double im, double* out_re, double* out_im) {
    const math::Complex z = math::c_conj(math::Complex{re, im});
    *out_re = z.re;
    *out_im = z.im;
}
