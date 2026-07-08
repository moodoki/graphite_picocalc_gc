#include "platform/system.hpp"

#include "pico/time.h"

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

uint64_t uptime_us() {
    return time_us_64();
}

uint32_t uptime_ms() {
    return static_cast<uint32_t>(time_us_64() / 1000);
}

}  // namespace platform
