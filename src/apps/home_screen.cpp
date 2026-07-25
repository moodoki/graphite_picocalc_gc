#include "apps/home_screen.hpp"

#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/complex_expr.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/list_expr.hpp"
#include "math/lists.hpp"
#include "math/mat_expr.hpp"
#include "math/matrix.hpp"
#include "math/solve_expr.hpp"
#include "render/layout_builder.hpp"
#include "render/layout_render.hpp"
#include "apps/calc_menu.hpp"
#include "apps/dist_screen.hpp"
#include "apps/files_screen.hpp"
#include "apps/graph_screen.hpp"
#include "apps/help_screen.hpp"
#include "apps/infer_screen.hpp"
#include "apps/list_editor.hpp"
#include "apps/matrix_editor.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/plot_screen.hpp"
#include "apps/solver_screen.hpp"
#include "apps/stats_screen.hpp"
#include "apps/window_screen.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {
constexpr const char* kHistoryPath = "/picocalc/history.txt";
constexpr const char* kVarsPath = "/picocalc/variables.dat";

// Screen layout (spec section 4.4, sized for the interim 8x12 font).
constexpr int kStatusH = 16;
constexpr int kInputY = 268;
constexpr int kSoftkeyY = 296;
}  // namespace

// Dirty bands for partial redraw (5.6 part 2). The input band is the
// typing fast path (~28 of 320 rows). Enter includes the status bar so
// the battery/mode indicators refresh at every evaluation.
void HomeScreen::invalidate_input() {
    invalidate(kInputY, kSoftkeyY);
}

void HomeScreen::invalidate_history() {
    invalidate(kStatusH, kInputY);
}

void HomeScreen::on_activate() {}

const HomeScreen::Entry* HomeScreen::entry_from_newest(int n) const {
    if (n >= history_count_) {
        return nullptr;
    }
    const int idx = (history_head_ - 1 - n + 2 * kMaxHistory) % kMaxHistory;
    return &history_[idx];
}

// Entries visible in the scrollback: everything since the last `cls`,
// capped by what the ring still holds. The recall walk ignores this.
int HomeScreen::visible_count() const {
    const uint32_t since = entries_total_ - cls_mark_;
    return since < static_cast<uint32_t>(history_count_) ? static_cast<int>(since) : history_count_;
}

void HomeScreen::push_entry(const char* expr, const char* result, bool error) {
    ++entries_total_;
    Entry& e = history_[history_head_];
    std::strncpy(e.expr, expr, sizeof(e.expr) - 1);
    e.expr[sizeof(e.expr) - 1] = 0;
    std::strncpy(e.result, result, sizeof(e.result) - 1);
    e.result[sizeof(e.result) - 1] = 0;
    e.error = error;
    history_head_ = (history_head_ + 1) % kMaxHistory;
    if (history_count_ < kMaxHistory) {
        ++history_count_;
    }
    scroll_ = 0;
    // Keep the full (untruncated) newest result for LEFT/RIGHT panning.
    std::strncpy(result_full_, result, sizeof(result_full_) - 1);
    result_full_[sizeof(result_full_) - 1] = 0;
    result_scroll_ = 0;
}

int HomeScreen::result_max_scroll() const {
    const int win = (platform::kScreenW - 8) / gfx::main_font().width();
    const int len = static_cast<int>(std::strlen(result_full_));
    return len > win ? len - win : 0;
}

void HomeScreen::persist_history_line(const char* expr, const char* result) {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    char line[256];
    const int n = std::snprintf(line, sizeof(line), "%s\t%s\n", expr, result);
    fs.append_file(kHistoryPath, reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(n));
}

namespace {

// variables.dat image (4D.15): versioned since complex storage widened
// Variables — the pre-PCV1 file was a raw 224-byte vars[] dump with no
// header, so it simply fails the magic check and is ignored (one-time
// variable reset on first boot, same precedent as PCL/PCM/PCG bumps).
struct VarsImage {
    char magic[4];
    math::calc_t vars[math::Variables::kCount];
    math::calc_t imag[math::Variables::kCount];
};
constexpr char kVarsMagic[4] = {'P', 'C', 'V', '1'};

}  // namespace

