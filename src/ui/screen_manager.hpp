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
    // Navigate to `screen` without growing the stack: pops when it is
    // directly beneath (the toggle-back case), replaces the top
    // otherwise. Cross-jumps between sibling views must use this, not
    // push() — repeated toggling would leak stack slots until every
    // push silently no-ops at kMaxDepth (HW 2026-07-18).
    void switch_to(Screen* screen);
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
