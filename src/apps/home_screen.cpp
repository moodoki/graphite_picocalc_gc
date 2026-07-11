#include "apps/home_screen.hpp"

#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "render/layout_builder.hpp"
#include "render/layout_render.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/window_screen.hpp"
#include "apps/y_editor.hpp"

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

void HomeScreen::push_entry(const char* expr, const char* result, bool error) {
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
    char line[140];
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

void HomeScreen::evaluate_input() {
    if (input_.empty()) {
        return;
    }
    const auto res = math::engine().evaluate(input_.text());

    char result[32];
    if (res.ok) {
        char num[24];
        math::format_number(res.value, num, sizeof(num));
        if (res.stored_var >= 0) {
            const char name =
                res.stored_var < 26 ? static_cast<char>('A' + res.stored_var) : 't';  // theta
            std::snprintf(result, sizeof(result), "%s>%c", num, name);
        } else {
            std::snprintf(result, sizeof(result), "%s", num);
        }
    } else {
        std::snprintf(result, sizeof(result), "%s", res.error);
    }

    push_entry(input_.text(), result, !res.ok);
    if (res.ok) {
        persist_history_line(input_.text(), result);
        save_variables();
    }
    input_.clear();
    hist_nav_ = -1;
    pending_[0] = 0;
}

bool HomeScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kEnter:
            if (!input_.empty()) {
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
                if (scroll_ < history_count_ - 1) {
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
        case Key::kF1:
            ui::screen_manager().push(&y_editor_screen());
            return true;
        case Key::kF2:
            ui::screen_manager().push(&window_screen());
            return true;
        case Key::kF3:
            ui::screen_manager().push(&graph_screen());
            return true;
        case Key::kF4:
            ui::screen_manager().push(&mode_screen());
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
    int y = kInputY - 4;
    for (int n = scroll_; y > kStatusH + lh; ++n) {
        const Entry* e = entry_from_newest(n);
        if (e == nullptr) {
            break;
        }
        // Result line (plain text).
        y -= lh;
        const int rx = platform::kScreenW - font.text_width(e->result) - 4;
        font.draw_string(fb, rx, y, e->result, e->error ? kRed : kWhite);

        // Expression line(s), pretty-printed. Build to learn the height,
        // then render immediately (the pool is reset on the next build).
        render::LayoutNode* root = render::build_layout(e->expr, metrics);
        const int eh = root != nullptr ? root->height : lh;
        y -= eh + 2;
        render::render_node(root, fb, 4, y, font, kGrayLine);
        y -= 2;
    }

    // Input area
    fb.draw_hline(0, kInputY, platform::kScreenW, kGrayLine);
    font.draw_char(fb, 2, kInputY + 8, '>', kGreen);
    input_.render(fb, 2 + font.width() + 2, kInputY + 8, platform::kScreenW - font.width() - 8,
                  font, true);

    const char* keys[6] = {"Y=", "WIN", "GRPH", "MODE", "", "DIAG"};
    ui::draw_softkeys(fb, keys);
}

HomeScreen& home_screen() {
    static HomeScreen instance;
    return instance;
}

}  // namespace apps
