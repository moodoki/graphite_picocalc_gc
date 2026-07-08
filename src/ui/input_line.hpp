#pragma once

#include <cstddef>

#include "platform/keyboard.hpp"
#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"

namespace ui {

// Single-line text editor with cursor (task 2.8): insert, backspace,
// delete, left/right/home, horizontal scroll for long content.
class InputLine {
public:
    static constexpr size_t kCapacity = 128;

    // Returns true if the event modified state (needs redraw).
    bool on_key(const platform::KeyEvent& ev);

    void clear();
    void set_text(const char* s);
    const char* text() const { return buf_; }
    size_t length() const { return len_; }
    bool empty() const { return len_ == 0; }

    // Draw within [x, x+w); y is the text baseline top. Shows a block
    // cursor at the insertion point when focused.
    void render(gfx::Framebuffer& fb, int x, int y, int w, const gfx::Font& font,
                bool focused) const;

private:
    char buf_[kCapacity] = {};
    size_t len_ = 0;
    size_t cursor_ = 0;

    bool insert(char c);
};

}  // namespace ui
