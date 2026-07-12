#include "apps/y_editor.hpp"

#include <cstdio>

#include "apps/graph_model.hpp"

namespace apps {

namespace {
constexpr int kRowH = 26;
}  // namespace

YEditorScreen::YEditorScreen() : SlotEditorScreen(kNumFuncs, kRowH) {}

const char* YEditorScreen::title() const {
    return "Y= FUNCTION EDITOR";
}

void YEditorScreen::field_label(int i, char* buf, size_t buf_len) const {
    std::snprintf(buf, buf_len, "Y%d=", i + 1);
}

platform::Color YEditorScreen::field_label_color(int i) const {
    return function_color(i);
}

const char* YEditorScreen::field_text(int i) const {
    return y_functions().expr[i];
}

void YEditorScreen::set_field_text(int i, const char* s) {
    auto& fns = y_functions();
    std::snprintf(fns.expr[i], sizeof(fns.expr[i]), "%s", s);
    // Auto-enable a slot when a non-empty expression is entered.
    if (fns.expr[i][0] != 0) {
        fns.enabled[i] = true;
    }
    save_functions();
}

void YEditorScreen::toggle_field(int i) {
    y_functions().enabled[i] = !y_functions().enabled[i];
    save_functions();
}

void YEditorScreen::clear_field(int i) {
    y_functions().expr[i][0] = 0;
    y_functions().enabled[i] = false;
    save_functions();
}

bool YEditorScreen::field_checked(int i) const {
    return y_functions().enabled[i];
}

YEditorScreen& y_editor_screen() {
    static YEditorScreen instance;
    return instance;
}

}  // namespace apps
