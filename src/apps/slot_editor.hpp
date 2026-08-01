#pragma once

#include <cstddef>

#include "platform/display.hpp"
#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// Shared machinery for the per-mode function editors (Y=, parametric,
// polar): a list of expression fields with enable checkboxes, inline
// InputLine editing, and dirty-band row invalidation. Subclasses define
// what a field is (task 2.5 extraction; the Y= editor's behavior is the
// reference).
//
// Keys: UP/DOWN select; ENTER/F1 edit (ENTER commits, ESC cancels);
// F2 toggles enable; F3 clears; F4 pushes the graph screen; ESC pops.
class SlotEditorScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

protected:
    SlotEditorScreen(int field_count, int row_h) : field_count_(field_count), row_h_(row_h) {
        track_dirty();
    }

    // ---- Per-mode hooks ----
    virtual const char* title() const = 0;
    // Write field i's label (e.g. "Y1=") into buf.
    virtual void field_label(int i, char* buf, size_t buf_len) const = 0;
    virtual platform::Color field_label_color(int i) const = 0;
    virtual const char* field_text(int i) const = 0;
    // Commit edited text (handles auto-enable + persistence).
    virtual void set_field_text(int i, const char* s) = 0;
    virtual void toggle_field(int i) = 0;  // F2 (handles persistence)
    virtual void clear_field(int i) = 0;   // F3 (handles persistence)
    virtual bool field_checked(int i) const = 0;
    virtual bool field_has_checkbox(int /*i*/) const { return true; }
    // Whether field i's text is a valid expression (drawn white; invalid
    // → red). Default: compiles under the plain engine. The sequence
    // editor overrides this — recursive refs like u(n-1) are valid
    // sequence definitions the plain engine rejects.
    virtual bool field_valid(int i, const char* text) const;
    // Label column width in characters (expr text starts after it).
    virtual int label_width_chars() const { return 4; }
    // Called after a commit; may move selection / re-enter editing
    // (parametric auto-focuses the paired field here).
    virtual void after_commit(int /*i*/) {}
    // Unhandled non-editing keys reach the subclass (the Y= editor's
    // shade-style cycle, 4D.11). Return true when consumed.
    virtual bool field_key(int /*i*/, const platform::KeyEvent& /*ev*/) { return false; }
    // Optional one-char marker drawn left of the checkbox (shade style).
    virtual char field_marker(int /*i*/) const { return 0; }
    // Softkey bar text (Y= appends its shade key).
    virtual const char* softkey_text() const { return "ENTER:EDIT SPC:SEL DEL:CLR F5:GRPH"; }

    void invalidate_row(int i);
    void begin_edit();

    int selected_ = 0;
    bool editing_ = false;
    ui::InputLine input_;

private:
    void commit_edit();

    int field_count_;
    int row_h_;
};

}  // namespace apps
