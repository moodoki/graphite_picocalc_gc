#include "ui/input_line.hpp"

#include <cstring>

namespace ui {

bool InputLine::insert(char c) {
    if (len_ + 1 >= kCapacity) {
        return false;
    }
    std::memmove(buf_ + cursor_ + 1, buf_ + cursor_, len_ - cursor_ + 1);
    buf_[cursor_] = c;
    ++len_;
    ++cursor_;
    return true;
}

bool InputLine::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kLeft:
            if (cursor_ > 0) {
                --cursor_;
                return true;
            }
            return false;
        case Key::kRight:
            if (cursor_ < len_) {
                ++cursor_;
                return true;
            }
            return false;
        case Key::kHome:
            cursor_ = 0;
            return true;
        case Key::kBackspace:
            if (cursor_ > 0) {
                std::memmove(buf_ + cursor_ - 1, buf_ + cursor_, len_ - cursor_ + 1);
                --len_;
                --cursor_;
                return true;
            }
            return false;
        case Key::kDel:
            if (cursor_ < len_) {
                std::memmove(buf_ + cursor_, buf_ + cursor_ + 1, len_ - cursor_);
                --len_;
                return true;
            }
            return false;
        default:
            if (ev.ch != 0) {
                return insert(ev.ch);
            }
            return false;
    }
}

void InputLine::clear() {
    buf_[0] = 0;
    len_ = 0;
    cursor_ = 0;
}

void InputLine::set_text(const char* s) {
    std::strncpy(buf_, s, kCapacity - 1);
    buf_[kCapacity - 1] = 0;
    len_ = std::strlen(buf_);
    cursor_ = len_;
}

void InputLine::render(gfx::Framebuffer& fb, int x, int y, int w, const gfx::Font& font,
                       bool focused) const {
    using namespace platform::colors;
    const int cw = font.width();
    const int visible = w / cw;

    // Horizontal scroll: keep the cursor in view.
    int first = 0;
    if (static_cast<int>(cursor_) >= visible) {
        first = static_cast<int>(cursor_) - visible + 1;
    }

    int cx = x;
    for (int i = first; i < static_cast<int>(len_) && i - first < visible; ++i) {
        font.draw_char(fb, cx, y, buf_[i], kWhite, kBlack);
        cx += cw;
    }

    if (focused) {
        const int cur_col = static_cast<int>(cursor_) - first;
        const int cur_x = x + cur_col * cw;
        fb.fill_rect(cur_x, y, 2, font.height(), kCursor);
    }
}

}  // namespace ui
