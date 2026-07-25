#include "platform/power.hpp"

#include <cstdint>
#include <cstring>

extern "C" {
#include "i2ckbd/i2ckbd.h"
}

#include "platform/display.hpp"
#include "platform/keyboard.hpp"
#include "platform/storage.hpp"
#include "platform/system.hpp"

namespace platform::power {

namespace {

constexpr const char* kPath = "/picocalc/settings.dat";
constexpr char kMagic[4] = {'P', 'C', 'S', '1'};

struct Image {
    char magic[4];
    uint8_t lcd_level;
    uint8_t kbd_level;
    uint16_t apd_minutes;
};

Settings g_settings;
bool g_asleep = false;
uint32_t g_last_activity_ms = 0;

// Paced STM32 write queue: bit 0 = LCD level pending, bit 1 = kbd
// level pending. One write per tick, >= 250 ms apart, bus idle only
// (the same caution battery_poll observes — a wedged STM32 needs a
// physical power cycle).
uint8_t g_pending = 0;
uint32_t g_last_write_ms = 0;
constexpr uint32_t kWriteSpacingMs = 250;

// The levels the queue should converge on (0/0 while asleep).
uint8_t target_lcd() {
    return g_asleep ? 0 : g_settings.lcd_level;
}
uint8_t target_kbd() {
    return g_asleep ? 0 : g_settings.kbd_level;
}

}  // namespace

Settings& settings() {
    return g_settings;
}

void request_apply() {
    g_pending = 0b11;
}

bool asleep() {
    return g_asleep;
}

bool note_key(bool pressed) {
    g_last_activity_ms = uptime_ms();
    if (g_asleep && pressed) {
        g_asleep = false;
        request_apply();  // Restore both backlights
        return true;      // Swallow the wake key
    }
    return false;
}

void tick() {
    const uint32_t now = uptime_ms();
    if (g_last_activity_ms == 0) {
        g_last_activity_ms = now;  // Boot counts as activity
    }
    if (!g_asleep && g_settings.apd_minutes > 0 &&
        now - g_last_activity_ms >= g_settings.apd_minutes * 60'000U) {
        g_asleep = true;
        request_apply();  // Dim both backlights
    }
    if (g_pending == 0) {
        return;
    }
    if (!keyboard().bus_idle() || now - g_last_write_ms < kWriteSpacingMs) {
        return;
    }
    if ((g_pending & 0b01) != 0) {
        display().set_backlight(target_lcd());
        g_pending &= 0b10;
    } else {
        set_kbd_backlight(target_kbd());
        g_pending = 0;
    }
    g_last_write_ms = now;
}

bool save(Storage& storage) {
    if (!storage.mounted()) {
        return false;
    }
    Image img = {};
    std::memcpy(img.magic, kMagic, sizeof(kMagic));
    img.lcd_level = g_settings.lcd_level;
    img.kbd_level = g_settings.kbd_level;
    img.apd_minutes = g_settings.apd_minutes;
    return storage.write_file(kPath, reinterpret_cast<const uint8_t*>(&img), sizeof(img));
}

bool load(Storage& storage) {
    if (!storage.mounted()) {
        return false;
    }
    Image img = {};
    const int n = storage.read_file(kPath, reinterpret_cast<uint8_t*>(&img), sizeof(img));
    if (n != static_cast<int>(sizeof(img)) || std::memcmp(img.magic, kMagic, sizeof(kMagic)) != 0) {
        return true;  // No/old file: keep the STM32's own boot defaults
    }
    g_settings.lcd_level = img.lcd_level;
    g_settings.kbd_level = img.kbd_level;
    g_settings.apd_minutes = img.apd_minutes;
    request_apply();
    return true;
}

}  // namespace platform::power
