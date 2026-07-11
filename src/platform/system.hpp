#pragma once

#include <cstdint>

namespace platform {

// Battery state from the STM32 south bridge.
struct BatteryInfo {
    int percent = -1;  // -1 if unavailable
    bool charging = false;
};

// Raw read. Blocks ~16 ms on the 10 kHz keyboard I2C bus — do not call
// per frame; use battery_status() instead.
BatteryInfo read_battery_info();

// Cached battery state for UI: refreshes from the STM32 at most every
// 30 s, and only while the keyboard poll state machine is idle (the
// bus is shared — see Keyboard::bus_idle). Cheap to call every frame.
BatteryInfo battery_status();

uint64_t uptime_us();
uint32_t uptime_ms();

}  // namespace platform
