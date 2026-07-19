#include "ui/screen_manager.hpp"

namespace ui {

namespace {
void render_trampoline(gfx::Framebuffer& fb, void* ctx) {
    static_cast<Screen*>(ctx)->render(fb);
}

// A screen surfacing to top of stack needs a full redraw regardless of
// its own dirty tracking.
void activate(Screen* screen) {
    screen->invalidate_all();
    screen->on_activate();
}
}  // namespace

void ScreenManager::push(Screen* screen) {
    if (depth_ >= kMaxDepth || screen == nullptr) {
        return;
    }
    if (depth_ > 0) {
        stack_[depth_ - 1]->on_deactivate();
    }
    stack_[depth_++] = screen;
    activate(screen);
}

void ScreenManager::pop() {
    if (depth_ <= 1) {
        return;  // Never pop the last screen
    }
    stack_[--depth_]->on_deactivate();
    activate(stack_[depth_ - 1]);
}

void ScreenManager::pop_to_root() {
    if (depth_ <= 1) {
        return;
    }
    while (depth_ > 1) {
        stack_[--depth_]->on_deactivate();
    }
    activate(stack_[0]);
}

void ScreenManager::switch_to(Screen* screen) {
    if (screen == nullptr || depth_ <= 0) {
        push(screen);
        return;
    }
    if (current() == screen) {
        return;
    }
    if (depth_ >= 2 && stack_[depth_ - 2] == screen) {
        pop();
        return;
    }
    if (depth_ == 1) {
        // Never displace the root: switch_to() from the home screen
        // (e.g. F4 trace) used to replace() it, leaving HOME/ESC with
        // no home screen to return to (HW 2026-07-19).
        push(screen);
        return;
    }
    replace(screen);
}

void ScreenManager::replace(Screen* screen) {
    if (screen == nullptr || depth_ == 0) {
        push(screen);
        return;
    }
    stack_[depth_ - 1]->on_deactivate();
    stack_[depth_ - 1] = screen;
    activate(screen);
}

Screen* ScreenManager::current() const {
    return depth_ > 0 ? stack_[depth_ - 1] : nullptr;
}

void ScreenManager::handle_key(const platform::KeyEvent& ev) const {
    if (Screen* s = current()) {
        s->on_key(ev);
    }
}

void ScreenManager::render_frame() const {
    Screen* s = current();
    if (s == nullptr) {
        return;
    }
    int y0 = 0;
    int y1 = 0;
    s->take_dirty(y0, y1);
    if (y0 >= y1) {
        return;  // Nothing changed — skip the render and push entirely
    }
    gfx::framebuffer().render_frame(render_trampoline, s, y0, y1);
}

ScreenManager& screen_manager() {
    static ScreenManager instance;
    return instance;
}

}  // namespace ui
