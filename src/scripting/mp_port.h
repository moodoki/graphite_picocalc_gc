#pragma once

// The C boundary between the firmware and MicroPython (Phase 6B).
//
// mp_port.c implements these and is the only translation unit that
// includes a MicroPython header; micropython_embed.cpp calls them and is
// the only one that knows anything about screens, heaps or keys. See the
// comment at the top of mp_port.c for why the seam is in C.
//
// Both sides include this so there is one declaration of each function
// rather than two that can drift.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- Implemented in mp_port.c, called from C++ ----

// heap/heap_size: the GC arena. stack_top: the high end of core 0's
// stack, which the GC scans down from for roots. stack_limit: how far
// below stack_top MicroPython may go before raising instead of running
// off the end (D47/D48).
void picocalc_mp_init(void* heap, size_t heap_size, void* stack_top, size_t stack_limit);
void picocalc_mp_deinit(void);

// 1 on success, 0 if the script raised. The traceback has already been
// printed through the output path by the time this returns.
int picocalc_mp_exec_str(const char* src);

size_t picocalc_mp_heap_free(void);

// ---- Implemented in micropython_embed.cpp, called from C ----

// Everything MicroPython writes to stdout, after it has gone to serial.
void picocalc_py_output(const char* str, size_t len);

// Polled from the VM's branch hook: nonzero means ESC was pressed and
// the script should be interrupted.
int picocalc_py_interrupt_requested(void);

// The VM hook itself. Declared here as well as in the macro that calls
// it (drivers/micropython_port/mpconfigport.h), so it has a declaration
// visible where it is defined.
void picocalc_mp_vm_hook(void);

#ifdef __cplusplus
}  // extern "C"
#endif
