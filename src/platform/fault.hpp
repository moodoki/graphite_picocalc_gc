#pragma once

#include <cstdint>

// Hard-fault capture (D47).
//
// With PICO_USE_STACK_GUARDS enabled, a core-0 stack overrun traps at
// __StackBottom instead of quietly chewing through core 1's stack. But
// the SDK's default handler is an infinite loop, which on this hardware
// is indistinguishable from the very lockup we are trying to diagnose —
// screen frozen, keys dead, power cycle. So take the fault, stash where
// it happened in a watchdog scratch register (those survive a warm
// reset), reboot, and report it on the way back up.
//
// Same marker-across-reboot trick main.cpp already uses for the PSRAM
// bulk-test wedge, on the scratch registers that test doesn't claim:
// 0/1 are the bulk test's, 4-7 belong to the boot ROM's watchdog-vector
// protocol, leaving 2/3.
namespace platform {

// Registers and stack pointer captured at the fault. SP is the field that
// matters most here: inside core 0's stack means it went too deep, outside
// it means execution had already gone wild (a corrupted return address), and
// the two want completely different investigations.
struct FaultInfo {
    uint32_t pc = 0;
    uint32_t lr = 0;
    uint32_t sp = 0;
    uint32_t core = 0;
    uint32_t depth = 0;   // bytes of core 0 stack used, 0 if SP is outside it
    uint32_t streak = 0;  // consecutive faulting boots
};

// True if the previous boot ended in a hard fault, filling *out. Does not
// clear the consecutive-fault streak — see clear_fault_streak().
bool take_prior_fault(FaultInfo* out);

// Forget the consecutive-fault streak. Call once the app has been up long
// enough to count as healthy — take_prior_fault() deliberately leaves the
// streak alone, or a fault recurring every boot would reset its own count
// and never reach the BOOTSEL escape.
void clear_fault_streak();

// ---- Core-0 stack high-water mark ----
//
// The guard says *that* the stack overflowed; this says how close any
// given operation came, which is what you need to size a depth cap or
// decide whether a frame is worth shrinking. Static frame analysis kept
// disagreeing with the hardware during D47 — measure instead.
//
// paint_stack() fills the unused part of core 0's stack with a sentinel;
// call it early in main, while the stack is shallow. stack_peak_used()
// then scans up from __StackBottom for the first word that no longer
// holds the sentinel: the deepest point reached since the paint.
//
// Only core 0. Core 1's stack is painted by nothing — it runs one small
// service loop, and it is the *victim* of overruns here, not a source.
void paint_stack();

// Bytes of core 0's stack used at the deepest point since paint_stack(),
// and the total it has (4 KB — SCRATCH_Y, with core 1's stack directly
// below). A result at or near the total means it has been overrunning.
uint32_t stack_peak_used();
uint32_t stack_total();

}  // namespace platform
