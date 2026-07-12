#pragma once

#include "ui/screen.hpp"

namespace apps {

// Built-in help browser (tasks 2.27/2.28, spec §10): three tabs —
// function catalog (driven by math::catalog, the same table the parser
// registers from), per-screen key reference, and syntax notes. All
// content is compiled into flash; no SD dependency.
//
// Keys: LEFT/RIGHT switch tabs, UP/DOWN scroll, ESC exits.
// Entry point: Home F5.
class HelpScreen : public ui::Screen {
public:
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    int tab_ = 0;
    int scroll_ = 0;

    int line_count() const;
    int max_scroll() const;
};

HelpScreen& help_screen();

}  // namespace apps
