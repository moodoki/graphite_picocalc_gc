/* MicroPython embed-port configuration for Graphite (Phase 6B.1).
 *
 * This is the ONE file that decides what the on-device Python actually
 * is. It is read twice: once by the host compiler during qstr/genhdr
 * generation, and once by arm-none-eabi when the generated tree is
 * compiled. Both reads must see the same thing, which is why the
 * configuration lives here and not in CMake defines.
 *
 * Derived from micropython/examples/embedding/mpconfigport.h (MIT).
 * The submodule at drivers/micropython/ is never edited; this file is
 * the whole of our local configuration.
 */

#include <port/mpconfigport_common.h>

// The embed port's own mphalport.h declares nothing (it is one line), so
// anything above the minimum ROM level calls HAL functions that were
// never declared. Repoint it at ours, which includes theirs and adds the
// declarations — the submodule stays untouched.
#undef MICROPY_MPHALPORT_H
#define MICROPY_MPHALPORT_H "picocalc_mphal.h"

// CORE_FEATURES is upstream's own default and the smallest level that
// still has a compiler, so scripts can be typed on the device instead
// of cross-compiled to .mpy on a host. Raising this costs mostly flash
// (22% used) but also qstr-pool and module-table SRAM, which is the
// scarce resource here — 21 KB of Pico 1 spare after the 40 KB heap.
// Raise it only against a size-report measurement.
#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES)

#define MICROPY_ENABLE_COMPILER (1)
#define MICROPY_ENABLE_GC (1)
#define MICROPY_PY_GC (1)

// gc.mem_free()/mem_alloc() — the spec (§4.4) explicitly wants a real
// heap measurement from a script, not an estimate.
#define MICROPY_PY_SYS (0)

// No filesystem behind the interpreter yet. Both of these reach for a
// VFS (mp_import_stat, mp_lexer_new_from_file, mp_builtin_open_obj) that
// this firmware does not implement — SD access goes through
// platform::Storage and reaches Python as calc.read_file/write_file
// (6B.10), not as builtin open(). `import` of an SD module arrives with
// the SD app manifests in 6B.15/6B.16; until then a script is one file.
#define MICROPY_ENABLE_EXTERNAL_IMPORT (0)

// json.loads()/dumps(). Not part of the py/ core — micropython_embed.mk
// adds extmod/modjson.c to both the qstr scan and the package copy. The
// spec asks for it in 6B.1 (§5) because §4.6's periodic-table app parses
// a bundled dataset with it.
#define MICROPY_PY_JSON (1)

// json.dumps() writes through a StringIO, which lives behind
// MICROPY_PY_IO — so json costs us the io module whether we want it or
// not. What comes with it is a builtin `open()` that py/modio.c
// explicitly leaves to the port to define; ours raises, because SD
// access goes through platform::Storage and reaches Python as
// calc.read_file/write_file (6B.10), not as a Python file object.
// IOBase is the one piece we can decline: it exists to let a Python
// class implement a stream, and nothing here does.
#define MICROPY_PY_IO (1)
#define MICROPY_PY_IO_IOBASE (0)

// Doubles, not floats. The calculator is `double` end to end (calc_t,
// math::Engine, the CAS), and 6B.3's calc.eval() will hand values
// across that boundary; a float Python would silently round every one
// of them. The Pico 1 has no FPU, so this is paid in soft-float calls.
#define MICROPY_FLOAT_IMPL (MICROPY_FLOAT_IMPL_DOUBLE)

// Named exceptions and argument details in error messages. TERSE is the
// CORE_FEATURES default and produces messages like "can't convert" with
// no indication of what or where — unhelpful on a 320x320 screen with
// no host to re-run on.
#define MICROPY_ERROR_REPORTING (MICROPY_ERROR_REPORTING_NORMAL)

// Core 0 has a 4 KB stack (SCRATCH_Y, D47) and MicroPython's parser and
// compiler both recurse. With this on, a too-deep script raises
// RuntimeError; with it off, it walks off the bottom of the stack into
// core 1's and hangs the machine (the D48 failure mode). Not optional.
#define MICROPY_STACK_CHECK (1)

// KeyboardInterrupt as a preallocated exception object, so it can be
// raised from the VM hook below without allocating on a possibly-full
// heap. Defaults on only at EXTRA_FEATURES, so it is asked for here.
#define MICROPY_KBD_EXCEPTION (1)

// ESC must be able to stop a running script. Without this a `while
// True:` typed by the user is an unbreakable hang on a battery device
// whose reset button is inside the case. The hook fires after branch
// opcodes (py/vm.c), so a loop of any shape reaches it; the divisor
// keeps the keyboard poll off the per-instruction path.
#define MICROPY_VM_HOOK_COUNT (128)
#define MICROPY_VM_HOOK_INIT static unsigned int vm_hook_divisor = MICROPY_VM_HOOK_COUNT;
#define MICROPY_VM_HOOK_POLL                        \
    if (--vm_hook_divisor == 0) {                   \
        vm_hook_divisor = MICROPY_VM_HOOK_COUNT;    \
        extern void picocalc_mp_vm_hook(void);      \
        picocalc_mp_vm_hook();                      \
    }
#define MICROPY_VM_HOOK_LOOP MICROPY_VM_HOOK_POLL
#define MICROPY_VM_HOOK_RETURN MICROPY_VM_HOOK_POLL
