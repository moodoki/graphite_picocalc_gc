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

// One directory entry, mirroring platform::Storage::DirEntry, which this
// header cannot include (it must stay C and free of C++ headers).
enum { kCalcDirNameMax = 64 };
typedef struct CalcDirEntry {
    char name[kCalcDirNameMax];
    int is_dir;
    unsigned long size;
} CalcDirEntry;

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

// What a binding just changed and needs written to the SD card. `index` is
// the slot for the two that have slots and -1 for the two that do not —
// matching ListStore/MatrixStore::save(storage, index), which persist one at
// a time on purpose (the 2026-07-22 perf fix that split lists.dat into six
// files).
typedef enum CalcPersistTarget {
    kCalcPersistVars = 0,  // A-Z / theta / Ans, index -1
    kCalcPersistGraph,     // Y1-Y7, the window, the mode row, index -1
    kCalcPersistList,      // l1-l6, index 0-5
    kCalcPersistMatrix,    // [A]-[J], index 0-9
} CalcPersistTarget;

// Called after a binding writes persistent calculator state. It is a hook
// rather than a direct platform::storage() call because that is the one
// dependency that would stop this file from building in the host test
// harness — which is where the pipeline, the name rules, D68's Y-slot
// semantics and both guards are actually tested.
// scripting::PythonInterpreter::init() installs the real one; tests install
// their own or none.
typedef void (*CalcPersistFn)(CalcPersistTarget what, int index);
void calc_api_set_persist_hook(CalcPersistFn fn);

// Called by the interpreter at the start of every top-level exec(), NOT on
// every binding call. It resets the per-run latches — D68's "has this run
// plotted yet", which is what makes a script's graph output a function of the
// script alone rather than of what the last one left behind, and the dirty
// mask below.
void calc_api_begin_run(void);

// Called by the interpreter once exec() has returned. Persists every list and
// matrix the run touched, one save per slot.
//
// D82: list_append marks dirty and does NOT save. A logging loop would
// otherwise cost one SD write per sample, which is the exact pattern the
// 2026-07-22 fix split the list files apart to avoid. Variables and graph
// state still persist immediately — a variable image is 456 bytes and a list
// can be 10,000 elements, so the loop cost is not comparable.
//
// The stated cost of deferring: a script killed by ESC, or one that raises,
// loses the samples it had not saved.
void calc_api_flush_run(void);

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

// ---- 6B.17: lists ----

// Lists are l1-l6, 1-based to match how they are written everywhere else.
// Named lists are not exposed in v1.
//
// A list is a math::Array: it moves to PSRAM above ~256 elements (D21), so a
// long log lives outside both SRAM and the Python heap. That is the whole
// point of list_append — D77 measured a 400-iteration loop exhausting the
// 40 KB heap when the samples accumulated in a Python list instead.
//
// set_list and list_append mark the list dirty and DO NOT save; the run's
// lists are written once by calc_api_flush_run (D82).
CalcStatus calc_api_list_size(int list, int* size, const char** err);
CalcStatus calc_api_list_resize(int list, int size, const char** err);

// Bulk element access, `count` at a time from `first`. The caller supplies
// the buffer, so nothing here holds storage across a call.
CalcStatus calc_api_list_read(int list, int first, int count, double* out, const char** err);
CalcStatus calc_api_list_write(int list, int first, int count, const double* src, const char** err);

CalcStatus calc_api_list_append(int list, double value, const char** err);

// 1-var statistics over a list. `what` is "mean", "sum", "min", "max",
// "stddev" or "n".
CalcStatus calc_api_list_stat(int list, const char* what, double* out, const char** err);

// ---- 6B.7: matrices ----

// Matrices cross as nested Python lists rather than as handles: math::Array
// is non-copyable and PSRAM-backed, and there are only ten named slots, so a
// chained calculation would run out of them. A list argument is copied into
// one file-static scratch Array for the duration of the call and released
// again; results land in MatAns, which is where the home screen puts a matrix
// result too.
//
// The scratch is filled by resize-then-write, and read back the same way.
CalcStatus calc_api_mat_begin(int rows, int cols, const char** err);
CalcStatus calc_api_mat_write_row(int row, int count, const double* src, const char** err);

// `op` is "det", "inverse", "transpose", "rref", "mul" or "eigenvalues".
// "mul" consumes the scratch as the left operand and slot `rhs` as the right.
// Scalar results (det) land in *scalar; everything else goes to MatAns and
// reports its shape.
CalcStatus calc_api_mat_op(const char* op, int rhs, double* scalar, int* rows, int* cols,
                           const char** err);

// Read MatAns back, a row at a time.
CalcStatus calc_api_mat_read_row(int row, int count, double* out, const char** err);

// Copy the scratch into [A]-[J] (0-9), or a named slot back into the scratch.
// These are the only bindings that touch the persisted matrices; like the
// lists they mark dirty and defer the save.
CalcStatus calc_api_mat_store(int slot, const char** err);
CalcStatus calc_api_mat_load(int slot, int* rows, int* cols, const char** err);

// ---- 6B.9: keyboard ----

