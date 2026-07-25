#include "apps/table_model.hpp"

#include <cstdio>
#include <limits>

#include "gfx/font.hpp"
#include "math/engine.hpp"

namespace apps {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Real theta glyph for the polar table column header (was ASCII "th",
// testdrive 2026-07-21).
const char kThetaLabel[] = {gfx::kGlyphTheta, 0};

// Function-mode column c -> Y slot index (enabled slots in order).
// Returns -1 when out of range.
int function_slot_for(const graph::GraphState& st, int col) {
    int c = 0;
    for (int i = 0; i < graph::kFunctionSlots; ++i) {
        if (st.y.enabled[i] && st.y.expr[i][0] != 0) {
            if (c == col) {
                return i;
            }
            ++c;
        }
    }
    return -1;
}

// Parametric column c -> pair index; even columns are X, odd are Y
// *within the enabled sequence* (each enabled pair contributes two).
int parametric_pair_for(const graph::GraphState& st, int col, bool* is_x) {
    int c = 0;
    for (int p = 0; p < graph::kParametricSlots; ++p) {
        if (st.param.enabled[p] && st.param.x_expr[p][0] != 0 && st.param.y_expr[p][0] != 0) {
            if (c == col) {
                *is_x = true;
                return p;
            }
            ++c;
            if (c == col) {
                *is_x = false;
                return p;
            }
            ++c;
        }
    }
    return -1;
}

int polar_slot_for(const graph::GraphState& st, int col) {
    int c = 0;
    for (int i = 0; i < graph::kPolarSlots; ++i) {
        if (st.polar.enabled[i] && st.polar.expr[i][0] != 0) {
            if (c == col) {
                return i;
            }
            ++c;
        }
    }
    return -1;
}

// Compile-eval-free one expression at `value` written into `var_slot`.
double eval_expr_at(const char* expr, int var_slot, double value) {
    auto& eng = math::engine();
    void* h = eng.compile(expr, var_slot);
    if (h == nullptr) {
        return kNaN;
    }
    const double r = eng.eval_compiled(h, var_slot, value);
    eng.free_compiled(h);
    return r;
}

}  // namespace

int table_column_count(const graph::GraphState& state) {
    int n = 0;
    switch (state.mode) {
        case graph::Mode::kParametric:
            for (int p = 0; p < graph::kParametricSlots; ++p) {
                if (state.param.enabled[p] && state.param.x_expr[p][0] != 0 &&
                    state.param.y_expr[p][0] != 0) {
                    n += 2;
                }
            }
            break;
        case graph::Mode::kPolar:
            for (int i = 0; i < graph::kPolarSlots; ++i) {
                if (state.polar.enabled[i] && state.polar.expr[i][0] != 0) {
                    ++n;
                }
            }
            break;
        default:
            for (int i = 0; i < graph::kFunctionSlots; ++i) {
                if (state.y.enabled[i] && state.y.expr[i][0] != 0) {
                    ++n;
                }
            }
            break;
    }
    return n;
}

void table_column_label(const graph::GraphState& state, int col, char* buf, size_t buf_len) {
    buf[0] = 0;
    switch (state.mode) {
        case graph::Mode::kParametric: {
            bool is_x = false;
            const int p = parametric_pair_for(state, col, &is_x);
            if (p >= 0) {
                std::snprintf(buf, buf_len, "%c%dT", is_x ? 'X' : 'Y', p + 1);
            }
            break;
        }
        case graph::Mode::kPolar: {
            const int s = polar_slot_for(state, col);
            if (s >= 0) {
                std::snprintf(buf, buf_len, "r%d", s + 1);
            }
            break;
        }
        default: {
            const int s = function_slot_for(state, col);
            if (s >= 0) {
                std::snprintf(buf, buf_len, "Y%d", s + 1);
            }
            break;
        }
    }
}

const char* table_independent_label(const graph::GraphState& state) {
    switch (state.mode) {
        case graph::Mode::kParametric:
            return "T";
        case graph::Mode::kPolar:
            return kThetaLabel;
        default:
            return "x";
    }
}

int evaluate_table_row(const graph::GraphState& state, double independent_value, double* results,
                       int max_results) {
    const int count = table_column_count(state);
    const int n = count < max_results ? count : max_results;
    auto& eng = math::engine();

    switch (state.mode) {
        case graph::Mode::kParametric: {
            const int slot = 't' - 'a';
            const math::calc_t saved = eng.vars()['t'];
            for (int c = 0; c < n; ++c) {
                bool is_x = false;
                const int p = parametric_pair_for(state, c, &is_x);
                results[c] =
                    p < 0 ? kNaN
                          : eval_expr_at(is_x ? state.param.x_expr[p] : state.param.y_expr[p], slot,
                                         independent_value);
            }
            eng.vars()['t'] = saved;
            break;
        }
        case graph::Mode::kPolar: {
            const math::calc_t saved = eng.vars().vars[math::Variables::kTheta];
            for (int c = 0; c < n; ++c) {
                const int s = polar_slot_for(state, c);
                results[c] = s < 0 ? kNaN
                                   : eval_expr_at(state.polar.expr[s], math::Variables::kTheta,
                                                  independent_value);
            }
            eng.vars().vars[math::Variables::kTheta] = saved;
            break;
        }
        default: {
            const int slot = 'x' - 'a';
            const math::calc_t saved = eng.vars()['x'];
            for (int c = 0; c < n; ++c) {
                const int s = function_slot_for(state, c);
                results[c] = s < 0 ? kNaN : eval_expr_at(state.y.expr[s], slot, independent_value);
            }
            eng.vars()['x'] = saved;
            break;
        }
    }
    return n;
}

}  // namespace apps
