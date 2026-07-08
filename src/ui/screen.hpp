#pragma once

#include "platform/keyboard.hpp"
#include "gfx/framebuffer.hpp"

namespace ui {

class Screen {
public:
    virtual ~Screen() = default;

    // Called when this screen becomes top of the stack.
    virtual void on_activate() {}
    // Called when popped or covered.
    virtual void on_deactivate() {}

    // Handle a key event. Return true if consumed.
    virtual bool on_key(const platform::KeyEvent& ev) = 0;

    // Render the full frame. In strip mode this is called once per strip;
    // use fb.clip_y0()/clip_y1() to skip out-of-strip work if profiling
    // demands it, otherwise just draw everything.
    virtual void render(gfx::Framebuffer& fb) = 0;
};

}  // namespace ui
