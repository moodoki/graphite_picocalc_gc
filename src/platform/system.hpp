#pragma once

#include <cstdint>

namespace platform {

// Battery state from the STM32 south bridge.
struct BatteryInfo {
    int percent = -1;  // -1 if unavailable
    bool charging = false;
};

BatteryInfo read_battery_info();

uint64_t uptime_us();
uint32_t uptime_ms();

}  // namespace platform
