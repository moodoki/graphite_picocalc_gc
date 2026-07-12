#pragma once

#include "apps/slot_editor.hpp"

namespace apps {

// Polar editor (task 2.9, spec §6.1): six r(theta) expressions with
// enable checkboxes. The independent variable is typed as "theta".
//
// Slots persist with the unified GraphState migration (task 2.23).
class PolarEditorScreen : public SlotEditorScreen {
public:
    PolarEditorScreen();

protected:
    const char* title() const override;
    void field_label(int i, char* buf, size_t buf_len) const override;
    platform::Color field_label_color(int i) const override;
    const char* field_text(int i) const override;
    void set_field_text(int i, const char* s) override;
    void toggle_field(int i) override;
    void clear_field(int i) override;
    bool field_checked(int i) const override;
};

PolarEditorScreen& polar_editor_screen();

}  // namespace apps
