#pragma once

#include "apps/slot_editor.hpp"

namespace apps {

// Parametric editor (task 2.5, spec §5.1): six (XnT, YnT) expression
// pairs, each pair sharing one enable checkbox (drawn on the X row).
// A pair plots only when enabled AND both expressions are non-empty;
// committing an X expression auto-focuses its empty partner.
//
// Reachable via the mode selector (task 2.22); slots persist with the
// unified GraphState migration (task 2.23).
class ParamEditorScreen : public SlotEditorScreen {
public:
    ParamEditorScreen();

protected:
    const char* title() const override;
    void field_label(int i, char* buf, size_t buf_len) const override;
    platform::Color field_label_color(int i) const override;
    const char* field_text(int i) const override;
    void set_field_text(int i, const char* s) override;
    void toggle_field(int i) override;
    void clear_field(int i) override;
    bool field_checked(int i) const override;
    bool field_has_checkbox(int i) const override;
    int label_width_chars() const override;
    void after_commit(int i) override;
};

ParamEditorScreen& param_editor_screen();

}  // namespace apps
