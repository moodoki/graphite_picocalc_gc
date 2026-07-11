#include "platform/system.hpp"

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
// HW note (2026-07-11, Pico 1 unit): the STM32 keyboard firmware on
// this unit does not implement the battery register — reg 0x01 answers
// but reg 0x0B times out on select or read. battery_status()'s failure
// cap handles it; the indicator shows "--". Revisit after a keyboard
// firmware update.
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
            // High byte: percentage; bit 7 of low byte: charging.
            info.percent = (raw >> 8) & 0x7F;
            info.charging = (raw & 0x80) != 0;
            return info;
        }
        sleep_ms(kRetryGapMs);
    }
    return info;
}

BatteryInfo battery_status() {
    static BatteryInfo cache;
    static uint32_t last_attempt_ms = 0;
    static bool attempted = false;
    static int consecutive_failures = 0;

    // Refresh every 30 s; after a failure, retry no sooner than 10 s.
    // The backoff applies to *attempts*, not successes — battery_status()
    // is called once per strip during rendering (~20x/frame), and
    // retrying per call hammers the STM32 into NACKing (seen on HW).
    // After several straight failures, give up until reboot: units with
    // pre-battery-register STM32 firmware time out every attempt, and
    // each failed attempt blocks the render for hundreds of ms.
    constexpr uint32_t kRefreshMs = 30'000;
    constexpr uint32_t kFailRetryMs = 10'000;
    constexpr int kMaxConsecutiveFailures = 5;

    if (consecutive_failures >= kMaxConsecutiveFailures) {
        return cache;
    }
    const uint32_t interval = cache.percent >= 0 ? kRefreshMs : kFailRetryMs;
    const uint32_t now = uptime_ms();
    const bool due = !attempted || (now - last_attempt_ms) >= interval;
    if (due && keyboard().bus_idle()) {
        attempted = true;
        last_attempt_ms = now;
        const BatteryInfo fresh = read_battery_info();
        if (fresh.percent >= 0) {
            cache = fresh;
            consecutive_failures = 0;
        } else {
            ++consecutive_failures;
        }
    }
    return cache;
}

uint64_t uptime_us() {
    return time_us_64();
}

uint32_t uptime_ms() {
    return static_cast<uint32_t>(time_us_64() / 1000);
}

}  // namespace platform
