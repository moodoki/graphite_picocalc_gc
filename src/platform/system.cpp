#include "platform/system.hpp"

#include "pico/time.h"

#include "platform/keyboard.hpp"

extern "C" {
#include "i2ckbd/i2ckbd.h"
}

namespace platform {

BatteryInfo read_battery_info() {
    BatteryInfo info;
    const int raw = read_battery();  // Vendored; blocks ~16 ms
    if (raw >= 0) {
        // High byte: percentage; bit 7 of low byte: charging.
        info.percent = (raw >> 8) & 0x7F;
        info.charging = (raw & 0x80) != 0;
    }
    return info;
}

BatteryInfo battery_status() {
    static BatteryInfo cache;
    static uint32_t last_attempt_ms = 0;
    static bool ever_read = false;

    constexpr uint32_t kRefreshMs = 30'000;
    const uint32_t now = uptime_ms();
    const bool due = !ever_read || (now - last_attempt_ms) >= kRefreshMs;
    if (due && keyboard().bus_idle()) {
        // Blocks ~16 ms; acceptable at this cadence (render is
        // event-driven, so this rides on a keypress redraw).
        const BatteryInfo fresh = read_battery_info();
        last_attempt_ms = now;  // Back off after failures too
        if (fresh.percent >= 0) {
            cache = fresh;
            ever_read = true;
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
