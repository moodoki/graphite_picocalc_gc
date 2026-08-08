#include "platform/fault.hpp"

#include "hardware/watchdog.h"

namespace {

// Distinctive enough that an uninitialized scratch register at cold
// power-on will not be mistaken for a fault record.
constexpr uint32_t kFaultMarker = 0xFA017EDDu;

}  // namespace

extern "C" {

// Records the fault site and reboots. Split out of the handler so the
// capture itself can be ordinary C++ — the naked wrapper exists only to
// hand over the stack pointer before the compiler touches it.
//
// `frame` is the exception frame the core pushed on entry:
// r0 r1 r2 r3 r12 lr pc xpsr. Note this only works because the push
// itself succeeded — a guard hit leaves SP inside the 32-byte guard
// window, and the frame lands just below it in ordinary RAM.
void fault_capture(const uint32_t* frame) {
    watchdog_hw->scratch[3] = frame[6];  // stacked PC
    watchdog_hw->scratch[2] = kFaultMarker;
    watchdog_reboot(0, 0, 0);
    while (true) {}
}

// Overrides the SDK's weak infinite-loop handler. Reads MSP because
// nothing here ever runs on the process stack.
__attribute__((naked)) void isr_hardfault() {
    __asm volatile(
        "mrs r0, msp\n"
        "bl fault_capture\n");
}

}  // extern "C"

namespace platform {

bool take_prior_fault(uint32_t* pc) {
    if (!watchdog_caused_reboot() || watchdog_hw->scratch[2] != kFaultMarker) {
        // Clear regardless: a stale record must not surface behind some
        // later, unrelated watchdog reboot.
        watchdog_hw->scratch[2] = 0;
        return false;
    }
    if (pc != nullptr) {
        *pc = watchdog_hw->scratch[3];
    }
    watchdog_hw->scratch[2] = 0;
    return true;
}

}  // namespace platform
