#pragma once

#include "apps/slot_editor.hpp"

namespace apps {

// Y= function editor (task 4.1): list of Y1..Y7, each a function string
// with an enable checkbox. Navigate with UP/DOWN; ENTER/F1 edits inline;
// F2 toggles enable; F3 clears; F4 jumps to the graph.
class YEditorScreen : public SlotEditorScreen {
public:
    YEditorScreen();

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

YEditorScreen& y_editor_screen();

}  // namespace apps