// One key event as a script sees it. `code` is a stable small integer
// (platform::Key's value); `ch` is the printable character or 0.
//
// `name` is what makes the event usable from Python. `code` is an enum value
// a script has no names for, and `ch` is 0 for every key that is not a
// character — so before this, **a script could not tell which arrow had been
// pressed** (found building the periodic table app, §4.6 entry 1, which is
// exactly what that list exists for). It is a static string, never owned, and
// "" for keys with no name; it uses the same table as calc_api_key_held, so
// `ev["name"] == "up"` and `calc.key_held("up")` cannot drift apart.
typedef struct CalcKeyEvent {
    int code;
    int ch;
    int shift;
    int ctrl;
    int alt;
    const char* name;
} CalcKeyEvent;

// Drain the keyboard into the queue and take the oldest event, if any.
// Returns 0 when nothing is waiting.
//
// D81, refined by building it: the rule is ONE DRAIN ROUTINE AND ONE QUEUE,
// not one caller. A blocking wait_key() sits inside a binding where the VM
// hook does not run, so it cannot be the hook's job alone — both call the
// same drain, ESC is recognised inside it, and the queue means neither
// steals from the other. Before this, the hook simply discarded every
// non-ESC event, which silently dropped type-ahead during a script.
typedef int (*CalcKeyPollFn)(CalcKeyEvent* out);

// Is the key called `name` down right now? Resolved on the platform side,
// where platform::Key is actually visible — a name table over hardcoded
// enumerator values would go quietly wrong the first time the enum gained a
// member. Keyboard::is_held is a separate query from the event stream, so
// this never competes with the queue.
typedef int (*CalcKeyHeldFn)(const char* name);
void calc_api_set_key_hooks(CalcKeyPollFn poll, CalcKeyHeldFn held);

// Non-blocking: 1 and *out filled, or 0.
int calc_api_key_pressed(CalcKeyEvent* out);
int calc_api_key_held(const char* name);

// ---- 6B.10: file I/O ----

// Storage reaches the bindings through this, for the same reason the
// persist and stack hooks exist: calc_api.cpp must keep building in the host
// test harness, where there is no SD card.
typedef struct CalcFileOps {
    long (*size)(const char* path);                                  // -1 if missing
    int (*read)(const char* path, long offset, char* buf, int len);  // bytes, or -1
    int (*write)(const char* path, const char* buf, int len);        // 1 ok
    int (*append)(const char* path, const char* buf, int len);       // 1 ok
    int (*exists)(const char* path);
    // At most `max` entries starting at `skip`; count, or -1 on error.
    // A short return means the directory ended (issue #53).
    int (*list)(const char* path, CalcDirEntry* out, int max, int skip);
} CalcFileOps;
void calc_api_set_file_ops(const CalcFileOps* ops);

CalcStatus calc_api_file_size(const char* path, long* out, const char** err);
CalcStatus calc_api_file_read(const char* path, long offset, char* buf, int len, int* got,
                              const char** err);
CalcStatus calc_api_file_write(const char* path, const char* buf, int len, int append,
                               const char** err);
int calc_api_file_exists(const char* path);

// One window of a directory listing (issue #53). The glue walks a whole
// directory by calling this with a rising `skip` and building Python
// objects BETWEEN calls, never during one — the same shape as
// calc_api_list_read, and the reason is D74: an allocation can trigger a
// GC pass, a finalizer, or a MemoryError longjmp, none of which may
// happen while a C++ frame holding an open directory is on the stack.
CalcStatus calc_api_list_dir(const char* path, int skip, int max, CalcDirEntry* out, int* out_count,
                             const char** err);

// ---- Opt-in ESC delivery (issue #55) ----
//
// Off by default and reset at the start of every run. While on, ESC
// arrives through the key bindings as an ordinary event ("esc") instead
// of raising KeyboardInterrupt, so a script can use it for "back one
// level" the way every screen in the firmware does.
//
// ESC still always gets the user out: the runtime counts presses the
// script has not read, and interrupts on the second. So this weakens
// nothing for a script that stops responding — it only gives a script
// that IS responding the chance to handle the key first.
typedef int (*CalcCaptureEscFn)(int on);  // returns the previous state
void calc_api_set_capture_esc_hook(CalcCaptureEscFn fn);
CalcStatus calc_api_capture_esc(int on, int* prev, const char** err);

// ---- 6B.8: the script canvas ----

// A colour, as RGB565. Resolved in the glue from either a name or an (r,g,b)
// tuple so the drawing entry points below take one plain integer.
CalcStatus calc_api_color(const char* name, int r, int g, int b, unsigned* out, const char** err);

// A script that clears the screen takes the panel until it ends — D80. The
// pixels survive because ProgramScreen then marks nothing dirty, which makes
// the render loop skip the frame entirely.
void calc_api_canvas_clear(unsigned color);
void calc_api_canvas_pixel(int x, int y, unsigned color);
void calc_api_canvas_line(int x0, int y0, int x1, int y1, unsigned color);
void calc_api_canvas_rect(int x, int y, int w, int h, unsigned color, int fill);
int calc_api_canvas_text(int x, int y, const char* s, unsigned fg, unsigned bg);
int calc_api_canvas_text_width(const char* s);
int calc_api_canvas_text_height(void);

// Did this run take the panel? ProgramScreen asks after exec() returns.
int calc_api_canvas_owns_display(void);

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
