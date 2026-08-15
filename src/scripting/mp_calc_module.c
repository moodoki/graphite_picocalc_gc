// The `calc` Python module (Phase 6B.3-6B.5).
//
// This file is the ONLY place MicroPython objects and calculator state meet,
// and it meets them in one direction at a time: convert arguments, call
// exactly one calc_api_* function, then build the result object or raise.
// Nothing here calls into C++ while holding a Python object half-built, and
// nothing in calc_api.cpp calls back into MicroPython — so no longjmp ever
// unwinds a C++ frame. calc_api.h explains why that is a structural rule and
// not a style preference.
//
// It is compiled by CMake as part of the firmware, NOT copied into the
// generated embed package. MicroPython's generator sees it only through
// SRC_QSTR (drivers/micropython_port/micropython_embed.mk), which is enough
// for the two things it must produce: the MP_QSTR_* names below, and the
// MP_REGISTER_MODULE entry that puts `calc` in the builtin module table.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "py/obj.h"
#include "py/runtime.h"

#include "py/objstr.h"

#include "scripting/calc_api.h"
#include "scripting/mp_port.h"

// Four checks are switched off for the whole file, all of them arguing with
// MicroPython's C rather than with anything this project decided:
//
//   readability-identifier-naming     MP_DEFINE_CONST_FUN_OBJ_* and
//                                     MP_DEFINE_CONST_DICT declare `const`
//                                     objects whose names upstream fixes as
//                                     lower_snake_case; the project's kName
//                                     rule cannot apply to a name a macro
//                                     builds from its argument.
//   performance-no-int-to-ptr         MP_ROM_QSTR is a tagged pointer. The
//                                     int-to-pointer cast IS the object
//                                     representation.
//   cppcoreguidelines-interfaces-global-init
//                                     A module object points at
//                                     mp_type_module, another const global.
//                                     Both are in flash with no initializer
//                                     to order.
//   ...insecureAPI.DeprecatedOrUnsafeBufferHandling
//                                     snprintf. The project uses it
//                                     everywhere; the C11 _s variants do not
//                                     exist in this toolchain.
//
// Scoped to this one file rather than added to .clang-tidy, so the rest of
// src/ keeps all four.
// NOLINTBEGIN(readability-identifier-naming,performance-no-int-to-ptr,cppcoreguidelines-interfaces-global-init,clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

// Also declared in the generated genhdr/moduledefs.h, which py/objmodule.c
// includes and this file does not.
extern const mp_obj_module_t calc_user_cmodule;

// ---- Shared helpers ----

// Every failure a calc_api_* call can report, as the Python exception that
// fits it. Errors arrive as static strings, so there is nothing to free and
// nothing whose lifetime could end before the raise.
static MP_NORETURN void calc_raise(CalcStatus st, const char* err) {
    if (st == kCalcBusy) {
        // Re-entered, which in practice means a __del__ finalizer ran inside
        // a binding and called back in. RuntimeError rather than ValueError:
        // nothing is wrong with the caller's arguments.
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("calc is busy"));
    }
    mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("%s"), err != NULL ? err : "calc error");
}

// A kCalcReal/kCalcComplex/kCalcText triple as the Python object it stands for.
static mp_obj_t calc_value(CalcKind kind, double re, double im, const char* text) {
    if (kind == kCalcReal) {
        return mp_obj_new_float((mp_float_t)re);
    }
    if (kind == kCalcComplex) {
        return mp_obj_new_complex((mp_float_t)re, (mp_float_t)im);
    }
    return mp_obj_new_str(text, strlen(text));
}

// An optional string argument, or NULL when absent — which lets the CAS
// operation apply its own default variable rather than this file guessing it.
static const char* opt_str(size_t n_args, const mp_obj_t* args, size_t i) {
    return i < n_args ? mp_obj_str_get_str(args[i]) : NULL;
}

// An optional argument rendered as text, because calc_api_cas composes the
// call as a string. A str passes through, so a bound can be an expression
// ("pi/2"); anything else is taken as a number. %.17g is what round-trips a
// double exactly, and this text is about to be re-parsed.
static const char* opt_text(size_t n_args, const mp_obj_t* args, size_t i, char* buf, size_t cap) {
    if (i >= n_args) {
        return NULL;
    }
    if (mp_obj_is_str(args[i])) {
        return mp_obj_str_get_str(args[i]);
    }
    snprintf(buf, cap, "%.17g", (double)mp_obj_get_float(args[i]));
    return buf;
}

