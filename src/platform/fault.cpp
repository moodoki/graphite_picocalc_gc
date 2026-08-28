#include "platform/fault.hpp"

#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/platform.h"

// Linker-provided bounds of core 0's stack (SCRATCH_Y). Absolute symbols,
// so the address *is* the value — hence the array-typed extern. These must
// stay at file scope: inside an anonymous namespace they pick up internal
// linkage and no longer resolve against the linker script. The reserved
// double-underscore names are the linker script's, not ours.
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" char __StackBottom[];
extern "C" char __StackTop[];
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

namespace {

// Full crash frame. Two watchdog scratch words forced a packed depth field
// that could only report "implausible" — and implausible is exactly what the
// interesting faults turned out to be (SP outside the stack entirely, PC
// garbage: 0xe6fd6f2e, 0x998c9015, 0xa123117c all observed). Those are a
// corrupted return address, not a trapped overrun, and telling them apart
// needs the whole frame. .uninitialized_data is not zeroed at startup, so a
// warm reset carries it across.
struct CrashRecord {
    uint32_t magic;
    uint32_t r0, r1, r2, r3, r12;
    uint32_t lr;  // return address of the faulting function
    uint32_t pc;  // faulting instruction
    uint32_t xpsr;
    uint32_t sp;  // stack pointer at fault — where core 0 actually was
    uint32_t core;
    uint32_t streak;  // consecutive faulting boots
};
constexpr uint32_t kCrashMagic = 0xFA01C0DEu;

// Not zeroed at startup, so it survives the warm reset the handler triggers.
CrashRecord __uninitialized_ram(g_crash);

// A fault that recurs every boot makes the board unrecoverable over USB: it
// resets long before USB can enumerate, so there is no way in short of
// physically holding BOOTSEL. After this many consecutive faults, drop to
// BOOTSEL deliberately instead — then a reflash needs no button.
//
// Not hypothetical: an early version of paint_stack() below wrote into the
// MPU guard region on its first store and bricked the board exactly this
// way (2026-08-08).
constexpr uint32_t kFaultsBeforeBootsel = 3;

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
    const uint32_t streak = (g_crash.magic == kCrashMagic) ? (g_crash.streak + 1u) : 1u;

    // The exception frame the core pushed: r0 r1 r2 r3 r12 lr pc xpsr. Its
    // address is the stack pointer at fault, which is the field that finally
    // distinguished "went too deep" from "jumped to garbage".
    g_crash.r0 = frame[0];
    g_crash.r1 = frame[1];
    g_crash.r2 = frame[2];
    g_crash.r3 = frame[3];
    g_crash.r12 = frame[4];
    g_crash.lr = frame[5];
    g_crash.pc = frame[6];
    g_crash.xpsr = frame[7];
    g_crash.sp = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame));
    g_crash.core = get_core_num() & 1u;
    g_crash.streak = streak;
    g_crash.magic = kCrashMagic;

    if (streak >= kFaultsBeforeBootsel) {
        // Faulting every boot. Rebooting would just repeat it, so hand the
        // board to the bootrom: BOOTSEL mounts, picotool works, no button.
        reset_usb_boot(0, 0);
    }
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

namespace {

constexpr uint32_t kPaint = 0xC0DEC0DEu;

// The MPU stack guard occupies the lowest 32 bytes of the stack region
// (runtime_init_stack_guard.c rounds __StackBottom up to a 32-byte boundary
// and protects one subregion). It permits neither read nor write, so both
// the paint and the scan must start above it — painting from __StackBottom
// itself faults on the very first store and boot-loops the board, which is
// exactly what happened the first time this was written.
constexpr int kGuardBytes = 32;

// Leave the live frames alone: paint only well below the current SP.
constexpr int kLiveMargin = 256;

bool g_painted = false;

// Lowest paintable/scannable word: just above the guard, 4-byte aligned.
uint32_t* paint_floor() {
    char* p = __StackBottom + kGuardBytes;
    return reinterpret_cast<uint32_t*>(p);
}

}  // namespace

namespace platform {

void paint_stack() {
    uint32_t* const bottom = paint_floor();

    // Address of a local as a stand-in for SP — pointer arithmetic rather
    // than an int-to-pointer round trip, and it needs no inline asm.
    uint32_t here = 0;
    char* const sp_approx = reinterpret_cast<char*>(&here);
    char* const limit_raw = sp_approx - kLiveMargin;
    auto* const limit =
        reinterpret_cast<uint32_t*>(limit_raw - (reinterpret_cast<uintptr_t>(limit_raw) & 3u));
    if (limit <= bottom) {
        return;  // Already deep — painting would corrupt live frames.
    }
    // Interrupts off for the fill. USB is live by this point and a TinyUSB
    // IRQ can use more than kLiveMargin of stack; if one fired mid-paint its
    // frame would land inside the region being painted and get scribbled on.
    const uint32_t irq = save_and_disable_interrupts();
    for (uint32_t* p = bottom; p < limit; ++p) {
        *p = kPaint;
    }
    g_painted = true;
    restore_interrupts(irq);
}

char* stack_top() {
    return __StackTop;
}

uint32_t stack_total() {
    // Compared as integers, not subtracted as pointers: they are the two
    // bounds of one linker-defined region, but they are separate externs and
    // a pointer subtraction across them is formally undefined.
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(__StackTop) -
                                 reinterpret_cast<uintptr_t>(__StackBottom));
}

uint32_t stack_peak_used() {
    if (!g_painted) {
        return 0;
    }
    const uint32_t* const bottom = paint_floor();
    const auto* const top = reinterpret_cast<const uint32_t*>(__StackTop);
    for (const uint32_t* p = bottom; p < top; ++p) {
        if (*p != kPaint) {
            return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(top) -
                                         reinterpret_cast<uintptr_t>(p));
        }
    }
    return 0;
}

bool take_prior_fault(FaultInfo* out) {
    if (!watchdog_caused_reboot() || g_crash.magic != kCrashMagic) {
        // Clear regardless: a stale record must not surface behind some
        // later, unrelated watchdog reboot.
        g_crash.magic = 0;
        return false;
    }
    if (out != nullptr) {
        out->pc = g_crash.pc;
        out->lr = g_crash.lr;
        out->sp = g_crash.sp;
        out->core = g_crash.core;
        out->streak = g_crash.streak;
        // Depth is only meaningful if SP is actually inside core 0's stack;
        // outside it, "how deep" is the wrong question and 0 says so.
        const auto top = reinterpret_cast<uintptr_t>(__StackTop);
        const auto bottom = reinterpret_cast<uintptr_t>(__StackBottom);
        out->depth = (g_crash.sp >= bottom && g_crash.sp < top)
                         ? static_cast<uint32_t>(top - g_crash.sp)
                         : 0u;
    }
    // Deliberately does NOT clear the streak: clearing here would reset the
    // count on every boot, and the BOOTSEL escape could never trigger.
    // clear_fault_streak() does it, once the app has proven it can stay up.
    return true;
}

void clear_fault_streak() {
    g_crash.magic = 0;
}

}  // namespace platform
