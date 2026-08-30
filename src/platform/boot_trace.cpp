#include "platform/boot_trace.hpp"

#include <cstdio>

#include "hardware/watchdog.h"
#include "pico/platform.h"

namespace {

// Not zeroed at startup, so it survives the warm reset the boot watchdog
// triggers -- the same __uninitialized_ram trick fault.cpp's crash record
// uses. The two records are independent; neither reads the other.
struct BootRecord {
    uint32_t magic;
    uint32_t stage;   // BootStage this boot has reached
    uint32_t streak;  // consecutive boots that died before kRunning
};
constexpr uint32_t kBootMagic = 0xB007'5A6Eu;

BootRecord __uninitialized_ram(g_boot);

// Long enough that a healthy boot never trips it -- bring-up is milliseconds,
// and the slowest leg is an SD mount over deadline-guarded SPI at a few
// hundred -- and short enough that kWedgesBeforeSkip of them is 15 s rather
// than a minute. Also clear of the RP2040's ~8.3 s watchdog_enable() ceiling.
constexpr uint32_t kBootWatchdogMs = 5'000;

// After this many consecutive wedges, boot without the stage that wedged
// rather than reboot into it again.
constexpr uint32_t kWedgesBeforeSkip = 3;

// This boot's read of the previous one. Captured once in boot_trace_begin()
// and then immutable, because the record itself is overwritten immediately
// afterwards.
bool g_prior_wedge = false;
platform::BootStage g_prior_stage = platform::BootStage::kEntry;
uint32_t g_prior_streak = 0;

void disarm() {
    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
}

}  // namespace

namespace platform {

const char* boot_stage_name(BootStage stage) {
    switch (stage) {
        case BootStage::kEntry:
            return "entry";
        case BootStage::kKeyboard:
            return "keyboard";
        case BootStage::kDisplay:
            return "display";
        case BootStage::kPsram:
            return "psram";
        case BootStage::kStorage:
            return "storage";
        case BootStage::kSelfTest:
            return "self-test";
        case BootStage::kStateLoad:
            return "state-load";
        case BootStage::kRunning:
            return "running";
    }
    // Reachable: stage comes out of uninitialized RAM, so it is only as
    // trustworthy as the magic that guards it.
    return "?";
}

void boot_trace_begin() {
    // A boot that ended early is only a wedge if a watchdog actually cut it
    // short. Without that check, a user who powers the unit off mid-boot
    // would be reported as one -- and D65 established that a real
    // power-cycle reads false here.
    const bool wedged = g_boot.magic == kBootMagic && watchdog_caused_reboot() &&
                        g_boot.stage != static_cast<uint32_t>(BootStage::kRunning);
    if (wedged) {
        g_prior_wedge = true;
        g_prior_stage = static_cast<BootStage>(g_boot.stage);
        g_prior_streak = g_boot.streak + 1u;
    }

    g_boot.magic = kBootMagic;
    g_boot.streak = g_prior_streak;
    g_boot.stage = static_cast<uint32_t>(BootStage::kEntry);

    watchdog_enable(kBootWatchdogMs, true);
}

void boot_stage(BootStage stage) {
    g_boot.stage = static_cast<uint32_t>(stage);
    if (stage == BootStage::kStateLoad) {
        disarm();
    } else {
        watchdog_update();
    }
    // The optimistic half of the report: on a boot that goes on to wedge,
    // USB has not enumerated yet and nobody sees this. prior_boot_wedged()
    // on the next boot's heartbeat is the half that survives.
    printf("boot: stage %s\n", boot_stage_name(stage));
}

void boot_trace_end() {
    g_boot.stage = static_cast<uint32_t>(BootStage::kRunning);
    g_boot.streak = 0;
    // Already off if boot_stage(kStateLoad) ran, and off again here because
    // reaching the main loop is the one place that must be unconditional.
    disarm();
}

bool prior_boot_wedged(BootStage* stage, uint32_t* streak) {
    if (!g_prior_wedge) {
        return false;
    }
    if (stage != nullptr) {
        *stage = g_prior_stage;
    }
    if (streak != nullptr) {
        *streak = g_prior_streak;
    }
    return true;
}

bool skip_psram_this_boot() {
    return g_prior_wedge && g_prior_stage == BootStage::kPsram &&
           g_prior_streak >= kWedgesBeforeSkip;
}

}  // namespace platform