// One CAS binding body, shared by all five single-result ops.
static mp_obj_t calc_cas_call(const char* op, size_t n_args, const mp_obj_t* args, bool takes_arg3,
                              bool takes_arg4) {
    char arg3_buf[32];
    char arg4_buf[32];
    const char* expr = mp_obj_str_get_str(args[0]);
    const char* var = opt_str(n_args, args, 1);
    const char* arg3 = takes_arg3 ? opt_text(n_args, args, 2, arg3_buf, sizeof(arg3_buf)) : NULL;
    const char* arg4 = takes_arg4 ? opt_text(n_args, args, 3, arg4_buf, sizeof(arg4_buf)) : NULL;

    CalcKind kind = kCalcText;
    double re = 0;
    char out[kCalcTextMax];
    const char* err = NULL;
    const CalcStatus st =
        calc_api_cas(op, expr, var, arg3, arg4, &kind, &re, out, sizeof(out), &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return calc_value(kind, re, 0, out);
}

// ---- 6B.3: evaluation and variables ----

static mp_obj_t calc_eval(mp_obj_t expr_obj) {
    const char* expr = mp_obj_str_get_str(expr_obj);
    CalcKind kind = kCalcReal;
    double re = 0;
    double im = 0;
    char text[kCalcTextMax];
    const char* err = NULL;
    const CalcStatus st = calc_api_eval(expr, &kind, &re, &im, text, sizeof(text), &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return calc_value(kind, re, im, text);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_eval_obj, calc_eval);

static mp_obj_t calc_store(mp_obj_t name_obj, mp_obj_t value_obj) {
    const char* name = mp_obj_str_get_str(name_obj);
    mp_float_t re = 0;
    mp_float_t im = 0;
    mp_obj_get_complex(value_obj, &re, &im);  // a real value comes back with im == 0
    const char* err = NULL;
    const CalcStatus st = calc_api_store(name, (double)re, (double)im, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(calc_store_obj, calc_store);

static mp_obj_t calc_recall(mp_obj_t name_obj) {
    const char* name = mp_obj_str_get_str(name_obj);
    double re = 0;
    double im = 0;
    int is_complex = 0;
    const char* err = NULL;
    const CalcStatus st = calc_api_recall(name, &re, &im, &is_complex, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    // Not calc_value(): a variable is never text, and passing it NULL for the
    // text argument is something the compiler can see and dislike.
    return is_complex ? mp_obj_new_complex((mp_float_t)re, (mp_float_t)im)
                      : mp_obj_new_float((mp_float_t)re);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_recall_obj, calc_recall);

// ---- 6B.4: CAS ----

static mp_obj_t calc_simplify(mp_obj_t e) {
    const mp_obj_t args[1] = {e};
    return calc_cas_call("simplify", 1, args, false, false);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_simplify_obj, calc_simplify);

static mp_obj_t calc_expand(mp_obj_t e) {
    const mp_obj_t args[1] = {e};
    return calc_cas_call("expand", 1, args, false, false);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_expand_obj, calc_expand);

static mp_obj_t calc_factor(size_t n_args, const mp_obj_t* args) {
    return calc_cas_call("factor", n_args, args, false, false);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_factor_obj, 1, 2, calc_factor);

// diff(expr [,var [,n]])
static mp_obj_t calc_diff(size_t n_args, const mp_obj_t* args) {
    return calc_cas_call("diff", n_args, args, true, false);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_diff_obj, 1, 3, calc_diff);

// integ(expr [,var]) is indefinite and returns a string;
// integ(expr, var, lo, hi) is definite and returns a float.
static mp_obj_t calc_integ(size_t n_args, const mp_obj_t* args) {
    if (n_args == 3) {
        mp_raise_ValueError(MP_ERROR_TEXT("integ needs both bounds"));
    }
    return calc_cas_call("integ", n_args, args, true, true);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_integ_obj, 1, 4, calc_integ);

static mp_obj_t calc_solve(size_t n_args, const mp_obj_t* args) {
    const char* expr = mp_obj_str_get_str(args[0]);
    const char* var = opt_str(n_args, args, 1);
    int count = 0;
    char packed[kCalcTextMax];
    const char* err = NULL;
    const CalcStatus st = calc_api_solve(expr, var, &count, packed, sizeof(packed), &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    // Only now do we allocate. Every solution is already a copy in `packed`,
    // so a GC pass triggered here — and any finalizer it runs — cannot pull
    // the CAS pool out from under the rest of this loop.
    mp_obj_t list = mp_obj_new_list(0, NULL);
    const char* p = packed;
    for (int i = 0; i < count; ++i) {
        const size_t len = strlen(p);
        mp_obj_list_append(list, mp_obj_new_str(p, len));
        p += len + 1;
    }
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_solve_obj, 1, 2, calc_solve);

// ---- 6B.6: graphing ----

// A Y= slot argument, accepted as "Y1".."Y7" (as §4.2 writes it) or as a
// plain 1..7. Returns 0 for anything else and lets calc_api name the range.
static int calc_slot_arg(mp_obj_t o) {
    if (mp_obj_is_str(o)) {
        const char* s = mp_obj_str_get_str(o);
        if ((s[0] == 'Y' || s[0] == 'y') && s[1] >= '1' && s[1] <= '9' && s[2] == 0) {
            return s[1] - '0';
        }
        return 0;
    }
    return mp_obj_get_int(o);
}

static mp_obj_t calc_plot(mp_obj_t expr_obj) {
    const char* expr = mp_obj_str_get_str(expr_obj);
    int slot = 0;
    const char* err = NULL;
    const CalcStatus st = calc_api_plot(expr, &slot, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    // The slot number is worth returning: it is also which colour the curve
    // will be, since colour is fixed per slot.
    return MP_OBJ_NEW_SMALL_INT(slot);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_plot_obj, calc_plot);

static mp_obj_t calc_window(size_t n_args, const mp_obj_t* args) {
    (void)n_args;
    const char* err = NULL;
    const CalcStatus st =
        calc_api_window((double)mp_obj_get_float(args[0]), (double)mp_obj_get_float(args[1]),
                        (double)mp_obj_get_float(args[2]), (double)mp_obj_get_float(args[3]), &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_window_obj, 4, 4, calc_window);

static mp_obj_t calc_show_graph(void) {
    calc_api_show_graph();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(calc_show_graph_obj, calc_show_graph);

// One body for all six analysis bindings. Those that describe a point
// return a (x, y) tuple; those that produce a single number return a float.
static mp_obj_t calc_analyze(const char* op, size_t n_args, const mp_obj_t* args) {
    const int slot = calc_slot_arg(args[0]);
    const double lo = (double)mp_obj_get_float(args[1]);
    const double hi = n_args > 2 ? (double)mp_obj_get_float(args[2]) : lo;
    double a = 0;
    double b = 0;
    int two = 0;
    const char* err = NULL;
    const CalcStatus st = calc_api_graph_analyze(op, slot, lo, hi, &a, &b, &two, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    if (two) {
        mp_obj_t pair[2] = {mp_obj_new_float((mp_float_t)a), mp_obj_new_float((mp_float_t)b)};
        return mp_obj_new_tuple(2, pair);
    }
    return mp_obj_new_float((mp_float_t)a);
}

static mp_obj_t calc_graph_zero(size_t n, const mp_obj_t* a) {
    return calc_analyze("zero", n, a);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_graph_zero_obj, 3, 3, calc_graph_zero);

static mp_obj_t calc_graph_min(size_t n, const mp_obj_t* a) {
    return calc_analyze("min", n, a);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_graph_min_obj, 3, 3, calc_graph_min);

static mp_obj_t calc_graph_max(size_t n, const mp_obj_t* a) {
    return calc_analyze("max", n, a);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_graph_max_obj, 3, 3, calc_graph_max);

static mp_obj_t calc_graph_integral(size_t n, const mp_obj_t* a) {
    return calc_analyze("integral", n, a);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_graph_integral_obj, 3, 3, calc_graph_integral);

static mp_obj_t calc_graph_deriv(size_t n, const mp_obj_t* a) {
    return calc_analyze("deriv", n, a);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_graph_deriv_obj, 2, 2, calc_graph_deriv);

static mp_obj_t calc_graph_value(size_t n, const mp_obj_t* a) {
    return calc_analyze("value", n, a);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_graph_value_obj, 2, 2, calc_graph_value);

// ---- 6B.17: lists ----

// How many elements cross per calc_api call. Element-at-a-time would be one
// boundary crossing per sample; one big buffer would be SRAM this does not
// have. 32 doubles is 256 bytes of stack against the ~2,200 a binding gets.
#define CALC_CHUNK 32

static mp_obj_t calc_set_list(mp_obj_t list_obj, mp_obj_t seq_obj) {
    const int list = mp_obj_get_int(list_obj);
    size_t n = 0;
    mp_obj_t* items = NULL;
    mp_obj_get_array(seq_obj, &n, &items);

    const char* err = NULL;
    CalcStatus st = calc_api_list_resize(list, (int)n, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    double buf[CALC_CHUNK];
    for (size_t i = 0; i < n; i += CALC_CHUNK) {
        const size_t take = (n - i) < CALC_CHUNK ? (n - i) : CALC_CHUNK;
        for (size_t k = 0; k < take; ++k) {
            buf[k] = (double)mp_obj_get_float(items[i + k]);
        }
        st = calc_api_list_write(list, (int)i, (int)take, buf, &err);
        if (st != kCalcOk) {
            calc_raise(st, err);
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(calc_set_list_obj, calc_set_list);

static mp_obj_t calc_get_list(mp_obj_t list_obj) {
    const int list = mp_obj_get_int(list_obj);
    int n = 0;
    const char* err = NULL;
    CalcStatus st = calc_api_list_size(list, &n, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    // Allocated before any of the reads, so a GC pass here cannot happen
    // while a calc_api call is on the stack (D74).
    mp_obj_t out = mp_obj_new_list(0, NULL);
    double buf[CALC_CHUNK];
    for (int i = 0; i < n; i += CALC_CHUNK) {
        const int take = (n - i) < CALC_CHUNK ? (n - i) : CALC_CHUNK;
        st = calc_api_list_read(list, i, take, buf, &err);
        if (st != kCalcOk) {
            calc_raise(st, err);
        }
        for (int k = 0; k < take; ++k) {
            mp_obj_list_append(out, mp_obj_new_float((mp_float_t)buf[k]));
        }
    }
    return out;
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_get_list_obj, calc_get_list);

static mp_obj_t calc_list_append(mp_obj_t list_obj, mp_obj_t value_obj) {
    const char* err = NULL;
    const CalcStatus st =
        calc_api_list_append(mp_obj_get_int(list_obj), (double)mp_obj_get_float(value_obj), &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(calc_list_append_obj, calc_list_append);

static mp_obj_t calc_list_stat(const char* what, mp_obj_t list_obj) {
    double v = 0;
    const char* err = NULL;
    const CalcStatus st = calc_api_list_stat(mp_obj_get_int(list_obj), what, &v, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return mp_obj_new_float((mp_float_t)v);
}

static mp_obj_t calc_stat_mean(mp_obj_t l) {
    return calc_list_stat("mean", l);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_stat_mean_obj, calc_stat_mean);

static mp_obj_t calc_stat_sum(mp_obj_t l) {
    return calc_list_stat("sum", l);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_stat_sum_obj, calc_stat_sum);

static mp_obj_t calc_stat_min(mp_obj_t l) {
    return calc_list_stat("min", l);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_stat_min_obj, calc_stat_min);

static mp_obj_t calc_stat_max(mp_obj_t l) {
    return calc_list_stat("max", l);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_stat_max_obj, calc_stat_max);

static mp_obj_t calc_stat_stddev(mp_obj_t l) {
    return calc_list_stat("stddev", l);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_stat_stddev_obj, calc_stat_stddev);

// ---- 6B.7: matrices ----

// A nested Python list into the scratch matrix. Ragged input is rejected by
// calc_api_mat_write_row, which knows the column count it agreed to.
static void calc_matrix_in(mp_obj_t obj) {
    size_t rows = 0;
    mp_obj_t* row_items = NULL;
    mp_obj_get_array(obj, &rows, &row_items);
    if (rows == 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("matrix has no rows"));
    }
    size_t cols = 0;
    mp_obj_t* first = NULL;
    mp_obj_get_array(row_items[0], &cols, &first);

    const char* err = NULL;
    CalcStatus st = calc_api_mat_begin((int)rows, (int)cols, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    double buf[CALC_CHUNK];
    if (cols > CALC_CHUNK) {
        mp_raise_ValueError(MP_ERROR_TEXT("matrix too wide"));
    }
    for (size_t r = 0; r < rows; ++r) {
        size_t n = 0;
        mp_obj_t* items = NULL;
        mp_obj_get_array(row_items[r], &n, &items);
        for (size_t c = 0; c < n && c < CALC_CHUNK; ++c) {
            buf[c] = (double)mp_obj_get_float(items[c]);
        }
        st = calc_api_mat_write_row((int)r, (int)n, buf, &err);
        if (st != kCalcOk) {
            calc_raise(st, err);
        }
    }
}

// MatAns back out as a nested Python list.
static mp_obj_t calc_matrix_out(int rows, int cols) {
    mp_obj_t out = mp_obj_new_list(0, NULL);
    double buf[CALC_CHUNK];
    for (int r = 0; r < rows; ++r) {
        const char* err = NULL;
        const CalcStatus st = calc_api_mat_read_row(r, cols, buf, &err);
        if (st != kCalcOk) {
            calc_raise(st, err);
        }
        mp_obj_t row = mp_obj_new_list(0, NULL);
        for (int c = 0; c < cols; ++c) {
            mp_obj_list_append(row, mp_obj_new_float((mp_float_t)buf[c]));
        }
        mp_obj_list_append(out, row);
    }
    return out;
}

static mp_obj_t calc_matrix_op(const char* op, mp_obj_t m, int rhs) {
    calc_matrix_in(m);
    double scalar = 0;
    int rows = 0;
    int cols = 0;
    const char* err = NULL;
    const CalcStatus st = calc_api_mat_op(op, rhs, &scalar, &rows, &cols, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    if (rows == 0) {
        return mp_obj_new_float((mp_float_t)scalar);
    }
    return calc_matrix_out(rows, cols);
}

static mp_obj_t calc_det(mp_obj_t m) {
    return calc_matrix_op("det", m, -1);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_det_obj, calc_det);

static mp_obj_t calc_inverse(mp_obj_t m) {
    return calc_matrix_op("inverse", m, -1);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_inverse_obj, calc_inverse);

static mp_obj_t calc_transpose(mp_obj_t m) {
    return calc_matrix_op("transpose", m, -1);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_transpose_obj, calc_transpose);

static mp_obj_t calc_rref(mp_obj_t m) {
    return calc_matrix_op("rref", m, -1);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_rref_obj, calc_rref);

// Flat, not nested: matops::eigenvalues produces a 1-D list on purpose, and
// a script wants [3.0, 1.0] rather than [[3.0, 1.0]].
static mp_obj_t calc_eigenvalues(mp_obj_t m) {
    calc_matrix_in(m);
    double scalar = 0;
    int rows = 0;
    int cols = 0;
    const char* err = NULL;
    const CalcStatus st = calc_api_mat_op("eigenvalues", -1, &scalar, &rows, &cols, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    double buf[CALC_CHUNK];
    if (cols > CALC_CHUNK) {
        mp_raise_ValueError(MP_ERROR_TEXT("too many eigenvalues"));
    }
    const CalcStatus rd = calc_api_mat_read_row(0, cols, buf, &err);
    if (rd != kCalcOk) {
        calc_raise(rd, err);
    }
    mp_obj_t out = mp_obj_new_list(0, NULL);
    for (int c = 0; c < cols; ++c) {
        mp_obj_list_append(out, mp_obj_new_float((mp_float_t)buf[c]));
    }
    return out;
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_eigenvalues_obj, calc_eigenvalues);

// [A]-[J] as a letter, matching how they are written on the calculator.
static int calc_mat_slot(mp_obj_t o) {
    const char* s = mp_obj_str_get_str(o);
    if (s[0] >= 'A' && s[0] <= 'J' && s[1] == 0) {
        return s[0] - 'A';
    }
    if (s[0] >= 'a' && s[0] <= 'j' && s[1] == 0) {
        return s[0] - 'a';
    }
    mp_raise_ValueError(MP_ERROR_TEXT("matrix slot must be A-J"));
}

static mp_obj_t calc_set_matrix(mp_obj_t slot_obj, mp_obj_t m) {
    const int slot = calc_mat_slot(slot_obj);
    calc_matrix_in(m);
    const char* err = NULL;
    const CalcStatus st = calc_api_mat_store(slot, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(calc_set_matrix_obj, calc_set_matrix);

static mp_obj_t calc_get_matrix(mp_obj_t slot_obj) {
    const int slot = calc_mat_slot(slot_obj);
    int rows = 0;
    int cols = 0;
    const char* err = NULL;
    CalcStatus st = calc_api_mat_load(slot, &rows, &cols, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    // mat_load puts it in the scratch; transpose twice would be silly, so
    // read it back out through MatAns the same way every other op does.
    double scalar = 0;
    st = calc_api_mat_op("copy_out", -1, &scalar, &rows, &cols, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return calc_matrix_out(rows, cols);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_get_matrix_obj, calc_get_matrix);

static mp_obj_t calc_matmul(mp_obj_t m, mp_obj_t slot_obj) {
    return calc_matrix_op("mul", m, calc_mat_slot(slot_obj));
}
static MP_DEFINE_CONST_FUN_OBJ_2(calc_matmul_obj, calc_matmul);

// ---- 6B.8: the script canvas ----

// A colour argument: a name from the palette, or an (r, g, b) tuple. §4.2
// specifies both, because ~10 category colours do not fit a 6-entry named
// palette but a script drawing a chart still wants to say "blue".
static unsigned calc_color_arg(mp_obj_t o) {
    const char* name = NULL;
    int r = 0;
    int g = 0;
    int b = 0;
    if (mp_obj_is_str(o)) {
        name = mp_obj_str_get_str(o);
    } else {
        size_t n = 0;
        mp_obj_t* items = NULL;
        mp_obj_get_array(o, &n, &items);
        if (n != 3) {
            mp_raise_ValueError(MP_ERROR_TEXT("colour is a name or (r, g, b)"));
        }
        r = mp_obj_get_int(items[0]);
        g = mp_obj_get_int(items[1]);
        b = mp_obj_get_int(items[2]);
    }
    unsigned out = 0;
    const char* err = NULL;
    const CalcStatus st = calc_api_color(name, r, g, b, &out, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return out;
}

static unsigned calc_opt_color(size_t n_args, const mp_obj_t* args, size_t i, unsigned dflt) {
    return i < n_args ? calc_color_arg(args[i]) : dflt;
}

// Black and white as RGB565, so the defaults below need no lookup.
#define CALC_BLACK 0x0000u
#define CALC_WHITE 0xFFFFu

static mp_obj_t calc_clear_screen(size_t n_args, const mp_obj_t* args) {
    calc_api_canvas_clear(calc_opt_color(n_args, args, 0, CALC_BLACK));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_clear_screen_obj, 0, 1, calc_clear_screen);

static mp_obj_t calc_draw_pixel(size_t n_args, const mp_obj_t* args) {
    calc_api_canvas_pixel(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]),
                          calc_opt_color(n_args, args, 2, CALC_WHITE));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_draw_pixel_obj, 2, 3, calc_draw_pixel);

static mp_obj_t calc_draw_line(size_t n_args, const mp_obj_t* args) {
    calc_api_canvas_line(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), mp_obj_get_int(args[2]),
                         mp_obj_get_int(args[3]), calc_opt_color(n_args, args, 4, CALC_WHITE));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_draw_line_obj, 4, 5, calc_draw_line);

static mp_obj_t calc_draw_rect(size_t n_args, const mp_obj_t* args) {
    const int fill = n_args > 5 && mp_obj_is_true(args[5]);
    calc_api_canvas_rect(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), mp_obj_get_int(args[2]),
                         mp_obj_get_int(args[3]), calc_opt_color(n_args, args, 4, CALC_WHITE),
                         fill);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_draw_rect_obj, 4, 6, calc_draw_rect);

// The background colour is not optional in spirit: the canvas cannot read the
// panel back, so a glyph is drawn as a filled cell. It defaults to black
// rather than being required, which is what a script clearing to black wants.
static mp_obj_t calc_draw_text(size_t n_args, const mp_obj_t* args) {
    const int w = calc_api_canvas_text(
        mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), mp_obj_str_get_str(args[2]),
        calc_opt_color(n_args, args, 3, CALC_WHITE), calc_opt_color(n_args, args, 4, CALC_BLACK));
    return MP_OBJ_NEW_SMALL_INT(w);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_draw_text_obj, 3, 5, calc_draw_text);

static mp_obj_t calc_text_size(mp_obj_t s) {
    mp_obj_t pair[2] = {MP_OBJ_NEW_SMALL_INT(calc_api_canvas_text_width(mp_obj_str_get_str(s))),
                        MP_OBJ_NEW_SMALL_INT(calc_api_canvas_text_height())};
    return mp_obj_new_tuple(2, pair);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_text_size_obj, calc_text_size);

// ---- 6B.9: keyboard ----

// A key event as a Python dict — self-describing, and a script reads
// ev["ch"] without having to remember a tuple order.
static mp_obj_t calc_key_obj_from(const CalcKeyEvent* e) {
    mp_obj_t d = mp_obj_new_dict(6);
    mp_obj_dict_store(d, MP_OBJ_NEW_QSTR(MP_QSTR_code), MP_OBJ_NEW_SMALL_INT(e->code));
    // The field that makes an arrow key usable: `code` is an enum value Python
    // has no names for and `ch` is None for anything that is not a character,
    // so "which arrow was that" had no answer before this. "" for a key with
    // no name, never None, so a script can compare it directly.
    mp_obj_dict_store(
        d, MP_OBJ_NEW_QSTR(MP_QSTR_name),
        mp_obj_new_str(e->name != NULL ? e->name : "", e->name != NULL ? strlen(e->name) : 0));
    mp_obj_dict_store(d, MP_OBJ_NEW_QSTR(MP_QSTR_ch),
                      e->ch != 0 ? mp_obj_new_str((char[]){(char)e->ch}, 1) : mp_const_none);
    mp_obj_dict_store(d, MP_OBJ_NEW_QSTR(MP_QSTR_shift), mp_obj_new_bool(e->shift));
    mp_obj_dict_store(d, MP_OBJ_NEW_QSTR(MP_QSTR_ctrl), mp_obj_new_bool(e->ctrl));
    mp_obj_dict_store(d, MP_OBJ_NEW_QSTR(MP_QSTR_alt), mp_obj_new_bool(e->alt));
    return d;
}

static mp_obj_t calc_key_pressed(void) {
    CalcKeyEvent e;
    if (calc_api_key_pressed(&e) == 0) {
        return mp_const_none;
    }
    return calc_key_obj_from(&e);
}
static MP_DEFINE_CONST_FUN_OBJ_0(calc_key_pressed_obj, calc_key_pressed);

// Blocks. The VM hook does not run while we are here, so this drives the
// drain itself — see D81's refinement in calc_api.h. ESC ends the wait by
// raising, so a script waiting for input is never a reason to power-cycle.
static mp_obj_t calc_wait_key(void) {
    for (;;) {
        CalcKeyEvent e;
        if (calc_api_key_pressed(&e) != 0) {
            return calc_key_obj_from(&e);
        }
        if (picocalc_py_interrupt_requested() != 0) {
            mp_raise_type(&mp_type_KeyboardInterrupt);
        }
        // No sleep: calc_api_key_pressed drives the keyboard's own two-phase
        // I2C state machine, which spends >=10 ms a cycle (D7). The drain is
        // the rate limiter, so a delay here would only add latency.
    }
}
static MP_DEFINE_CONST_FUN_OBJ_0(calc_wait_key_obj, calc_wait_key);

static mp_obj_t calc_key_held(mp_obj_t name) {
    return mp_obj_new_bool(calc_api_key_held(mp_obj_str_get_str(name)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_key_held_obj, calc_key_held);

// A line editor on the canvas: echo, backspace, Enter. Deliberately minimal —
// a script wanting more can build it from wait_key.
static mp_obj_t calc_input(size_t n_args, const mp_obj_t* args) {
    const char* prompt = n_args > 0 ? mp_obj_str_get_str(args[0]) : "";
    const int y = n_args > 1 ? mp_obj_get_int(args[1]) : 0;
    char buf[64];
    int len = 0;
    const int px = calc_api_canvas_text(0, y, prompt, 0xFFFFu, 0x0000u);
    for (;;) {
        CalcKeyEvent e;
        if (calc_api_key_pressed(&e) == 0) {
            if (picocalc_py_interrupt_requested() != 0) {
                mp_raise_type(&mp_type_KeyboardInterrupt);
            }
            continue;
        }
        if (e.ch == '\r' || e.ch == '\n') {
            break;
        }
        if (e.ch == 8 || e.ch == 127) {
            if (len > 0) {
                buf[--len] = 0;
                // Repaint the field: no readback, so clearing means drawing
                // background over the row (D85).
                calc_api_canvas_rect(px, y, 320 - px, calc_api_canvas_text_height(), 0x0000u, 1);
                calc_api_canvas_text(px, y, buf, 0xFFFFu, 0x0000u);
            }
            continue;
        }
        if (e.ch >= 32 && e.ch < 127 && len < (int)sizeof(buf) - 1) {
            buf[len++] = (char)e.ch;
            buf[len] = 0;
            calc_api_canvas_text(px, y, buf, 0xFFFFu, 0x0000u);
        }
    }
    buf[len] = 0;
    return mp_obj_new_str(buf, (size_t)len);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calc_input_obj, 0, 2, calc_input);

// ---- 6B.10: file I/O ----

// D83: no staging buffer. The Python string is allocated first, at the file's
// size, and one pass fills it through read_file_range. io_scratch's one-shot
// invariant never comes into it, and nothing is capped below the heap.
static mp_obj_t calc_read_file(mp_obj_t path_obj) {
    const char* path = mp_obj_str_get_str(path_obj);
    long size = 0;
    const char* err = NULL;
    CalcStatus st = calc_api_file_size(path, &size, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    vstr_t vstr;
    vstr_init_len(&vstr, (size_t)size);
    long done = 0;
    while (done < size) {
        int got = 0;
        const int want = (int)((size - done) > 4096 ? 4096 : (size - done));
        st = calc_api_file_read(path, done, vstr.buf + done, want, &got, &err);
        if (st != kCalcOk || got <= 0) {
            vstr_clear(&vstr);
            calc_raise(st == kCalcOk ? kCalcFailed : st, err != NULL ? err : "Short read");
        }
        done += got;
    }
    return mp_obj_new_str_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_read_file_obj, calc_read_file);

static mp_obj_t calc_write_file_impl(mp_obj_t path_obj, mp_obj_t data_obj, int append) {
    size_t len = 0;
    const char* data = mp_obj_str_get_data(data_obj, &len);
    const char* err = NULL;
    const CalcStatus st =
        calc_api_file_write(mp_obj_str_get_str(path_obj), data, (int)len, append, &err);
    if (st != kCalcOk) {
        calc_raise(st, err);
    }
    return mp_const_none;
}

static mp_obj_t calc_write_file(mp_obj_t p, mp_obj_t d) {
    return calc_write_file_impl(p, d, 0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(calc_write_file_obj, calc_write_file);

static mp_obj_t calc_append_file(mp_obj_t p, mp_obj_t d) {
    return calc_write_file_impl(p, d, 1);
}
static MP_DEFINE_CONST_FUN_OBJ_2(calc_append_file_obj, calc_append_file);

static mp_obj_t calc_file_exists(mp_obj_t p) {
    return mp_obj_new_bool(calc_api_file_exists(mp_obj_str_get_str(p)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_file_exists_obj, calc_file_exists);

// ---- 6B.5: complex ----

// Python already has complex(); this exists so `calc.complex` reads the same
// as the rest of the module and so a script need not remember which of the
// two namespaces a given operation lives in.
static mp_obj_t calc_complex(mp_obj_t re_obj, mp_obj_t im_obj) {
    return mp_obj_new_complex(mp_obj_get_float(re_obj), mp_obj_get_float(im_obj));
}
static MP_DEFINE_CONST_FUN_OBJ_2(calc_complex_obj, calc_complex);

static mp_obj_t calc_c_abs(mp_obj_t z_obj) {
    mp_float_t re = 0;
    mp_float_t im = 0;
    mp_obj_get_complex(z_obj, &re, &im);
    return mp_obj_new_float((mp_float_t)calc_api_c_abs((double)re, (double)im));
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_c_abs_obj, calc_c_abs);

static mp_obj_t calc_c_arg(mp_obj_t z_obj) {
    mp_float_t re = 0;
    mp_float_t im = 0;
    mp_obj_get_complex(z_obj, &re, &im);
    return mp_obj_new_float((mp_float_t)calc_api_c_arg((double)re, (double)im));
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_c_arg_obj, calc_c_arg);

static mp_obj_t calc_c_conj(mp_obj_t z_obj) {
    mp_float_t re = 0;
    mp_float_t im = 0;
    mp_obj_get_complex(z_obj, &re, &im);
    double out_re = 0;
    double out_im = 0;
    calc_api_c_conj((double)re, (double)im, &out_re, &out_im);
    return mp_obj_new_complex((mp_float_t)out_re, (mp_float_t)out_im);
}
static MP_DEFINE_CONST_FUN_OBJ_1(calc_c_conj_obj, calc_c_conj);

// ---- The module ----

static const mp_rom_map_elem_t calc_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_calc)},
    {MP_ROM_QSTR(MP_QSTR_eval), MP_ROM_PTR(&calc_eval_obj)},
    {MP_ROM_QSTR(MP_QSTR_store), MP_ROM_PTR(&calc_store_obj)},
    {MP_ROM_QSTR(MP_QSTR_recall), MP_ROM_PTR(&calc_recall_obj)},
    {MP_ROM_QSTR(MP_QSTR_simplify), MP_ROM_PTR(&calc_simplify_obj)},
    {MP_ROM_QSTR(MP_QSTR_expand), MP_ROM_PTR(&calc_expand_obj)},
    {MP_ROM_QSTR(MP_QSTR_factor), MP_ROM_PTR(&calc_factor_obj)},
    {MP_ROM_QSTR(MP_QSTR_diff), MP_ROM_PTR(&calc_diff_obj)},
    {MP_ROM_QSTR(MP_QSTR_integ), MP_ROM_PTR(&calc_integ_obj)},
    {MP_ROM_QSTR(MP_QSTR_solve), MP_ROM_PTR(&calc_solve_obj)},
    {MP_ROM_QSTR(MP_QSTR_plot), MP_ROM_PTR(&calc_plot_obj)},
    {MP_ROM_QSTR(MP_QSTR_window), MP_ROM_PTR(&calc_window_obj)},
    {MP_ROM_QSTR(MP_QSTR_show_graph), MP_ROM_PTR(&calc_show_graph_obj)},
    {MP_ROM_QSTR(MP_QSTR_graph_zero), MP_ROM_PTR(&calc_graph_zero_obj)},
    {MP_ROM_QSTR(MP_QSTR_graph_min), MP_ROM_PTR(&calc_graph_min_obj)},
    {MP_ROM_QSTR(MP_QSTR_graph_max), MP_ROM_PTR(&calc_graph_max_obj)},
    {MP_ROM_QSTR(MP_QSTR_graph_integral), MP_ROM_PTR(&calc_graph_integral_obj)},
    {MP_ROM_QSTR(MP_QSTR_graph_deriv), MP_ROM_PTR(&calc_graph_deriv_obj)},
    {MP_ROM_QSTR(MP_QSTR_graph_value), MP_ROM_PTR(&calc_graph_value_obj)},
    {MP_ROM_QSTR(MP_QSTR_set_list), MP_ROM_PTR(&calc_set_list_obj)},
    {MP_ROM_QSTR(MP_QSTR_get_list), MP_ROM_PTR(&calc_get_list_obj)},
    {MP_ROM_QSTR(MP_QSTR_list_append), MP_ROM_PTR(&calc_list_append_obj)},
    {MP_ROM_QSTR(MP_QSTR_stat_mean), MP_ROM_PTR(&calc_stat_mean_obj)},
    {MP_ROM_QSTR(MP_QSTR_stat_sum), MP_ROM_PTR(&calc_stat_sum_obj)},
    {MP_ROM_QSTR(MP_QSTR_stat_min), MP_ROM_PTR(&calc_stat_min_obj)},
    {MP_ROM_QSTR(MP_QSTR_stat_max), MP_ROM_PTR(&calc_stat_max_obj)},
    {MP_ROM_QSTR(MP_QSTR_stat_stddev), MP_ROM_PTR(&calc_stat_stddev_obj)},
    {MP_ROM_QSTR(MP_QSTR_det), MP_ROM_PTR(&calc_det_obj)},
    {MP_ROM_QSTR(MP_QSTR_inverse), MP_ROM_PTR(&calc_inverse_obj)},
    {MP_ROM_QSTR(MP_QSTR_transpose), MP_ROM_PTR(&calc_transpose_obj)},
    {MP_ROM_QSTR(MP_QSTR_rref), MP_ROM_PTR(&calc_rref_obj)},
    {MP_ROM_QSTR(MP_QSTR_eigenvalues), MP_ROM_PTR(&calc_eigenvalues_obj)},
    {MP_ROM_QSTR(MP_QSTR_matmul), MP_ROM_PTR(&calc_matmul_obj)},
    {MP_ROM_QSTR(MP_QSTR_set_matrix), MP_ROM_PTR(&calc_set_matrix_obj)},
    {MP_ROM_QSTR(MP_QSTR_get_matrix), MP_ROM_PTR(&calc_get_matrix_obj)},
    {MP_ROM_QSTR(MP_QSTR_clear_screen), MP_ROM_PTR(&calc_clear_screen_obj)},
    {MP_ROM_QSTR(MP_QSTR_draw_pixel), MP_ROM_PTR(&calc_draw_pixel_obj)},
    {MP_ROM_QSTR(MP_QSTR_draw_line), MP_ROM_PTR(&calc_draw_line_obj)},
    {MP_ROM_QSTR(MP_QSTR_draw_rect), MP_ROM_PTR(&calc_draw_rect_obj)},
    {MP_ROM_QSTR(MP_QSTR_draw_text), MP_ROM_PTR(&calc_draw_text_obj)},
    {MP_ROM_QSTR(MP_QSTR_text_size), MP_ROM_PTR(&calc_text_size_obj)},
    {MP_ROM_QSTR(MP_QSTR_key_pressed), MP_ROM_PTR(&calc_key_pressed_obj)},
    {MP_ROM_QSTR(MP_QSTR_wait_key), MP_ROM_PTR(&calc_wait_key_obj)},
    {MP_ROM_QSTR(MP_QSTR_key_held), MP_ROM_PTR(&calc_key_held_obj)},
    {MP_ROM_QSTR(MP_QSTR_input), MP_ROM_PTR(&calc_input_obj)},
    {MP_ROM_QSTR(MP_QSTR_read_file), MP_ROM_PTR(&calc_read_file_obj)},
    {MP_ROM_QSTR(MP_QSTR_write_file), MP_ROM_PTR(&calc_write_file_obj)},
    {MP_ROM_QSTR(MP_QSTR_append_file), MP_ROM_PTR(&calc_append_file_obj)},
    {MP_ROM_QSTR(MP_QSTR_file_exists), MP_ROM_PTR(&calc_file_exists_obj)},
    {MP_ROM_QSTR(MP_QSTR_complex), MP_ROM_PTR(&calc_complex_obj)},
    {MP_ROM_QSTR(MP_QSTR_c_abs), MP_ROM_PTR(&calc_c_abs_obj)},
    {MP_ROM_QSTR(MP_QSTR_c_arg), MP_ROM_PTR(&calc_c_arg_obj)},
    {MP_ROM_QSTR(MP_QSTR_c_conj), MP_ROM_PTR(&calc_c_conj_obj)},
};
static MP_DEFINE_CONST_DICT(calc_module_globals, calc_module_globals_table);

const mp_obj_module_t calc_user_cmodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t*)&calc_module_globals,
};

// MP_REGISTER_MODULE expands to nothing outside the generator's scan, so the
// trailing ';' — which makeqstrdefs.py's regex requires in order to find the
// call at all — is left as a stray semicolon at file scope. Upstream builds
// without -Wpedantic and never sees it; this project does not.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
MP_REGISTER_MODULE(MP_QSTR_calc, calc_user_cmodule);
#pragma GCC diagnostic pop

// NOLINTEND(readability-identifier-naming,performance-no-int-to-ptr,cppcoreguidelines-interfaces-global-init,clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
