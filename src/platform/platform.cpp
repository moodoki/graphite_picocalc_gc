#include "platform/platform.hpp"

#include "platform/boot_trace.hpp"

namespace platform {

InitStatus init() {
    InitStatus status;

    boot_stage(BootStage::kKeyboard);
    keyboard().init();  // First: display backlight control needs the I2C bus
    status.keyboard = true;

    boot_stage(BootStage::kDisplay);
    display().init();
    status.display = display().initialized();
    // Backlight on here rather than after the self-tests, where it used to
    // live (2026-08-30). Everything below this line can block, and until the
    // panel is lit a board wedged in bring-up is indistinguishable from one
    // that never started -- which is exactly the distinction two "it won't
    // boot" incidents could not be made on. Lit-but-blank is a diagnosis;
    // dark is not. power::load() still applies the persisted level later.
    display().set_backlight(200);

    boot_stage(BootStage::kPsram);
    // A board that has wedged here on consecutive boots comes up without
    // PSRAM rather than rebooting into it again -- it is optional, and the
    // status bar shows it red (D26). PSRAM then stays down for the rest of
    // this boot: Psram::reinit() refuses an instance init() never
    // configured, so the late-init loop cannot revive it. The next power
    // cycle starts the streak over and tries again.
    status.psram = skip_psram_this_boot() ? false : psram().init();

    boot_stage(BootStage::kStorage);
    status.storage = storage().init();

    return status;
}

}  // namespace platform
