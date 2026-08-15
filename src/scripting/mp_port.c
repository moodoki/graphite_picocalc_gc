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
#include "py/reader.h"
#include "py/runtime.h"
#include "py/stackctrl.h"

#include "scripting/calc_api.h"
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

// ---- Running a script straight off the card (6B.15/6B.16) ----
//
// The source is NOT read into a buffer first. MicroPython's lexer pulls
// through mp_reader_t a byte at a time, so a 128-byte window over the
// file is the entire cost of an SD app's source, whatever its size —
// which is what the spec was worried about when it deferred exec_file to
// here ("would cost a second 4 KB staging buffer"). It costs 144 bytes,
// and there is no cap on script length.
//
// What is still bounded by the 40 KB heap is the PARSE TREE and the
// bytecode, which is inherent to running Python at all and is the same
// bound the RUN key already has.
//
// The whole file is drained during mp_parse, before a line of user code
// runs, so this reader never overlaps with a script's own calc.read_file.

typedef struct {
    const char* path;  // borrowed; must outlive the call (see below)
    long offset;       // next byte to fetch from the file
    int len;           // valid bytes in buf
    int pos;           // next byte to hand out of buf
    char buf[128];
} picocalc_script_reader_t;

// One at a time, matching the one-interpreter invariant: a script cannot
// launch another app, so exec_file never nests.
static picocalc_script_reader_t g_script_reader;

static mp_uint_t script_readbyte(void* data) {
    picocalc_script_reader_t* r = (picocalc_script_reader_t*)data;
    if (r->pos >= r->len) {
        int got = 0;
        const char* err = NULL;
        if (calc_api_file_read(r->path, r->offset, r->buf, (int)sizeof(r->buf), &got, &err) !=
                kCalcOk ||
            got <= 0) {
            // Also the genuine end of the file: read past the end
            // returns 0 bytes, which is EOF and not an error. A read
            // that fails mid-file therefore truncates the script rather
            // than reporting a fault — the parse then fails on its own,
            // which is a diagnostic the user actually sees.
            return MP_READER_EOF;
        }
        r->offset += got;
        r->len = got;
        r->pos = 0;
    }
    return (mp_uint_t)(unsigned char)r->buf[r->pos++];
}

static void script_reader_close(void* data) {
    (void)data;  // nothing is held open; each read is a fresh open/seek
}

// 1 ok, 0 the script raised (traceback already printed), -1 the file
// could not be read at all.
//
// `path` is borrowed for the duration — it becomes the traceback's
// source name and backs the reader. Callers pass a pointer into the
// permanent SdAppManifest table (§4.5), not a stack buffer.
int picocalc_mp_exec_file(const char* path) {
    if (path == NULL || calc_api_file_exists(path) == 0) {
        return -1;
    }
    g_script_reader.path = path;
    g_script_reader.offset = 0;
    g_script_reader.len = 0;
    g_script_reader.pos = 0;

    mp_reader_t reader = {&g_script_reader, script_readbyte, script_reader_close};

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        // The path, not <stdin>, so a traceback from an SD app names the
        // file it came from. Interning is idempotent, so launching the
        // same app repeatedly does not grow the qstr pool.
        qstr source_name = qstr_from_str(path);
        mp_lexer_t* lex = mp_lexer_new(source_name, reader);
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
        // is_repl = FALSE, unlike exec_str above. A file is a module, not
        // something someone just typed: in REPL mode every top-level
        // expression statement prints its own value, so an app that calls
        // calc.draw_text five times emitted five glyph advances
        // ("176 192 168 168 216", HW 2026-08-16) into the output pane, with
        // no way for the script to suppress them. exec_str keeps REPL
        // semantics on purpose — that is what makes `py 1+1` at the home
        // screen show 2.
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, false);
        mp_call_function_0(module_fun);
        nlr_pop();
        return 1;
    }
    mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
    return 0;
}

// Reclaim garbage without going through Python.
//
// This is what stops a heap exhaustion from being permanent. Every
// statement has to be COMPILED before it runs, and compiling allocates —
// so once the heap is full, `gc.collect()` fails while being parsed, long
// before it could collect anything. Measured on 2026-08-15: after one
// MemoryError, every subsequent `py` line failed, including `import gc`,
// with several KB of unreachable garbage sitting in the heap and no way
// to reach it from Python. Only a power cycle cleared it.
//
// Calling gc_collect() from C needs no compiler and no allocation, so it
// works exactly in the state where Python cannot.
void picocalc_mp_gc_collect(void) {
    gc_collect();
}

// Free heap bytes, for the program screen's status line and for the
// §4.4 measurement the spec asks for ("how much of the 40 KB does a
// modest dataset actually cost").
size_t picocalc_mp_heap_free(void) {
    gc_info_t info;
    gc_info(&info);
    return info.free;
}

// The largest CONTIGUOUS free run, in bytes — a different question from
// heap_free, and the one that decides whether anything can still run.
//
// MicroPython's GC is mark-and-sweep with no compaction, so a heap can be
// mostly free and still unable to satisfy a modest request. Measured on
// 2026-08-15: after a 400-iteration loop that built expression strings,
// 31.5 KB was free and a 512-byte allocation failed. Every surviving float
// pinned a block, and the freed strings between them left nothing long
// enough to compile another statement.
//
// gc_info reports `used` and `free` in bytes but leaves `max_free` in
// blocks (gc.c:818-819 multiplies the first two and not the third); the
// conversion here is that asymmetry, not an error.
size_t picocalc_mp_heap_max_free(void) {
    gc_info_t info;
    gc_info(&info);
    return info.max_free * MICROPY_BYTES_PER_GC_BLOCK;
}