void HomeScreen::save_variables() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    const auto& v = math::engine().vars();
    static VarsImage img;
    std::memcpy(img.magic, kVarsMagic, sizeof(img.magic));
    std::memcpy(img.vars, v.vars, sizeof(img.vars));
    std::memcpy(img.imag, v.imag, sizeof(img.imag));
    fs.write_file(kVarsPath, reinterpret_cast<const uint8_t*>(&img), sizeof(img));
}

void HomeScreen::load_variables() {
    auto& fs = platform::storage();
    auto& v = math::engine().vars();
    static VarsImage img;
    const int n = fs.read_file(kVarsPath, reinterpret_cast<uint8_t*>(&img), sizeof(img));
    if (n != static_cast<int>(sizeof(img)) ||
        std::memcmp(img.magic, kVarsMagic, sizeof(img.magic)) != 0) {
        return;  // Missing, old-format, or truncated: keep defaults
    }
    std::memcpy(v.vars, img.vars, sizeof(v.vars));
    std::memcpy(v.imag, img.imag, sizeof(v.imag));
}

void HomeScreen::load_state() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    load_variables();

    // Load the tail of the history file (plaintext "expr\tresult" lines,
    // decision D4). 8 KB tail is at least 50 full-size lines.
    static char tail[8192];
    const int n = fs.read_file(kHistoryPath, reinterpret_cast<uint8_t*>(tail), sizeof(tail) - 1);
    if (n <= 0) {
        return;
    }
    tail[n] = 0;
    char* line = tail;
    while (line != nullptr && *line != 0) {
        char* nl = std::strchr(line, '\n');
        if (nl != nullptr) {
            *nl = 0;
        }
        char* sep = std::strchr(line, '\t');
        if (sep != nullptr) {
            *sep = 0;
            push_entry(line, sep + 1, false);
        }
        line = nl != nullptr ? nl + 1 : nullptr;
    }
    scroll_ = 0;
}

namespace {

// "num" or "num>a" for the store form (shared by the scalar and
// list-expression result paths).
void format_scalar_result(const math::EvalResult& res, char* out, size_t out_len) {
    if (!res.ok) {
        std::snprintf(out, out_len, "%s", res.error);
        return;
    }
    char num[24];
    math::format_number(res.value, num, sizeof(num));
    if (res.stored_var >= 0) {
        const char name =
            res.stored_var < 26 ? static_cast<char>('a' + res.stored_var) : 't';  // theta
        std::snprintf(out, out_len, "%s%c%c", num, gfx::kGlyphStore, name);
    } else {
        std::snprintf(out, out_len, "%s", num);
    }
}

}  // namespace

