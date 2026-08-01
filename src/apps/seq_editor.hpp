#pragma once

#include "apps/slot_editor.hpp"

namespace apps {

// Sequence editor (4D.8, spec §7.2): TI's seq-mode Y= screen. Seven
// fields — nMin, then an expression row + seed row per sequence:
//
//   nMin=1
//   u(n)=u(n-1)+1     [x]
//   u(nMin)=1
//   v(n)=             [ ]
//   v(nMin)=0
//   w(n)=             [ ]
//   w(nMin)=0
//
// The seed field accepts a plain number or "{a,b}" — b is the value at
// nMin+1, consumed only by sequences referencing an (n-2) lag.
// Reachable via the mode selector; persists in GraphState (PCG6).
class SeqEditorScreen : public SlotEditorScreen {
public:
    SeqEditorScreen();

protected:
    const char* title() const override;
    void field_label(int i, char* buf, size_t buf_len) const override;
    platform::Color field_label_color(int i) const override;
    const char* field_text(int i) const override;
    void set_field_text(int i, const char* s) override;
    void toggle_field(int i) override;
    void clear_field(int i) override;
    bool field_checked(int i) const override;
    bool field_valid(int i, const char* text) const override;
    bool field_has_checkbox(int i) const override;
    int label_width_chars() const override;
};

SeqEditorScreen& seq_editor_screen();

}  // namespace apps
