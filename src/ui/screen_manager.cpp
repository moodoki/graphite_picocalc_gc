#include "ui/screen_manager.hpp"

namespace ui {

namespace {
void render_trampoline(gfx::Framebuffer& fb, void* ctx) {
    static_cast<Screen*>(ctx)->render(fb);
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
    screen->on_activate();
}

void ScreenManager::pop() {
    if (depth_ <= 1) {
        return;  // Never pop the last screen
    }
    stack_[--depth_]->on_deactivate();
    stack_[depth_ - 1]->on_activate();
}

void ScreenManager::replace(Screen* screen) {
    if (screen == nullptr || depth_ == 0) {
        push(screen);
        return;
    }
    stack_[depth_ - 1]->on_deactivate();
    stack_[depth_ - 1] = screen;
    screen->on_activate();
}

Screen* ScreenManager::current() const {
    return depth_ > 0 ? stack_[depth_ - 1] : nullptr;
}

void ScreenManager::handle_key(const platform::KeyEvent& ev) {
    if (Screen* s = current()) {
        s->on_key(ev);
    }
}

void ScreenManager::render_frame() {
    if (Screen* s = current()) {
        gfx::framebuffer().render_frame(render_trampoline, s);
    }
}

ScreenManager& screen_manager() {
    static ScreenManager instance;
    return instance;
}

}  // namespace ui