void HomeScreen::evaluate_input() {
    if (input_.empty()) {
        return;
    }

    // Inline solve() calls become numeric literals first (Phase 4A),
    // so they compose inside any downstream path. History shows the
    // original input; evaluation continues on the substituted text.
    char expr[160];
    std::snprintf(expr, sizeof(expr), "%s", input_.text());
    if (math::solveexpr::contains_solve(expr)) {
        const char* serr = nullptr;
        if (!math::solveexpr::substitute(expr, sizeof(expr), &serr)) {
            push_entry(input_.text(), serr, true);
            input_.clear();
            hist_nav_ = -1;
            pending_[0] = 0;
            return;
        }
    }

    // Matrix expressions (Phase 4A) get next crack — [X] tokens are
    // unambiguous. Kind::kNone means "not matrix syntax".
    const auto mres = math::matexpr::evaluate(expr);
    if (mres.kind != math::matexpr::Kind::kNone) {
        char result[128];
        bool error = false;
        if (mres.kind == math::matexpr::Kind::kError) {
            std::snprintf(result, sizeof(result), "%s", mres.error);
            error = true;
        } else if (mres.kind == math::matexpr::Kind::kScalar) {
            format_scalar_result(mres.scalar, result, sizeof(result));
            error = !mres.scalar.ok;
        } else if (mres.kind == math::matexpr::Kind::kList) {
            char text[120];
            math::listexpr::format_list(*mres.list, text, sizeof(text));
            if (mres.stored_list >= 0) {
                std::snprintf(result, sizeof(result), "%s%cl%c", text, gfx::kGlyphStore,
                              static_cast<char>('1' + mres.stored_list));
            } else {
                std::snprintf(result, sizeof(result), "%s", text);
            }
        } else if (mres.kind == math::matexpr::Kind::kText) {
            std::snprintf(result, sizeof(result), "%s", mres.text);
        } else {
            char text[120];
            math::matexpr::format_matrix(*mres.matrix, text, sizeof(text));
            if (mres.stored_matrix >= 0) {
                std::snprintf(result, sizeof(result), "%s%c[%c]", text, gfx::kGlyphStore,
                              static_cast<char>('A' + mres.stored_matrix));
            } else {
                std::snprintf(result, sizeof(result), "%s", text);
            }
        }
        push_entry(input_.text(), result, error);
        if (!error) {
            persist_history_line(input_.text(), result);
            save_variables();
            if (mres.matrices_modified) {
                math::matrices().save(platform::storage(), mres.stored_matrix);
            }
            if (mres.lists_modified) {
                math::lists().save(platform::storage(), mres.stored_list);
            }
        }
        input_.clear();
        hist_nav_ = -1;
        pending_[0] = 0;
        return;
    }

    // List expressions (Phase 3A) next; Kind::kNone means
    // "not list syntax" and falls through to the scalar engine.
    const auto lres = math::listexpr::evaluate(expr);
    if (lres.kind != math::listexpr::Kind::kNone) {
        char result[128];
        bool error = false;
        if (lres.kind == math::listexpr::Kind::kError) {
            std::snprintf(result, sizeof(result), "%s", lres.error);
            error = true;
        } else if (lres.kind == math::listexpr::Kind::kScalar && lres.scalar_complex) {
            // Standalone sum/mean of a complex list (4D.24): commit Ans
            // here, mirroring the complexexpr contract.
            math::engine().vars().set_complex(math::Variables::kAns, lres.cvalue.re,
                                              lres.cvalue.im);
            math::format_complex(lres.cvalue, math::number_mode(), result, sizeof(result));
        } else if (lres.kind == math::listexpr::Kind::kScalar) {
            format_scalar_result(lres.scalar, result, sizeof(result));
            error = !lres.scalar.ok;
        } else {
            char text[120];
            math::listexpr::format_list(*lres.list, text, sizeof(text));
            if (lres.stored_list >= 0) {
                std::snprintf(result, sizeof(result), "%s%cl%c", text, gfx::kGlyphStore,
                              static_cast<char>('1' + lres.stored_list));
            } else {
                std::snprintf(result, sizeof(result), "%s", text);
            }
        }
        push_entry(input_.text(), result, error);
        if (!error) {
            persist_history_line(input_.text(), result);
            save_variables();
            if (lres.lists_modified) {
                math::lists().save(platform::storage(), lres.stored_list);
            }
        }
        input_.clear();
        hist_nav_ = -1;
        pending_[0] = 0;
        return;
    }

    // Scalar path (Phase 4C, D30): complex-aware whenever the number
    // mode isn't REAL or the input names `i`. In plain REAL mode, the
    // complex evaluator still runs once as a side-effect-free probe —
    // only when the real engine would otherwise show a bare NaN — so
    // sqrt(-4) etc. get "Non-real result" instead, without committing
    // Ans/a store twice (math::complexexpr::evaluate never mutates
    // engine state; only this dispatch does, exactly once).
    char result[128];
    bool error = false;
    // Complex-valued variables force the complex path too (4D.15): in
    // REAL mode that yields the pointed "Non-real result" error below;
    // in RECT/POLAR the reference resolves normally.
    const bool force_complex = math::number_mode() != math::NumberMode::kReal ||
                               math::complexexpr::mentions_i(expr) || math::refs_complex_var(expr);

    if (force_complex) {
        const auto cres = math::complexexpr::evaluate(expr);
        if (!cres.ok) {
            std::snprintf(result, sizeof(result), "%s", cres.error);
            error = true;
        } else if (math::number_mode() == math::NumberMode::kReal && !cres.value.is_real()) {
            std::snprintf(result, sizeof(result), "Non-real result");
            error = true;
        } else if (cres.value.is_real()) {
            math::engine().vars().set_real(math::Variables::kAns, cres.value.re);
            if (cres.stored_var >= 0) {
                math::engine().vars().set_real(cres.stored_var, cres.value.re);
            }
            math::EvalResult r;
            r.ok = true;
            r.value = cres.value.re;
            r.stored_var = cres.stored_var;
            format_scalar_result(r, result, sizeof(result));
        } else {
            // Complex commit (4D.15): Ans and the store target hold the
            // full value; real-only readers error on them (D37).
            math::engine().vars().set_complex(math::Variables::kAns, cres.value.re, cres.value.im);
            if (cres.stored_var >= 0) {
                math::engine().vars().set_complex(cres.stored_var, cres.value.re, cres.value.im);
            }
            char num[64];
            math::format_complex(cres.value, math::number_mode(), num, sizeof(num));
            if (cres.stored_var >= 0) {
                const char name =
                    cres.stored_var < 26 ? static_cast<char>('a' + cres.stored_var) : 't';  // theta
                std::snprintf(result, sizeof(result), "%s%c%c", num, gfx::kGlyphStore, name);
            } else {
                std::snprintf(result, sizeof(result), "%s", num);
            }
        }
    } else {
        const auto probe = math::complexexpr::evaluate(expr);
        if (probe.ok && !probe.value.is_real()) {
            std::snprintf(result, sizeof(result), "Non-real result");
            error = true;
        } else {
            const auto res = math::engine().evaluate(expr);
            format_scalar_result(res, result, sizeof(result));
            error = !res.ok;
        }
    }

    push_entry(input_.text(), result, error);
    if (!error) {
        persist_history_line(input_.text(), result);
        save_variables();
    }
    input_.clear();
    hist_nav_ = -1;
    pending_[0] = 0;
}

