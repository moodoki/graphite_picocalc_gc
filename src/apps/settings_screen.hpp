#pragma once

#include "ui/screen.hpp"

namespace apps {

// Device settings (4D.19-20): LCD brightness, keyboard backlight,
// auto-power-down timeout. Typed `settings` command. LEFT/RIGHT
// adjusts; every change applies (paced, via platform::power) and
// persists to /picocalc/settings.dat (PCS1).
//
// Strip-safe (§8): pure selection state; render reads settings only.
class SettingsScreen : public ui::Screen {
public:
    SettingsScreen() { track_dirty(); }

    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    int selected_ = 0;

    void adjust(int dir);
};

SettingsScreen& settings_screen();

}  // namespace apps
