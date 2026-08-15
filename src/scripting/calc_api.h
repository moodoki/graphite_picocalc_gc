#pragma once

// The calculator's C boundary, for the `calc` Python module (Phase 6B.3-6B.5).
//
// THREE FILES, ONE RULE. mp_calc_module.c converts Python arguments, calls
// exactly one function from this header, and only then builds Python objects
// or raises. calc_api.cpp implements these as C++ leaves that never call back
// into MicroPython.
//
// The rule exists because MicroPython raises by longjmp, which unwinds past
// every intervening frame without running a C++ destructor and without a
// compiler diagnostic. It is not only mp_raise_* that longjmps: mp_obj_new_*
// can trigger a GC pass, a Python __del__ finalizer can run arbitrary code
// during it, and a MemoryError leaves from there. So *allocation* has to
// happen after the C++ leaf has returned, not just raising. Splitting the
// files makes that structural rather than something to remember, the same way
// mp_port.c does for the runtime glue.
//
// Two consequences for this header:
//
//   * It is preprocessed by the HOST compiler during MicroPython's qstr
//     generation (see drivers/micropython_port/micropython_embed.mk) as well
//     as by arm-none-eabi during the build. It may include nothing but
//     <stddef.h> — no Pico SDK, no C++.
//   * Every function reports failure by return code plus a STATIC error
//     string. Nothing here hands back a pointer with a lifetime.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Longest expression accepted or result string produced. Mirrors
// config::kMaxExprLen, which this header cannot include.
enum { kCalcTextMax = 256 };

// Most solutions math::cas::solve can return (cas_eval.hpp's HomeResult).
enum { kCalcMaxSolutions = 8 };

typedef enum CalcStatus {
    kCalcOk = 0,
    kCalcFailed,  // *err holds a static message
    kCalcBusy,    // reentered — see the guard in calc_api.cpp
} CalcStatus;

// What calc_api_eval produced. A list, a matrix, "Done (n lists)" and a
// symbolic CAS result all arrive as kCalcText: math::unified::evaluate_home
// returns formatted text rather than a live Value, so there is no reference
// with a lifetime to mismanage (phase6-spec.md §4.7 point 2).
typedef enum CalcKind {
    kCalcReal = 0,
    kCalcComplex,
    kCalcText,
} CalcKind;

// ---- Wiring ----

// What a binding just changed and needs written to the SD card.
typedef enum CalcPersistTarget {
    kCalcPersistVars = 0,  // A-Z / theta / Ans
    kCalcPersistGraph,     // Y1-Y7, the window, the mode row
} CalcPersistTarget;

// Called after a binding writes persistent calculator state. It is a hook
// rather than a direct platform::storage() call because that is the one
// dependency that would stop this file from building in the host test
// harness — which is where the pipeline, the name rules, D68's Y-slot
// semantics and both guards are actually tested.
// scripting::PythonInterpreter::init() installs the real one; tests install
// their own or none.
typedef void (*CalcPersistFn)(CalcPersistTarget what);
void calc_api_set_persist_hook(CalcPersistFn fn);

// Called by the interpreter at the start of every top-level exec(), NOT on
// every binding call. It resets the per-run latches — currently just D68's
// "has this run plotted yet", which is what makes a script's graph output a
// function of the script alone rather than of what the last one left behind.
void calc_api_begin_run(void);

// True when a script asked for the graph to be shown (calc.show_graph()).
// Reading it clears it.
//
// The switch is DEFERRED rather than immediate: a binding runs inside the
// VM, inside the screen's on_key, so pushing a screen from here would nest
// screen management inside itself and still render nothing until the script
// returned. The caller performs the switch once exec() is done — the same
// shape as 6B.12's decision to buffer output rather than stream it.
int calc_api_take_show_graph(void);

// Returns nonzero when at least `need` bytes of core 0's stack remain below
// the caller. A hook for the same reason as the one above.
//
// This is not belt-and-braces. A binding runs with the MicroPython VM already
// ~1.5 KB into a 4 KB stack, and the evaluator paths below it have the
// deepest frames in the firmware. MicroPython's own MICROPY_STACK_CHECK only
// guards its own recursion; once control is inside math/, nothing checks, and
// running off the end of SCRATCH_Y hangs the machine (D48) rather than
// raising. With no hook installed every path is allowed — which is what host
// tests want, since they run on a normal-sized stack.
typedef int (*CalcStackRoomFn)(size_t need);
void calc_api_set_stack_hook(CalcStackRoomFn fn);

