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

// True if the previous boot ended in a hard fault; *pc receives the
// faulting instruction address. Clears the record, so the second call
// returns false.
bool take_prior_fault(uint32_t* pc);

}  // namespace platform
