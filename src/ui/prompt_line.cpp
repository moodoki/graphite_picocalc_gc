#include "ui/prompt_line.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

void PromptLine::open(const char* label, const char* initial) {
    active_ = true;
    std::snprintf(label_, sizeof(label_), "%s", label != nullptr ? label : "");
    error_[0] = 0;
    input_.clear();
    if (initial != nullptr) {
        input_.set_text(initial);
    }
}

void PromptLine::close() {
    active_ = false;
    error_[0] = 0;
    input_.clear();
}

void PromptLine::set_error(const char* msg) {
    std::snprintf(error_, sizeof(error_), "%s", msg != nullptr ? msg : "");
}

bool PromptLine::on_key(const platform::KeyEvent& ev, bool* submitted, bool* cancelled) {
    if (submitted != nullptr) {
        *submitted = false;
    }
    if (cancelled != nullptr) {
        *cancelled = false;
    }
    if (!active_ || !ev.pressed) {
        return false;
    }

    if (ev.key == platform::Key::kEnter) {
        // Stay open: the caller may reject the value with set_error()
        // and want the user to fix it in place.
        if (submitted != nullptr) {
            *submitted = true;
        }
        return true;
    }
    if (ev.key == platform::Key::kEscape) {
        close();
        if (cancelled != nullptr) {
            *cancelled = true;
        }
        return true;
    }

    // Any edit clears a stale error — it no longer describes what is
    // in the box.
    if (input_.on_key(ev)) {
        error_[0] = 0;
        return true;
    }
    // Swallow everything while modal, so arrows and softkeys can't
    // drive the screen underneath.
    return true;
}

void PromptLine::render(gfx::Framebuffer& fb, int x, int y, int w, const gfx::Font& font) const {
    using namespace platform::colors;
    if (!active_) {
        return;
    }

    const int label_w = font.text_width(label_);
    font.draw_string(fb, x, y, label_, kGreen);

    int input_w = w - label_w - 4;
    if (error_[0] != 0) {
        const int err_w = font.text_width(error_);
        font.draw_string(fb, x + w - err_w, y, error_, kRed);
        input_w -= err_w + 8;
    }
    if (input_w > 0) {
        input_.render(fb, x + label_w + 4, y, input_w, font, true);
    }
}

}  // namespace ui
