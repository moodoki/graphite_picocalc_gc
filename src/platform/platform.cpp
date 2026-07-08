#include "platform/platform.hpp"

namespace platform {

InitStatus init() {
    InitStatus status;

    keyboard().init();  // First: display backlight control needs the I2C bus
    status.keyboard = true;

    display().init();
    status.display = display().initialized();

    status.psram = psram().init();
    status.storage = storage().init();

    return status;
}

}  // namespace platform