// Typed commands (2026-07-18): lowercase-only (input is
// case-sensitive), matched against the trimmed line before math
// evaluation. Commands don't enter history.
bool HomeScreen::handle_command(const char* cmd) {
    if (std::strcmp(cmd, "cls") == 0) {
        // Session-level clear: hide the current scrollback, keep the
        // recall walk and history.txt intact.
        cls_mark_ = entries_total_;
        scroll_ = 0;
        result_full_[0] = 0;
        result_scroll_ = 0;
        return true;
    }
    if (std::strcmp(cmd, "clrhist") == 0) {
        history_count_ = 0;
        history_head_ = 0;
        entries_total_ = 0;
        cls_mark_ = 0;
        scroll_ = 0;
        hist_nav_ = -1;
        result_full_[0] = 0;
        result_scroll_ = 0;
        auto& fs = platform::storage();
        if (fs.mounted()) {
            fs.delete_file(kHistoryPath);
        }
        return true;
    }
    // Short aliases (D24): "?" for help, singular "list"/"stat" for the
    // frequently-typed screens. No engine collisions — none parse as
    // expressions.
    if (std::strcmp(cmd, "help") == 0 || std::strcmp(cmd, "?") == 0) {
        ui::screen_manager().push(&help_screen());
        return true;
    }
    if (std::strcmp(cmd, "files") == 0) {
        ui::screen_manager().push(&files_screen());
        return true;
    }
    if (std::strcmp(cmd, "lists") == 0 || std::strcmp(cmd, "list") == 0) {
        ui::screen_manager().push(&list_editor());
        return true;
    }
    if (std::strcmp(cmd, "stats") == 0 || std::strcmp(cmd, "stat") == 0) {
        ui::screen_manager().push(&stats_screen());
        return true;
    }
    if (std::strcmp(cmd, "dist") == 0) {
        ui::screen_manager().push(&dist_screen());
        return true;
    }
    if (std::strcmp(cmd, "test") == 0 || std::strcmp(cmd, "infer") == 0) {
        ui::screen_manager().push(&infer_screen());
        return true;
    }
    if (std::strcmp(cmd, "plot") == 0 || std::strcmp(cmd, "plots") == 0) {
        ui::screen_manager().push(&plot_screen());
        return true;
    }
    if (std::strcmp(cmd, "matrix") == 0 || std::strcmp(cmd, "mat") == 0) {
        ui::screen_manager().push(&matrix_editor());
        return true;
    }
    // Bare `solve` opens the form screen; `solve(...)` with arguments
    // stays an expression (inline solve, handled in evaluate_input).
    if (std::strcmp(cmd, "solve") == 0 || std::strcmp(cmd, "solver") == 0) {
        ui::screen_manager().push(&solver_screen());
        return true;
    }
    // Graph analysis (4B): jump to the graph with the CALC menu open.
    if (std::strcmp(cmd, "calc") == 0 || std::strcmp(cmd, "analyze") == 0) {
        ui::screen_manager().push(&graph_screen());
        ui::screen_manager().push(&calc_menu());
        return true;
    }
    if (std::strcmp(cmd, "diag") == 0 && diag_screen_ != nullptr) {
        ui::screen_manager().push(diag_screen_);
        return true;
    }
    return false;
}

