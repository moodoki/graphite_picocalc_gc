#pragma once

#include "ui/screen.hpp"

namespace apps {

// Scientific-constants picker (4D.17): typed `const` command. Lists
// the math::constants() catalog (symbol, value, summary); ENTER
// inserts the selected constant's engine identifier into the home
// screen's input line and pops back.
//
// Strip-safe (§8): pure selection state; render draws from the static
// catalog only.
class ConstScreen : public ui::Screen {
public:
    ConstScreen() { track_dirty(); }

    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    int selected_ = 0;
    int top_ = 0;          // First visible row
    int desc_scroll_ = 0;  // Left/right horizontal scroll of the selected
                           // row's summary (chars), so truncated
                           // descriptions can be read in full. Reset on
                           // selection change.
};

ConstScreen& const_screen();

}  // namespace apps
