#include "apps/solver_screen.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/numeric_solve.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/window_screen.hpp"

namespace apps {

namespace {

constexpr int kTopY = 44;
constexpr int kRowH = 28;
constexpr int kSoftkeyY = 300;

// Variable cycle: x first (the common case), then t, theta, then the
// rest of a-z ('e' excluded — it's Euler's constant in the engine).
struct VarChoice {
    const char* label;
    int slot;
};

constexpr int kVarCycle = 26;
const VarChoice kVars[kVarCycle] = {
    {"x", 'x' - 'a'}, {"t", 't' - 'a'}, {"theta", math::Variables::kTheta},
    {"a", 0},         {"b", 1},         {"c", 2},
    {"d", 3},         {"f", 5},         {"g", 6},
    {"h", 7},         {"i", 8},         {"j", 9},
    {"k", 10},        {"l", 11},        {"m", 12},
    {"n", 13},        {"o", 14},        {"p", 15},
    {"q", 16},        {"r", 17},        {"s", 18},
    {"u", 'u' - 'a'}, {"v", 'v' - 'a'}, {"w", 'w' - 'a'},
    {"y", 'y' - 'a'}, {"z", 'z' - 'a'},
};

}  // namespace

void SolverScreen::clear_result() {
    result_[0][0] = 0;
    result_[1][0] = 0;
    result_[2][0] = 0;
    msg_ = nullptr;
}

void SolverScreen::on_activate() {
    editing_ = false;
    clear_result();
    invalidate_all();
}

void SolverScreen::begin_edit(bool from_empty) {
    if (from_empty || (row_ == kRowGuess && !has_guess_)) {
        input_.set_text("");
    } else if (row_ == kRowEquation) {
        input_.set_text(equation_);
    } else {
        const math::calc_t v = row_ == kRowLower ? lower_ : row_ == kRowUpper ? upper_ : guess_;
        char buf[24];
        math::format_number(v, buf, sizeof(buf));
        input_.set_text(buf);
    }
    editing_ = true;
    invalidate_all();
}

void SolverScreen::commit_edit() {
    if (row_ == kRowEquation) {
        // Bounded copy (input lines run to 128 chars; the field is 96).
        std::snprintf(equation_, sizeof(equation_), "%.*s", static_cast<int>(sizeof(equation_)) - 1,
                      input_.text());
        clear_result();
    } else if (row_ == kRowGuess && input_.text()[0] == 0) {
        has_guess_ = false;  // Cleared: back to the bracketed mode
        clear_result();
    } else {
        math::calc_t v = 0;
        if (math::eval_field(input_.text(), &v)) {
            if (row_ == kRowLower) {
                lower_ = v;
            } else if (row_ == kRowUpper) {
                upper_ = v;
            } else {
                guess_ = v;
                has_guess_ = true;
            }
            clear_result();
        }
    }
    editing_ = false;
    invalidate_all();
}

void SolverScreen::solve() {
    clear_result();
    if (equation_[0] == 0) {
        msg_ = "Enter an equation";
        return;
    }

    // Equation form: split a top-level '=' into lhs/rhs.
    char work[sizeof(equation_)];
    std::snprintf(work, sizeof(work), "%s", equation_);
    char* eq = nullptr;
    int depth = 0;
    for (char* q = work; *q != 0; ++q) {
        if (*q == '(' || *q == '{') {
            ++depth;
        } else if (*q == ')' || *q == '}') {
            --depth;
        } else if (*q == '=' && depth == 0) {
            eq = q;
            break;
        }
    }

    const int slot = kVars[var_].slot;
    const math::calc_t lo = has_guess_ ? guess_ : lower_;
    const math::calc_t hi = has_guess_ ? guess_ : upper_;
    math::SolveResult sr;
    if (eq != nullptr) {
        *eq = 0;
        sr = math::numeric_solve_equation(work, eq + 1, slot, lo, hi);
    } else {
        sr = math::numeric_solve(work, slot, lo, hi);
    }
    if (!sr.converged) {
        msg_ = sr.error != nullptr ? sr.error : "No solution found";
        return;
    }

    char num[24];
    math::format_number(sr.root, num, sizeof(num));
    std::snprintf(result_[0], sizeof(result_[0]), "%s = %s", kVars[var_].label, num);
    // Residual as %.3g: format_number would round 1e-13 to "0" and
    // hide exactly the information this line exists to show.
    std::snprintf(result_[1], sizeof(result_[1]), "f(%s) = %.3g", kVars[var_].label,
                  static_cast<double>(sr.residual));
    std::snprintf(result_[2], sizeof(result_[2]), "%d iterations", sr.iterations);
    // The found root lands in the variable and Ans, TI-solver style.
    math::engine().vars().set_real(slot, sr.root);
    math::engine().vars().set_real(math::Variables::kAns, sr.root);
}

bool SolverScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    if (editing_) {
        if (ev.key == Key::kEnter) {
            commit_edit();
            return true;
        }
        if (ev.key == Key::kEscape) {
            editing_ = false;
            invalidate_all();
            return true;
        }
        if (input_.on_key(ev)) {
            invalidate_all();
            return true;
        }
        return false;
    }

