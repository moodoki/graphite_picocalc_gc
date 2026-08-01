#include "apps/seq_editor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/seq_expr.hpp"
#include "apps/graph_model.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {
// 7 field rows between the title (y=24) and the softkey bar (y=300).
constexpr int kRowH = 22;
constexpr int kFieldCount = 1 + 2 * graph::kSeqSlots;  // nMin + (expr, seed) x3

constexpr int kFieldNMin = 0;

graph::SeqFunctions& funcs() {
    return graph::state().seq;
}

// Field i >= 1 edits sequence (i-1)/2; odd fields are expressions,
// even fields (2, 4, 6) are seeds.
int seq_of(int i) {
    return (i - 1) / 2;
}
bool is_expr_field(int i) {
    return i >= 1 && (i % 2) == 1;
}

char seq_name(int s) {
    return static_cast<char>('u' + s);
}

// Seed text: "1" or "{1,2}" when the second seed is set. Rendered into
// a shared scratch (render draws one row at a time).
char g_scratch[48];

const char* seed_text(int s) {
    char a[24];
    math::format_number(funcs().seed1[s], a, sizeof(a));
    if (funcs().seed2[s] != 0) {
        char b[24];
        math::format_number(funcs().seed2[s], b, sizeof(b));
        std::snprintf(g_scratch, sizeof(g_scratch), "{%s,%s}", a, b);
    } else {
        std::snprintf(g_scratch, sizeof(g_scratch), "%s", a);
    }
    return g_scratch;
}

// Parse a seed entry: "a" or "{a,b}" (full expression syntax per part).
bool parse_seed(const char* s, double* first, double* second) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s", s);
    char* p = buf;
    while (*p == ' ') {
        ++p;
    }
    *second = 0;
    if (*p == '{') {
        ++p;
        char* close = std::strrchr(p, '}');
        if (close == nullptr) {
            return false;
        }
        *close = 0;
        char* comma = std::strchr(p, ',');
        if (comma != nullptr) {
            *comma = 0;
            if (!math::eval_field(comma + 1, second)) {
                return false;
            }
        }
        return math::eval_field(p, first);
    }
    return math::eval_field(p, first);
}
}  // namespace

SeqEditorScreen::SeqEditorScreen() : SlotEditorScreen(kFieldCount, kRowH) {}

const char* SeqEditorScreen::title() const {
    return "SEQUENCE EDITOR";
}

void SeqEditorScreen::field_label(int i, char* buf, size_t buf_len) const {
    if (i == kFieldNMin) {
        std::snprintf(buf, buf_len, "nMin=");
    } else if (is_expr_field(i)) {
        std::snprintf(buf, buf_len, "%c(n)=", seq_name(seq_of(i)));
    } else {
        std::snprintf(buf, buf_len, "%c(nMin)=", seq_name(seq_of(i)));
    }
}

platform::Color SeqEditorScreen::field_label_color(int i) const {
    if (i == kFieldNMin) {
        return platform::colors::kGreen;
    }
    return function_color(seq_of(i));
}

const char* SeqEditorScreen::field_text(int i) const {
    if (i == kFieldNMin) {
        math::format_number(graph::state().n_min, g_scratch, sizeof(g_scratch));
        return g_scratch;
    }
    if (is_expr_field(i)) {
        return funcs().expr[seq_of(i)];
    }
    return seed_text(seq_of(i));
}

void SeqEditorScreen::set_field_text(int i, const char* s) {
    if (i == kFieldNMin) {
        double v = 0;
        if (math::eval_field(s, &v)) {
            graph::state().n_min = std::floor(v);
            if (graph::state().n_max < graph::state().n_min) {
                graph::state().n_max = graph::state().n_min + 9;
            }
            graph::state().plot_start = std::max(graph::state().plot_start, graph::state().n_min);
        }
    } else if (is_expr_field(i)) {
        const int sq = seq_of(i);
        std::snprintf(funcs().expr[sq], config::kMaxExprLen, "%s", s);
        if (funcs().expr[sq][0] != 0) {
            funcs().enabled[sq] = true;  // Auto-enable on entry (Y= behavior)
        }
    } else {
        const int sq = seq_of(i);
        double first = 0;
        double second = 0;
        if (parse_seed(s, &first, &second)) {
            funcs().seed1[sq] = first;
            funcs().seed2[sq] = second;
        }
    }
    save_graph_state();
    invalidate_row(i);
}

void SeqEditorScreen::toggle_field(int i) {
    if (!is_expr_field(i)) {
        return;
    }
    funcs().enabled[seq_of(i)] = !funcs().enabled[seq_of(i)];
    save_graph_state();
    invalidate_row(i);
}

void SeqEditorScreen::clear_field(int i) {
    if (i == kFieldNMin) {
        graph::state().n_min = 1.0;
    } else if (is_expr_field(i)) {
        funcs().expr[seq_of(i)][0] = 0;
        funcs().enabled[seq_of(i)] = false;
    } else {
        funcs().seed1[seq_of(i)] = 0;
        funcs().seed2[seq_of(i)] = 0;
    }
    save_graph_state();
    invalidate_row(i);
}

bool SeqEditorScreen::field_checked(int i) const {
    return is_expr_field(i) && funcs().enabled[seq_of(i)];
}

bool SeqEditorScreen::field_valid(int i, const char* text) const {
    // nMin + seed rows are re-formatted numerics — always valid. Only
    // the u/v/w(n)= expression rows need the sequence-aware check, so a
    // recurrence like u(n-1)+1 isn't wrongly flagged red by the plain
    // engine (HW 2026-07-27).
    if (i == kFieldNMin || !is_expr_field(i)) {
        return true;
    }
    return math::seqexpr::compiles(text);
}

bool SeqEditorScreen::field_has_checkbox(int i) const {
    return is_expr_field(i);
}

int SeqEditorScreen::label_width_chars() const {
    return 8;  // "u(nMin)="
}

SeqEditorScreen& seq_editor_screen() {
    static SeqEditorScreen instance;
    return instance;
}

}  // namespace apps