bool HomeScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kEnter:
            if (!input_.empty()) {
                // Trimmed command match first (cls, help, ...).
                char cmd[16];
                const char* t = input_.text();
                while (*t == ' ') {
                    ++t;
                }
                size_t len = std::strlen(t);
                while (len > 0 && t[len - 1] == ' ') {
                    --len;
                }
                if (len > 0 && len < sizeof(cmd)) {
                    std::memcpy(cmd, t, len);
                    cmd[len] = 0;
                    if (handle_command(cmd)) {
                        input_.clear();
                        hist_nav_ = -1;
                        pending_[0] = 0;
                        invalidate(0, kSoftkeyY);
                        return true;
                    }
                }
                evaluate_input();
                invalidate(0, kSoftkeyY);  // History + input + status bar
            }
            return true;
        case Key::kUp:
            // Modifier+UP scrolls the history view; plain UP walks back
            // through past inputs shell-style (supersedes task 5.5's
            // single-recall). Alt/Ctrl, because the STM32 swallows
            // Shift on arrow keys (D12, HW-verified 2026-07-11); shift
            // kept in case a future keyboard firmware reports it.
            if (ev.alt_held || ev.ctrl_held || ev.shift_held) {
                // View scroll stops at the cls watermark; the recall
                // walk below intentionally does not.
                if (scroll_ < visible_count() - 1) {
                    ++scroll_;
                    invalidate_history();
                }
            } else if (hist_nav_ < history_count_ - 1) {
                if (hist_nav_ < 0) {
                    // Stash whatever was being typed for DOWN to restore.
                    std::strncpy(pending_, input_.text(), sizeof(pending_) - 1);
                    pending_[sizeof(pending_) - 1] = 0;
                }
                ++hist_nav_;
                input_.set_text(entry_from_newest(hist_nav_)->expr);
                invalidate_input();
            }
            return true;
        case Key::kDown:
            if (ev.alt_held || ev.ctrl_held || ev.shift_held) {
                if (scroll_ > 0) {
                    --scroll_;
                    invalidate_history();
                }
            } else if (hist_nav_ > 0) {
                --hist_nav_;
                input_.set_text(entry_from_newest(hist_nav_)->expr);
                invalidate_input();
            } else if (hist_nav_ == 0) {
                hist_nav_ = -1;
                input_.set_text(pending_);
                invalidate_input();
            }
            return true;
        case Key::kLeft:
        case Key::kRight:
            // With the input empty and the view pinned to newest, arrows
            // pan the (possibly truncated) newest result instead of moving
            // a cursor: RIGHT reveals later elements, LEFT walks back
            // toward the start (testdrive 2026-07-20). Otherwise they fall
            // through to the input line's cursor movement.
            if (input_.empty() && scroll_ == 0) {
                const int maxs = result_max_scroll();
                if (maxs > 0) {
                    if (ev.key == Key::kRight && result_scroll_ < maxs) {
                        ++result_scroll_;
                        invalidate_history();
                    } else if (ev.key == Key::kLeft && result_scroll_ > 0) {
                        --result_scroll_;
                        invalidate_history();
                    }
                    return true;
                }
            }
            if (input_.on_key(ev)) {
                invalidate_input();
                return true;
            }
            return false;
        case Key::kEscape:
            input_.clear();
            hist_nav_ = -1;
            invalidate_input();
            return true;
        // Global F-key scheme (2026-07-18 remap, TI-84-shaped):
        // F1 editor, F2 window, F3 mode, F4 trace, F5 graph.
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
        default:
            if (input_.on_key(ev)) {
                invalidate_input();
                return true;
            }
            return false;
    }
}

void HomeScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();
    const int lh = font.height() + 2;

    fb.clear(kBlack);

    ui::draw_status_bar(fb, "HOME");

    // History: newest at the bottom. Expressions render as 2D typeset
    // math (task 3.6); results stay as plain right-aligned text (a
    // number or error string is already display-ready).
    const render::Metrics metrics{font.width(), font.height()};
    const int visible = visible_count();  // cls hides older entries
    int y = kInputY - 4;
    for (int n = scroll_; y > kStatusH + lh && n < visible; ++n) {
        const Entry* e = entry_from_newest(n);
        if (e == nullptr) {
            break;
        }
        // Expression layout first: the entry's full height must be
        // known *before* drawing, or a tall pretty-printed expression
        // ends up rendered across the status bar (HW 2026-07-18).
        render::LayoutNode const* root = render::build_layout(e->expr, metrics);
        const int eh = root != nullptr ? root->height : lh;
        if (y - lh - (eh + 2) < kStatusH) {
            break;
        }

        // Result line (plain text). The newest result can overflow the
        // display; when it does, LEFT/RIGHT pan a left-anchored window
        // across the full text (testdrive 2026-07-20). Everything else
        // stays right-aligned and truncated.
        y -= lh;
        if (n == 0 && result_max_scroll() > 0) {
            const int win = (platform::kScreenW - 8) / font.width();
            int off = result_scroll_;
            const int maxs = result_max_scroll();
            off = off > maxs ? maxs : (off < 0 ? 0 : off);
            char window[64];
            const int w = win < static_cast<int>(sizeof(window)) - 1
                              ? win
                              : static_cast<int>(sizeof(window)) - 1;
            std::strncpy(window, result_full_ + off, static_cast<size_t>(w));
            window[w] = 0;
            font.draw_string(fb, 4, y, window, e->error ? kRed : kWhite);
        } else {
            const int rx = platform::kScreenW - font.text_width(e->result) - 4;
            font.draw_string(fb, rx, y, e->result, e->error ? kRed : kWhite);
        }

        // Expression line(s), rendered immediately (the pool is reset
        // on the next build).
        y -= eh + 2;
        render::render_node(root, fb, 4, y, font, kGrayLine);
        y -= 2;
    }

    // Input area
    fb.draw_hline(0, kInputY, platform::kScreenW, kGrayLine);
    font.draw_char(fb, 2, kInputY + 8, '>', kGreen);
    input_.render(fb, 2 + font.width() + 2, kInputY + 8, platform::kScreenW - font.width() - 8,
                  font, true);

    // Help discoverability (typed-command era): dim hint on the empty
    // input line pointing at `help`. kGridLine, not kGrayLine — the
    // latter reads near-white on the panel and was jarring here (HW
    // 2026-07-18).
    if (input_.empty()) {
        const char* hint = "type help for docs";
        font.draw_string(fb, platform::kScreenW - font.text_width(hint) - 4, kInputY + 8, hint,
                         kGridLine);
    }

    const char* f1 = "Y=";
    switch (graph::state().mode) {
        case graph::Mode::kParametric:
            f1 = "PAR";
            break;
        case graph::Mode::kPolar:
            f1 = "R=";
            break;
        default:
            break;
    }
    const char* const keys[6] = {f1, "WIN", "MODE", "TRC", "GRPH", ""};
    ui::draw_softkeys(fb, keys);
}

HomeScreen& home_screen() {
    static HomeScreen instance;
    return instance;
}

}  // namespace apps
