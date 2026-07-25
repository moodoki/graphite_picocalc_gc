#pragma once

#include <cstdint>

namespace platform {

class Storage;

// Device power polish (4D.19-20, D38): soft-sleep auto-power-down —
// LCD (and keyboard) backlight to 0 after a period of no keys, any key
// wakes — plus persisted brightness levels. No deep sleep in v1 (the
// core-1 display service and tinyusb make true sleep a separate
// project); the STM32 keeps polling so wake is instant.
//
// All STM32 writes go through tick()'s paced, bus-idle-gated queue —
// never back-to-back with the keyboard poll state machine (the wedge
// needs a physical power cycle; same discipline as battery_poll).
namespace power {

struct Settings {
    uint8_t lcd_level = 255;   // LCD backlight while awake (STM32 reg 0x05)
    uint8_t kbd_level = 0;     // Keyboard backlight (STM32 reg 0x0A)
    uint16_t apd_minutes = 5;  // Auto-power-down timeout; 0 = never
};

Settings& settings();

// Ask tick() to push the current levels to the hardware (settings
// screen edits, post-load). Writes happen one per tick, paced.
void request_apply();

// Main-loop hook, once per iteration: runs the inactivity timer and
// the paced STM32 write queue.
void tick();

// Key-activity hook (call for every drained key event, presses and
// releases). Returns true when this event woke the device from soft
// sleep — the caller should swallow it (the wake key must not type).
bool note_key(bool pressed);

bool asleep();

// Persistence: /picocalc/settings.dat (magic PCS1). load() applies the
// levels only when a valid file exists (a fresh unit keeps the STM32's
// own boot defaults untouched). False = storage not ready, retry.
bool save(Storage& storage);
bool load(Storage& storage);

}  // namespace power

}  // namespace platform
