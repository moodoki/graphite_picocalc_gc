#pragma once

#include <cstddef>

#include "platform/keyboard.hpp"
#include "gfx/font.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/input_line.hpp"

namespace ui {

// A modal one-line prompt: label, an InputLine, and an error slot
// (Phase 6A.5).
//
// Before this, the "ask the user for a name" pattern was reimplemented
// per screen — ListEditorScreen's name_prompt_/prompt_/msg_ and
// MatrixEditorScreen's dim_prompt_ are the same idea written twice.
// The editor's F4:NEW plus the browser's rename and new-folder keys
// make three more call sites, which is what justifies factoring it out
// rather than writing a fourth.
//
// The owning screen drives it: check active(), give it the key first,
// and act on the submitted/cancelled flags.
class PromptLine {
public:
    static constexpr std::size_t kMaxLabel = 24;
    static constexpr std::size_t kMaxError = 40;

    // `initial` pre-fills the input (rename), or null for an empty one.
    void open(const char* label, const char* initial = nullptr);
    void close();
    bool active() const { return active_; }

    // Returns true if the event was consumed. On ENTER *submitted is
    // set and text() holds the answer — the prompt stays open so the
    // caller can reject it with set_error() and let the user correct
    // it; call close() once satisfied. On ESC *cancelled is set and the
    // prompt has already closed itself.
    bool on_key(const platform::KeyEvent& ev, bool* submitted, bool* cancelled);

    const char* text() const { return input_.text(); }
    bool empty() const { return input_.empty(); }

    void set_error(const char* msg);
    const char* error() const { return error_; }

    // One line, drawn at [x, x+w). Label in green, then the input, then
    // any error right-aligned in red — matching the list editor's
    // existing prompt look.
    void render(gfx::Framebuffer& fb, int x, int y, int w, const gfx::Font& font) const;

private:
    bool active_ = false;
    char label_[kMaxLabel] = {};
    char error_[kMaxError] = {};
    InputLine input_;
};

}  // namespace ui
