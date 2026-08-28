// The platform:: seam's host half, minus display (display_headless.cpp)
// and storage (6.4.2). Phase 6.4.0.
//
// Every backend here is a stub, and each one is a stub for a stated
// reason rather than because it was easy: the host has no STM32, no
// battery, no watchdog and no second core. Spec section 3.5 is blunt that
// this is a development instrument and not a fidelity emulator, and these
// files are where that is literally true -- read them before trusting a
// host result about power, faults or stack depth.

#include <chrono>
#include <cmath>
#include <cstring>

#include "platform/fault.hpp"
#include "platform/keyboard.hpp"
#include "platform/platform.hpp"
#include "platform/power.hpp"
#include "platform/psram.hpp"
#include "platform/storage.hpp"
#include "platform/system.hpp"

namespace platform {

// ---- system ----

uint64_t uptime_us() {
    // Monotonic and process-relative, matching time_us_64()'s contract of
    // "microseconds since the thing started running".
    static const auto t0 = std::chrono::steady_clock::now();
    const auto dt = std::chrono::steady_clock::now() - t0;
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(dt).count());
}

uint32_t uptime_ms() {
    return static_cast<uint32_t>(uptime_us() / 1000u);
}

BatteryInfo read_battery_info() {
    // -1 is the interface's own "unavailable", so the status bar renders
    // the no-battery case rather than an invented percentage. A desktop
    // reporting a plausible-looking charge would be a small lie in the
    // one place a screenshot is meant to be evidence.
    return BatteryInfo{};
}

BatteryInfo battery_status() {
    return read_battery_info();
}

BatteryInfo battery_poll() {
    return read_battery_info();
}

float die_temp_c() {
    return std::nanf("");
}

// ---- fault ----

bool take_prior_fault(FaultInfo* /*out*/) {
    return false;  // No watchdog, no persisted crash record.
}

void clear_fault_streak() {}

void paint_stack() {}

uint32_t stack_peak_used() {
    return 0;
}

uint32_t stack_total() {
    // 0, not 4096. The host stack is megabytes and nothing here measures
    // it, so reporting the Pico's number would make a host screenshot of
    // the diagnostics screen say something false about SRAM -- the exact
    // false-confidence failure section 5.2 names.
    return 0;
}

// ---- power ----

namespace power {

Settings& settings() {
    static Settings s;
    return s;
}

void request_apply() {}

void tick() {}

bool note_key(bool /*pressed*/) {
    return false;  // Never asleep, so no key is ever swallowed as a wake.
}

bool asleep() {
    return false;
}

bool save(Storage& /*storage*/) {
    return false;
}

bool load(Storage& /*storage*/) {
    return false;
}

}  // namespace power

// ---- psram ----
//
// math::psram_backend is the seam the array/list stack actually uses, and
// psram_arena.cpp implements that over malloc. This class is the lower
// layer, referenced only by the firmware's boot self-test, so on host it
// reports absent rather than pretending to be an 8 MB device.

bool Psram::init() {
    ok_ = false;
    return false;
}

bool Psram::reinit() {
    return false;
}

uint32_t Psram::alloc(size_t /*bytes*/, size_t /*alignment*/) {
    return kInvalid;
}

void Psram::reset() {
    next_ = 0;
}

void Psram::write_word(uint32_t /*addr*/, uint32_t /*value*/) {}

uint32_t Psram::read_word(uint32_t /*addr*/) {
    return 0;
}

void Psram::write(uint32_t /*addr*/, const uint8_t* /*data*/, size_t /*len*/) {}

void Psram::read(uint32_t /*addr*/, uint8_t* data, size_t len) {
    if (data != nullptr && len > 0) {
        std::memset(data, 0, len);
    }
}

Psram& psram() {
    static Psram instance;
    return instance;
}

// ---- init ----

InitStatus init() {
    // Same order as platform.cpp, so the host's bring-up reads against
    // the firmware's. Keyboard first is a hardware constraint there (the
    // backlight needs the I2C bus up) and free here; keeping it costs
    // nothing and keeps the two sequences comparable.
    InitStatus status;
    keyboard().init();
    status.keyboard = true;
    display().init();
    status.display = true;
    status.psram = psram().init();
    status.storage = storage().init();
    return status;
}

}  // namespace platform