    switch (ev.key) {
        case Key::kUp:
            if (row_ > 0) {
                --row_;
                invalidate_all();
            }
            return true;
        case Key::kDown:
            if (row_ < kRowCount - 1) {
                ++row_;
                invalidate_all();
            }
            return true;
        case Key::kLeft:
        case Key::kRight:
            if (row_ == kRowVariable) {
                const int dir = ev.key == Key::kRight ? 1 : -1;
                var_ = (var_ + dir + kVarCycle) % kVarCycle;
                clear_result();
                invalidate_all();
            }
            return true;
        case Key::kEnter:
            if (row_ == kRowSolve) {
                solve();
                invalidate_all();
            } else if (row_ == kRowVariable) {
                var_ = (var_ + 1) % kVarCycle;
                clear_result();
                invalidate_all();
            } else {
                begin_edit(false);
            }
            return true;
        case Key::kDel:
            if (row_ == kRowEquation || row_ == kRowLower || row_ == kRowUpper ||
                row_ == kRowGuess) {
                begin_edit(true);
            }
            return true;
        // Global F-key scheme (D20).
        case Key::kF1:
            push_mode_editor();
            return true;
        case Key::kF2:
            ui::screen_manager().push(&window_screen());
            return true;
        case Key::kF3:
            ui::screen_manager().push(&mode_screen());
            return true;
        case Key::kF4:
            goto_graph_trace();
            return true;
        case Key::kF5:
            ui::screen_manager().push(&graph_screen());
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void SolverScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "SOLVER");

    for (int i = 0; i < kRowCount; ++i) {
        const int y = kTopY + i * kRowH;
        if (i == row_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH - 4,
                         platform::Color::from_rgb(0, 0, 60));
        }
        const char* label = nullptr;
        char value[40] = {};
        switch (i) {
            case kRowEquation:
                label = "Equation";
                std::snprintf(value, sizeof(value), "%.*s", static_cast<int>(sizeof(value)) - 1,
                              equation_);
                break;
            case kRowVariable:
                label = "Variable";
                std::snprintf(value, sizeof(value), "%s", kVars[var_].label);
                break;
            case kRowLower:
                label = "Lower";
                math::format_number(lower_, value, sizeof(value));
                break;
            case kRowUpper:
                label = "Upper";
                math::format_number(upper_, value, sizeof(value));
                break;
            case kRowGuess:
                label = "Guess";
                if (has_guess_) {
                    math::format_number(guess_, value, sizeof(value));
                } else {
                    std::snprintf(value, sizeof(value), "(bracket)");
                }
                break;
            default:
                label = "Solve";
                std::snprintf(value, sizeof(value), "[ENTER]");
                break;
        }
        font.draw_string(fb, 12, y, label, kWhite);
        if (editing_ && i == row_) {
            const int vx = platform::kScreenW * 2 / 5;
            input_.render(fb, vx, y, platform::kScreenW - vx - 12, font, true);
        } else {
            font.draw_string(fb, platform::kScreenW - 12 - font.text_width(value), y, value,
                             kGreen);
        }
    }

    // Result block below the form.
    const int ry = kTopY + kRowCount * kRowH + 8;
    for (int i = 0; i < 3; ++i) {
        if (result_[i][0] != 0) {
            font.draw_string(fb, 12, ry + i * 18, result_[i], i == 0 ? kWhite : kGridLine);
        }
    }
    if (msg_ != nullptr) {
        font.draw_string(fb, 12, ry, msg_, kRed);
    }

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "ENTER:EDIT/SOLVE DEL:CLEAR ESC:BACK", kGrayLine);
}

SolverScreen& solver_screen() {
    static SolverScreen instance;
    return instance;
}

}  // namespace apps
