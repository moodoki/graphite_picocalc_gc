#include "platform/system.hpp"

#include <cstdio>

#include "hardware/i2c.h"
#include "pico/time.h"

#include "platform/keyboard.hpp"

extern "C" {
#include "i2ckbd/i2ckbd.h"
}

namespace platform {

namespace {

// Battery register on the STM32 (same device as the keyboard FIFO).
constexpr uint8_t kRegBattery = 0x0B;

// The STM32's bit-banged I2C slave NACKs transactions that arrive
// back-to-back (observed on HW 2026-07-11: the vendored read_battery()
// failed reliably when called right after a keyboard FIFO read, with
// 500 ms timeouts — so it's pacing, not speed). Give it a gap before
// the register select, and space retries out.
constexpr uint32_t kPreGapMs = 5;
constexpr uint32_t kSelectToReadMs = 16;  // Same as the vendored driver
constexpr uint32_t kRetryGapMs = 10;
constexpr int kAttempts = 3;
constexpr uint32_t kI2cTimeoutUs = 100'000;  // Matches keyboard.cpp

// One paced register read: gap, select reg, wait, read 2 bytes.
// Returns the raw 16-bit value, or -1 on failure. Blocks ~21 ms on
// success. Do not call directly from UI code — use battery_status().
//
// HW note: STM32 keyboard fw before v1.6 lacks the battery register
// (0x0B times out; the indicator shows "--" via the failure cap).
// Verified working on this unit after updating to v1.6 (2026-07-11).
int read_stm32_reg(uint8_t reg_id) {
    sleep_ms(kPreGapMs);
    if (i2c_write_timeout_us(I2C_KBD_MOD, I2C_KBD_ADDR, &reg_id, 1, false, kI2cTimeoutUs) < 0) {
        return -1;
    }
    sleep_ms(kSelectToReadMs);
    uint8_t buf[2] = {0, 0};
    if (i2c_read_timeout_us(I2C_KBD_MOD, I2C_KBD_ADDR, buf, 2, false, kI2cTimeoutUs) < 0) {
        return -1;
    }
    return (buf[1] << 8) | buf[0];
}

}  // namespace

BatteryInfo read_battery_info() {
    // Do not use the vendored read_battery(): it NACKs under our poll
    // cadence and printf-spams the serial log (drivers are read-only —
    // workaround lives here, per AGENTS.md).
    BatteryInfo info;
    for (int i = 0; i < kAttempts; ++i) {
        const int raw = read_stm32_reg(kRegBattery);
        if (raw > 0) {
            // High byte = value: bits 0-6 percentage, bit 7 charging.
            // The low byte is just the echoed register ID — reading the
            // charging flag from it (pre-2026-07-18) meant charging
            // could never show. Raw is printed so the layout can be
            // confirmed on HW with the battery below full.
            info.percent = (raw >> 8) & 0x7F;
            info.charging = (raw & 0x8000) != 0;
            // At the 5 s poll cadence an unconditional print clutters
            // the serial log; print on change (that's the interesting
            // raw for the charging-bit confirmation) plus a 30 s
            // heartbeat.
            static int last_printed_raw = -1;
            static uint32_t last_print_ms = 0;
            const uint32_t print_now = uptime_ms();
            if (raw != last_printed_raw || print_now - last_print_ms >= 30'000) {
                last_printed_raw = raw;
                last_print_ms = print_now;
                printf("battery: raw=0x%04x pct=%d chg=%d\n", static_cast<unsigned>(raw),
                       info.percent, info.charging ? 1 : 0);
            }
            return info;
        }
        sleep_ms(kRetryGapMs);
    }
    return info;
}

namespace {
BatteryInfo g_battery_cache;
}  // namespace

BatteryInfo battery_status() {
    // Cache only — never touches I2C. Render code (status bar, diag
    // screen) calls this per strip (~20x/frame on Pico 1); any bus
    // traffic here caused mid-render stalls and hammered the STM32
    // into NACKing (seen on HW). All refreshes go through
    // battery_poll() from the main loop.
    return g_battery_cache;
}

BatteryInfo battery_poll() {
    static uint32_t last_attempt_ms = 0;
    static bool attempted = false;
    static int consecutive_failures = 0;

    // Refresh every 5 s; after a failure, retry no sooner than 10 s.
    // Used to be 30 s, which capped status-bar freshness at ~31 s no
    // matter how often the UI repainted (offline spin, HW 2026-07-18).
    // 5 s (not 1 s) is a deliberate stability margin: a wedged STM32
    // needs a physical power cycle, and a quicker charging indicator
    // isn't worth risking that (developer call, 2026-07-18). One paced
    // ~21 ms read per 5 s from a single call site is far from the
    // per-render hammering that provoked NACKs.
    // After several straight failures, give up until reboot: units with
    // pre-battery-register STM32 firmware time out every attempt, and
    // each failed attempt blocks for hundreds of ms.
    constexpr uint32_t kRefreshMs = 5'000;
    constexpr uint32_t kFailRetryMs = 10'000;
    constexpr int kMaxConsecutiveFailures = 5;
    // On cold power-on the STM32 is still booting when the first frame
    // renders, so the first read fails and the 10 s backoff left "--"
    // in the status bar until a much later keypress (HW 2026-07-11).
    // Until the first success, retry quickly and don't count failures
    // toward the give-up cap.
    constexpr uint32_t kBootGraceMs = 10'000;
    constexpr uint32_t kBootRetryMs = 2'000;

    if (consecutive_failures >= kMaxConsecutiveFailures) {
        return g_battery_cache;
    }
    const uint32_t now = uptime_ms();
    const bool in_boot_grace = g_battery_cache.percent < 0 && now < kBootGraceMs;
    const uint32_t interval = in_boot_grace                  ? kBootRetryMs
                              : g_battery_cache.percent >= 0 ? kRefreshMs
                                                             : kFailRetryMs;
    const bool due = !attempted || (now - last_attempt_ms) >= interval;
    if (due && keyboard().bus_idle()) {
        attempted = true;
        last_attempt_ms = now;
        const BatteryInfo fresh = read_battery_info();
        if (fresh.percent >= 0) {
            g_battery_cache = fresh;
            consecutive_failures = 0;
        } else if (!in_boot_grace) {
            ++consecutive_failures;
        }
    }
    return g_battery_cache;
}

uint64_t uptime_us() {
    return time_us_64();
}

uint32_t uptime_ms() {
    return static_cast<uint32_t>(time_us_64() / 1000);
}

}  // namespace platform
