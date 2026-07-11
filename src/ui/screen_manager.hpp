#pragma once

#include "ui/screen.hpp"

namespace ui {

// Fixed-depth screen stack. Screens are statically allocated by the
// caller; the manager never owns them.
class ScreenManager {
public:
    void push(Screen* screen);
    void pop();
    // Pop everything above the root screen (global HOME key).
    void pop_to_root();
    void replace(Screen* screen);
    Screen* current() const;

    void handle_key(const platform::KeyEvent& ev) const;
    // Renders one full frame of the current screen via the framebuffer
    // pipeline (dispatches to core 1).
    void render_frame() const;

private:
    static constexpr int kMaxDepth = 8;
    Screen* stack_[kMaxDepth] = {};
    int depth_ = 0;
};

ScreenManager& screen_manager();

}  // namespace ui
