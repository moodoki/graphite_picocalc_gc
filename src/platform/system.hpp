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

// On-chip die temperature in degrees Celsius, from the RP2040/RP2350
// internal temperature sensor (ADC input 4 — no external pin). This is
// junction temperature, so it reads above ambient (self-heating, more so
// with the Pico 1 overclock + core-1 display service). Lazily initialises
// the ADC on first call. Returns NaN if the reading is unavailable.
float die_temp_c();

}  // namespace platform
