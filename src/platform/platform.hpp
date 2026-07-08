#pragma once

#include "platform/display.hpp"
#include "platform/keyboard.hpp"
#include "platform/psram.hpp"
#include "platform/storage.hpp"
#include "platform/system.hpp"

namespace platform {

// Results of hardware bring-up, for diagnostics display.
struct InitStatus {
    bool display = false;
    bool keyboard = false;
    bool psram = false;
    bool storage = false;
};

// Initialize all peripherals (call once from core 0 before launching
// core 1). Display and keyboard are required; PSRAM and SD are optional
// and reported in the returned status.
InitStatus init();

}  // namespace platform
