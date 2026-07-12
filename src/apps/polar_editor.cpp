#include "apps/polar_editor.hpp"

#include <cstdio>

#include "apps/graph_model.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {
constexpr int kRowH = 26;

graph::PolarFunctions& funcs() {
    return graph::state().polar;
}
}  // namespace

PolarEditorScreen::PolarEditorScreen() : SlotEditorScreen(graph::kPolarSlots, kRowH) {}

const char* PolarEditorScreen::title() const {
    return "POLAR EDITOR  r(theta)";
}

void PolarEditorScreen::field_label(int i, char* buf, size_t buf_len) const {
    std::snprintf(buf, buf_len, "r%d=", i + 1);
}

platform::Color PolarEditorScreen::field_label_color(int i) const {
    return function_color(i);
}

const char* PolarEditorScreen::field_text(int i) const {
    return funcs().expr[i];
}

void PolarEditorScreen::set_field_text(int i, const char* s) {
    std::snprintf(funcs().expr[i], config::kMaxExprLen, "%s", s);
    // Auto-enable a slot when a non-empty expression is entered.
    if (funcs().expr[i][0] != 0) {
        funcs().enabled[i] = true;
    }
    // Persisted with the GraphState migration (task 2.23).
}

void PolarEditorScreen::toggle_field(int i) {
    funcs().enabled[i] = !funcs().enabled[i];
}

void PolarEditorScreen::clear_field(int i) {
    funcs().expr[i][0] = 0;
    funcs().enabled[i] = false;
}

bool PolarEditorScreen::field_checked(int i) const {
    return funcs().enabled[i];
}

PolarEditorScreen& polar_editor_screen() {
    static PolarEditorScreen instance;
    return instance;
}

}  // namespace apps
