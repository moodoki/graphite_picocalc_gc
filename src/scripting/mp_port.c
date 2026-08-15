// MicroPython HAL glue and runtime entry points (Phase 6B.1/6B.2).
//
// ALL MicroPython header use lives in this file, in C, on purpose:
//
//   * MicroPython's headers have no `extern "C"` guards, so including
//     them from C++ means wrapping every one of them by hand.
//   * MicroPython raises exceptions by longjmp, which unwinds straight
//     past every intervening frame. A C++ frame holding a
//     non-trivially-destructible object in that path leaks it silently,
//     with no compiler diagnostic. Keeping the boundary in C makes that
//     structural instead of something to remember — which will matter a
//     great deal from 6B.3, when `calc` bindings start calling back the
//     other way.
//
// micropython_embed.cpp owns the policy (heap, when to init, what ESC
// means); this file owns the plumbing. Same reason
// src/math/cephes_support.c is C.
//
// This also REPLACES the embed port's own port/mphalport.c, which sends
// stdout straight to printf — CMakeLists.txt drops that file from the
// generated source list.

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "port/micropython_embed.h"
#include "py/builtin.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "py/stackctrl.h"

#include "scripting/mp_port.h"

// ---- HAL ----

// print(), tracebacks, and everything else MicroPython writes. Fans out
// to two sinks: USB serial always (so a script is debuggable from a host
// with no display involved), and the on-device output pane when a screen
// has registered for it.
void mp_hal_stdout_tx_strn_cooked(const char* str, size_t len) {
    printf("%.*s", (int)len, str);
    picocalc_py_output(str, len);
}

// Called from the VM's branch hook (see mpconfigport.h). Raising here
// rather than returning a flag is what lets ESC break out of a loop that
// never calls back into our code: mp_sched_keyboard_interrupt() sets a
// pending exception which the VM checks immediately after this hook.
void picocalc_mp_vm_hook(void) {
    if (picocalc_py_interrupt_requested()) {
        mp_sched_keyboard_interrupt();
    }
}

// Required by MICROPY_KBD_EXCEPTION (it backs micropython.kbd_intr()).
// Inert here: interrupting a script is ESC through the VM hook above, not
// a character arriving on a serial stream. Kept so the feature — which is
// what makes KeyboardInterrupt a preallocated object, raisable from the
// hook without touching a possibly-full heap — can be switched on.
void mp_hal_set_interrupt_char(int c) {
    (void)c;
}

#if MICROPY_PY_IO
// py/modio.c:208 says outright that mp_builtin_open_obj "should be
// defined by port" — the core registers builtins `open` under
// MICROPY_PY_IO but only supplies an implementation when there is a VFS,
// which there is not. We only have the io module at all because
// json.dumps() writes through a StringIO.
//
// It raises rather than being quietly absent so a script that reaches
// for open() is told why instead of getting NameError. SD access reaches
// Python as calc.read_file/write_file (6B.10).
static mp_obj_t picocalc_builtin_open(size_t n_args, const mp_obj_t* args, mp_map_t* kwargs) {
    (void)n_args;
    (void)args;
    (void)kwargs;
    mp_raise_NotImplementedError(MP_ERROR_TEXT("no filesystem; use calc file I/O"));
}
// The symbol name is py/builtin.h's, not ours.
// NOLINTNEXTLINE(readability-identifier-naming)
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, picocalc_builtin_open);
#endif

// ---- Runtime ----

void picocalc_mp_init(void* heap, size_t heap_size, void* stack_top, size_t stack_limit) {
    mp_embed_init(heap, heap_size, stack_top);
    // Measured from stack_top, which is the linker's __StackTop rather
    // than a local — so the limit is an absolute floor in SCRATCH_Y and
    // does not shift with however deep the UI happened to be when the
    // interpreter came up. Overrunning it raises RuntimeError instead of
    // walking into core 1's stack (the D48 failure mode).
    mp_stack_set_limit(stack_limit);
}

void picocalc_mp_deinit(void) {
    mp_embed_deinit();
}

// 1 on success, 0 if the script raised. The exception has already been
// printed through mp_hal_stdout_tx_strn_cooked by the time this returns,
// which is why there is no error-string out-param.
//
// This is embed_util.c's mp_embed_exec_str with a return value; upstream's
// returns void, and "did it work" is exactly what the RUN key needs.
int picocalc_mp_exec_str(const char* src) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t* lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, strlen(src), 0);
        qstr source_name = lex->source_name;
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, true);
        mp_call_function_0(module_fun);
        nlr_pop();
        return 1;
    }
    mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
    return 0;
}

// Free heap bytes, for the program screen's status line and for the
// §4.4 measurement the spec asks for ("how much of the 40 KB does a
// modest dataset actually cost").
size_t picocalc_mp_heap_free(void) {
    gc_info_t info;
    gc_info(&info);
    return info.free;
}
