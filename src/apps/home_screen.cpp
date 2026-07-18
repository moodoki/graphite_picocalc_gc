#include "apps/home_screen.hpp"

#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/list_expr.hpp"
#include "math/lists.hpp"
#include "render/layout_builder.hpp"
#include "render/layout_render.hpp"
#include "apps/files_screen.hpp"
#include "apps/graph_screen.hpp"
#include "apps/help_screen.hpp"
#include "apps/list_editor.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
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
}

void HomeScreen::persist_history_line(const char* expr, const char* result) {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    char line[160];
    const int n = std::snprintf(line, sizeof(line), "%s\t%s\n", expr, result);
    fs.append_file(kHistoryPath, reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(n));
}

void HomeScreen::save_variables() {
    auto& fs = platform::storage();
    if (!fs.mounted()) {
        return;
    }
    const auto& v = math::engine().vars();
    fs.write_file(kVarsPath, reinterpret_cast<const uint8_t*>(v.vars), sizeof(v.vars));
}

void HomeScreen::load_variables() {
    auto& fs = platform::storage();
    auto& v = math::engine().vars();
    fs.read_file(kVarsPath, reinterpret_cast<uint8_t*>(v.vars), sizeof(v.vars));
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
        std::snprintf(out, out_len, "%s>%c", num, name);
    } else {
        std::snprintf(out, out_len, "%s", num);
    }
}

}  // namespace

void HomeScreen::evaluate_input() {
    if (input_.empty()) {
        return;
    }

    // List expressions (Phase 3A) get first crack; Kind::kNone means
    // "not list syntax" and falls through to the scalar engine.
    const auto lres = math::listexpr::evaluate(input_.text());
    if (lres.kind != math::listexpr::Kind::kNone) {
        char result[48];
        bool error = false;
        if (lres.kind == math::listexpr::Kind::kError) {
            std::snprintf(result, sizeof(result), "%s", lres.error);
            error = true;
        } else if (lres.kind == math::listexpr::Kind::kScalar) {
            format_scalar_result(lres.scalar, result, sizeof(result));
            error = !lres.scalar.ok;
        } else {
            char text[40];
            math::listexpr::format_list(*lres.list, text, sizeof(text));
            if (lres.stored_list >= 0) {
                std::snprintf(result, sizeof(result), "%s>l%c", text,
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
                math::lists().save(platform::storage());
            }
        }
        input_.clear();
        hist_nav_ = -1;
        pending_[0] = 0;
        return;
    }

    const auto res = math::engine().evaluate(input_.text());
    char result[48];
    format_scalar_result(res, result, sizeof(result));

    push_entry(input_.text(), result, !res.ok);
    if (res.ok) {
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
        return true;
    }
    if (std::strcmp(cmd, "clrhist") == 0) {
        history_count_ = 0;
        history_head_ = 0;
        entries_total_ = 0;
        cls_mark_ = 0;
        scroll_ = 0;
        hist_nav_ = -1;
        auto& fs = platform::storage();
        if (fs.mounted()) {
            fs.delete_file(kHistoryPath);
        }
        return true;
    }
    if (std::strcmp(cmd, "help") == 0) {
        ui::screen_manager().push(&help_screen());
        return true;
    }
    if (std::strcmp(cmd, "files") == 0) {
        ui::screen_manager().push(&files_screen());
        return true;
    }
    if (std::strcmp(cmd, "lists") == 0) {
        ui::screen_manager().push(&list_editor());
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

        // Result line (plain text).
        y -= lh;
        const int rx = platform::kScreenW - font.text_width(e->result) - 4;
        font.draw_string(fb, rx, y, e->result, e->error ? kRed : kWhite);

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
