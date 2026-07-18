#pragma once

#include <cstdint>

namespace platform {

// Battery state from the STM32 south bridge.
struct BatteryInfo {
    int percent = -1;  // -1 if unavailable
    bool charging = false;
};

// Raw read. Blocks ~21 ms on the 10 kHz keyboard I2C bus — do not call
// per frame; use battery_status() / battery_poll() instead.
BatteryInfo read_battery_info();

// Cached battery state for UI. Never touches I2C — safe to call from
// render code every strip/frame. The cache is only as fresh as the
// last battery_poll().
BatteryInfo battery_status();

// Refresh the cache from the STM32 when due (every ~5 s on success,
// backed off on failure), and only while the keyboard poll state
// machine is idle (the bus is shared — see Keyboard::bus_idle).
// Call from the main loop only; a successful refresh blocks ~21 ms.
BatteryInfo battery_poll();

uint64_t uptime_us();
uint32_t uptime_ms();

}  // namespace platform