// ---- 6B.3: evaluation and variables ----

// Run `expr` through the same four steps HomeScreen::evaluate_input does, in
// the same order. `text`/`text_cap` are only written for kCalcText, and are
// caller-provided so no static buffer's contents outlive the call.
CalcStatus calc_api_eval(const char* expr, CalcKind* kind, double* re, double* im, char* text,
                         size_t text_cap, const char** err);

// `name` must be exactly one character 'a'..'z' (case-sensitive), or "theta"
// or "ans". Anything else fails rather than silently landing on Ans, which is
// what math::Variables::operator[] would do.
//
// Storing is persisted to the SD card immediately; a value written from a
// script has to survive a power cycle the same way a typed one does.
CalcStatus calc_api_store(const char* name, double re, double im, const char** err);
CalcStatus calc_api_recall(const char* name, double* re, double* im, int* is_complex,
                           const char** err);

// ---- 6B.4: CAS ----

// `op` is one of "simplify", "expand", "factor", "diff", "integ", "solve" —
// the strings math::cas::evaluate_home itself recognizes. `var` may be NULL
// for the ops that default it to 'x'. `arg3`/`arg4` are extra positional
// arguments as text (diff's order n, integ's bounds), NULL when absent.
//
// A result that is a bare numeric literal — a definite integral, or an
// expression that simplified to a constant — comes back as kCalcReal with the
// value in *re. Everything else is kCalcText, serialized into `out` before
// returning, because cas Expr nodes are only valid until the next top-level
// CAS operation.
CalcStatus calc_api_cas(const char* op, const char* expr, const char* var, const char* arg3,
                        const char* arg4, CalcKind* kind, double* re, char* out, size_t out_cap,
                        const char** err);

// solve() yields a set, and this hands the whole set back in ONE call: `out`
// receives *count NUL-terminated strings packed back to back.
//
// The obvious alternative — report the count, then serialize solution i on
// demand — would leave the cas Expr pool exposed between calls. Building the
// Python list is what happens in that window, every mp_obj_new_str can trigger
// a GC pass, a __del__ finalizer during it can call calc.simplify, and that
// resets the pool out from under the solutions not yet read. Packing them up
// front costs one buffer and removes the window entirely.
CalcStatus calc_api_solve(const char* expr, const char* var, int* count, char* out, size_t out_cap,
                          const char** err);

// ---- 6B.6: graphing ----

// Write `expr` into a Y= slot. D68: the FIRST plot of a script run clears
// all seven slots and writes Y1; later calls in the same run append to Y2,
// Y3, ...; an eighth fails. The latch resets at calc_api_begin_run, so a
// re-run starts clean and a script's graph is a function of the script.
//
// This DESTROYS the user's own Y= functions, and graph state is persisted,
// so the loss survives a power cycle. That is the documented cost of D68's
// predictability, not an oversight.
//
// The slot number written lands in *slot (1-based), which is also the
// colour: slot colour is fixed per index (Y1 blue, Y2 red, ...) exactly as
// the Y= editor shows it. There is no per-plot colour — GraphState has no
// field for one, and adding it would bump the persistence magic and reset
// every user's graphs.
CalcStatus calc_api_plot(const char* expr, int* slot, const char** err);

// Set the plot window. Fails rather than silently accepting an empty or
// inverted range, which would render as a blank screen with no explanation.
CalcStatus calc_api_window(double x_min, double x_max, double y_min, double y_max,
                           const char** err);

// Request the graph screen. Deferred — see calc_api_take_show_graph.
void calc_api_show_graph(void);

// Numeric graph analysis over a Y= slot, 1-based to match the labels.
// `op` is one of "zero", "min", "max", "integral", "deriv", "value".
//
// Two outputs because the useful answer differs by op: zero and value give
// (x, y); min and max give the extremum point; integral and deriv give a
// single number in *a with *b unused. `two_values` says which.
CalcStatus calc_api_graph_analyze(const char* op, int slot, double lo, double hi, double* a,
                                  double* b, int* two_values, const char** err);

// ---- 6B.5: complex ----

// Wrappers over math::c_abs/c_arg/c_conj rather than libm, so the
// conventions are the calculator's. Note c_arg is ALWAYS radians in -pi..pi,
// independent of angle mode (math/complex.hpp).
double calc_api_c_abs(double re, double im);
double calc_api_c_arg(double re, double im);
void calc_api_c_conj(double re, double im, double* out_re, double* out_im);

#ifdef __cplusplus
}  // extern "C"
#endif
