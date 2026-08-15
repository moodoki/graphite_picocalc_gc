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

#include "scripting/calc_api.h"

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
