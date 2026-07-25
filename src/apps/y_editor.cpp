#include "apps/y_editor.hpp"

#include <cstdint>
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

bool YEditorScreen::field_key(int i, const platform::KeyEvent& ev) {
    // 'S' cycles inequality shading: none -> above -> below (4D.11).
    if (ev.ch == 's' || ev.ch == 'S') {
        auto& mode = graph::state().shade_mode[i];
        mode = static_cast<uint8_t>((mode + 1) % 3);
        save_graph_state();
        return true;
    }
    return false;
}

char YEditorScreen::field_marker(int i) const {
    const uint8_t mode = graph::state().shade_mode[i];
    return mode == 1 ? '^' : (mode == 2 ? 'v' : 0);
}

const char* YEditorScreen::softkey_text() const {
    return "ENTER:EDIT SPC:SEL DEL:CLR S:SHD F5:GRPH";
}

YEditorScreen& y_editor_screen() {
    static YEditorScreen instance;
    return instance;
}

}  // namespace apps
