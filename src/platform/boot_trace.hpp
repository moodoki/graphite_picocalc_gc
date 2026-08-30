#pragma once

#include <cstdint>

// Boot-stage trace (2026-08-30).
//
// The failure this exists for: a Pico 1 that powered off and then would not
// come back, twice, recovered both times by a reflash. The reflash was never
// the fix -- leaving the unit long enough to drain recovered it just as well
// -- so whatever was stuck was volatile, and held on a rail a normal
// power-off does not fully collapse. `3V3_OUT` is the Pico module's own
// regulator output and feeds the QSPI flash, the PSRAM (U301) and the SD
// card; a chip on it left un-reset while the RP2040 does reset is the
// working hypothesis.
//
// Which chip it was cannot be told apart after the fact, because a wedge
// inside platform::init() looks exactly like a board that never started:
// dark panel, dead keys, and no serial (boot printfs race USB enumeration by
// ~2 s and are lost on precisely the boot worth seeing).
//
// So: record how far bring-up got, in RAM that survives a warm reset, and
// put a watchdog behind the one stretch of the boot path that can block
// forever. psram_spi_init()'s PIO and DMA waits have no timeout at all
// (drivers/rp2040-psram/psram_spi.h) and nothing was covering them -- the
// bulk test's watchdog arms later, inside run_self_tests(). A wedge now
// reboots instead of hanging, and the next boot can say where the last one
// died.
//
// Same mechanism as fault.cpp's crash record, and for the same reason. Kept
// out of that file because it answers the opposite question: fault.cpp is
// about execution that trapped, this is about execution that never arrived.
namespace platform {

// Bring-up stages, in the order platform::init() runs them. The ordering is
// load-bearing -- "wedged at kPsram" is a different investigation from
// "wedged at kStorage", and that distinction is the whole point.
enum class BootStage : uint32_t {
    kEntry = 0,  // main() reached; nothing initialized yet
    kStdio,      // stdio_init_all() -- USB CDC bring-up
    kKeyboard,   // keyboard().init() -- brings up the STM32 I2C bus
    kDisplay,    // display().init() -- panel, then backlight on
    kPsram,      // psram().init() -- the unbounded-wait suspect
    kStorage,    // storage().init() -- SD mount
    kSelfTest,   // run_self_tests()
    kStateLoad,  // persisted state off the card; watchdog is off from here
    kRunning,    // main loop reached; boot is over
};

const char* boot_stage_name(BootStage stage);

// Call once, first thing in main() -- ahead of stdio_init_all(), not after it.
//
// That ordering was wrong in the first cut and hardware said so (2026-08-30,
// second occurrence). A wedge inside USB bring-up leaves no serial to report
// itself *and*, with the watchdog armed later, nothing to cut it short: the
// board simply stops. The signature is unmistakable once you know it -- no USB
// device of any kind, yet BOOTSEL still mounts, because the bootrom is fine
// and it is our first few instructions that are not. Arming here means every
// line of main() is covered.
//
// Still not covered: anything before main() -- runtime init and static
// constructors. If a hang ever survives this change without rebooting, that
// absence is itself the bisection result.
//
// Captures the previous boot's verdict first (reading the reason bits before
// anything can consume them, the rule take_prior_fault() established), then
// arms the boot watchdog and marks this boot kEntry.
void boot_trace_begin();

// Advance the trace and pet the watchdog. Two stores and a register write.
//
// kStateLoad disarms instead of petting: everything past it goes through
// FatFs over deadline-guarded SPI, where a slow card legitimately takes
// seconds. Guarding that would trade a hang we can already bound for a
// reboot loop we could not.
void boot_stage(BootStage stage);

// Main loop reached: clears the wedge streak and makes sure the boot
// watchdog is off. From here the app's own timeouts are in charge.
void boot_trace_end();

// The previous boot's verdict, captured by boot_trace_begin(). True when the
// last boot was cut short by the boot watchdog; *stage receives where it
// died, *streak how many consecutive boots have died there.
bool prior_boot_wedged(BootStage* stage, uint32_t* streak);

// True when USB stdio bring-up has wedged on enough consecutive boots that
// this one should skip it. Skipping costs the serial console for that boot --
// which is why prior_boot_wedged() is also reported on the panel, the only
// channel left when this is the stage that failed.
bool skip_stdio_this_boot();

// True when PSRAM bring-up has wedged on enough consecutive boots that this
// one should skip it. PSRAM is optional and the status bar already shows it
// red (D26), so a board that cannot get past psram().init() should still
// reach the calculator rather than reboot-loop out of reach. Same intent as
// fault.cpp's kFaultsBeforeBootsel: an automatic escape from a loop that
// would otherwise need a human with a USB cable.
//
// Skipping is for the whole boot, not just init(): Psram::reinit() refuses
// an instance that was never configured, so main()'s late-init loop cannot
// revive it. Reaching the main loop clears the streak, so the next power
// cycle tries PSRAM again from scratch.
bool skip_psram_this_boot();

}  // namespace platform
