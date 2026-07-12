#include "apps/param_editor.hpp"

#include <cstdio>

#include "apps/graph_model.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {
// 12 field rows (6 pairs) between the title (y=24) and the softkey bar
// (y=300): 24 + 12*22 = 288.
constexpr int kRowH = 22;
constexpr int kFieldCount = 2 * graph::kParametricSlots;

graph::ParametricFunctions& funcs() {
    return graph::state().param;
}

// Field i edits pair i/2; even fields are X, odd are Y.
int pair_of(int i) {
    return i / 2;
}
bool is_x_field(int i) {
    return (i % 2) == 0;
}

char* field_buf(int i) {
    auto& p = funcs();
    return is_x_field(i) ? p.x_expr[pair_of(i)] : p.y_expr[pair_of(i)];
}

bool pair_complete(int pair) {
    const auto& p = funcs();
    return p.x_expr[pair][0] != 0 && p.y_expr[pair][0] != 0;
}
}  // namespace

ParamEditorScreen::ParamEditorScreen() : SlotEditorScreen(kFieldCount, kRowH) {}

const char* ParamEditorScreen::title() const {
    return "PARAMETRIC EDITOR";
}

void ParamEditorScreen::field_label(int i, char* buf, size_t buf_len) const {
    std::snprintf(buf, buf_len, "%c%dT=", is_x_field(i) ? 'X' : 'Y', pair_of(i) + 1);
}

platform::Color ParamEditorScreen::field_label_color(int i) const {
    return function_color(pair_of(i));
}

const char* ParamEditorScreen::field_text(int i) const {
    return field_buf(i);
}

void ParamEditorScreen::set_field_text(int i, const char* s) {
    char* dst = field_buf(i);
    std::snprintf(dst, config::kMaxExprLen, "%s", s);
    // Auto-enable the pair once both halves are present (§5.1).
    if (pair_complete(pair_of(i))) {
        funcs().enabled[pair_of(i)] = true;
    }
    save_graph_state();
    invalidate_row(2 * pair_of(i));  // Checkbox lives on the X row.
}

void ParamEditorScreen::toggle_field(int i) {
    funcs().enabled[pair_of(i)] = !funcs().enabled[pair_of(i)];
    save_graph_state();
    invalidate_row(2 * pair_of(i));
}

void ParamEditorScreen::clear_field(int i) {
    field_buf(i)[0] = 0;
    if (!pair_complete(pair_of(i))) {
        funcs().enabled[pair_of(i)] = false;
    }
    save_graph_state();
    invalidate_row(2 * pair_of(i));
}

bool ParamEditorScreen::field_checked(int i) const {
    return funcs().enabled[pair_of(i)];
}

bool ParamEditorScreen::field_has_checkbox(int i) const {
    return is_x_field(i);
}

int ParamEditorScreen::label_width_chars() const {
    return 5;  // "X1T= "
}

void ParamEditorScreen::after_commit(int i) {
    // Committing an X expression auto-focuses its empty partner (§5.1).
    if (is_x_field(i) && field_buf(i)[0] != 0 && field_buf(i + 1)[0] == 0) {
        invalidate_row(selected_);
        selected_ = i + 1;
        begin_edit();
        invalidate_row(selected_);
    }
}

ParamEditorScreen& param_editor_screen() {
    static ParamEditorScreen instance;
    return instance;
}

}  // namespace apps
