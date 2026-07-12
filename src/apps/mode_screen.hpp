#pragma once

#include "ui/screen.hpp"

namespace apps {

// Mode screen (task 5.3): angle mode, display format, fix digits, graph
// mode (minimal task 2.22 pull — Function/Parametric), and a
// reboot-to-bootloader action for flashing (task 5.8).
class ModeScreen : public ui::Screen {
public:
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    static constexpr int kNumRows = 5;
    int selected_ = 0;

    void adjust(int dir) const;
};

ModeScreen& mode_screen();

}  // namespace apps
