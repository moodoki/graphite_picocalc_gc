#include "apps/help_screen.hpp"

#include <cstdio>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/catalog.hpp"

namespace apps {

namespace {

constexpr int kTabCount = 3;
const char* const kTabNames[kTabCount] = {"FUNC", "KEYS", "SYNTAX"};

constexpr int kTopY = 24;
constexpr int kLineH = 14;
constexpr int kVisibleLines = 19;  // kTopY + 19*14 = 290, above softkeys

// Lines starting with '#' render as green section headers.
const char* const kKeysLines[] = {
    "#GLOBAL",
    "HOME     back to home screen",
    "ESC      back / cancel edit",
    "F6-F9    = Shift+F1-F4",
    "#HOME",
    "F1 Y=    function editor",
    "F2 WIN   window settings",
    "F3 GRPH  graph",
    "F4 MODE  mode settings",
    "F5 HELP  this help",
    "F6 DIAG  diagnostics",
    "UP       recall last entry",
    "UP/DOWN  walk input history",
    "Alt/Ctrl+UP/DOWN scroll view",
    "#GRAPH",
    "F1 TRC   toggle trace",
    "F2 Z+    zoom in",
    "F3 Z-    zoom out",
    "F5       open editor (Y=/PAR)",
    "S / T    ZStandard / ZTrig",
    "LT/RT    move trace cursor",
    "UP/DOWN  next curve (trace)",
    "#EDITORS (Y=, PARAMETRIC)",
    "ENTER/F1 edit field",
    "F2       toggle enable",
    "F3       clear field",
    "F4       graph",
    "#WINDOW",
    "ENTER/F1 edit value",
    "#MODE",
    "LT/RT    change value",
    "ENTER    select / reboot row",
};
constexpr int kKeysCount = sizeof(kKeysLines) / sizeof(kKeysLines[0]);

const char* const kSyntaxLines[] = {
    "#STORE",
    "expr->A     store result in A",
    "works for a-z and theta",
    "#CONSTANTS",
    "pi, e (Euler's number)",
    "E is reserved; use 1e10 for",
    "scientific literals",
    "#VARIABLES",
    "a-z, theta, ans",
    "ans = last result",
    "#FACTORIAL",
    "n! or fac(n)",
    "#ANGLE MODE",
    "MODE sets RADIAN or DEGREE;",
    "trig functions follow it",
    "#GRAPH MODES",
    "MODE > Graph mode: FUNC/PARAM",
    "PARAM plots X1T(t), Y1T(t)",
    "over Tmin..Tmax (see WINDOW)",
    "#HISTORY",
    "UP on empty input recalls;",
    "UP/DOWN walks past entries",
};
constexpr int kSyntaxCount = sizeof(kSyntaxLines) / sizeof(kSyntaxLines[0]);

// Column where the function summary starts ("round(x, n)" = 11 chars).
constexpr int kSummaryCol = 13;

void draw_text_line(gfx::Framebuffer& fb, const gfx::Font& font, int y, const char* text) {
    using namespace platform::colors;
    if (text[0] == '#') {
        font.draw_string(fb, 4, y, text + 1, kGreen);
    } else {
        font.draw_string(fb, 12, y, text, kWhite);
    }
}

}  // namespace

int HelpScreen::line_count() const {
    if (tab_ == 0) {
        int n = 0;
        math::catalog(&n);
        return n;
    }
    return tab_ == 1 ? kKeysCount : kSyntaxCount;
}

int HelpScreen::max_scroll() const {
    const int extra = line_count() - kVisibleLines;
    return extra > 0 ? extra : 0;
}

bool HelpScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kLeft:
            tab_ = (tab_ + kTabCount - 1) % kTabCount;
            scroll_ = 0;
            return true;
        case Key::kRight:
            tab_ = (tab_ + 1) % kTabCount;
            scroll_ = 0;
            return true;
        case Key::kUp:
            if (scroll_ > 0) {
                --scroll_;
            }
            return true;
        case Key::kDown:
            if (scroll_ < max_scroll()) {
                ++scroll_;
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void HelpScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);

    // Title bar with tabs; the active one is highlighted.
    fb.fill_rect(0, 0, platform::kScreenW, 16, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 4, 2, "HELP", kGrayLine);
    int tx = 4 + 6 * font.width();
    for (int t = 0; t < kTabCount; ++t) {
        const int w = font.text_width(kTabNames[t]);
        if (t == tab_) {
            fb.fill_rect(tx - 2, 0, w + 4, 16, platform::Color::from_rgb(0, 0, 90));
        }
        font.draw_string(fb, tx, 2, kTabNames[t], t == tab_ ? kWhite : kGrayLine);
        tx += w + 12;
    }

    const int count = line_count();
    for (int row = 0; row < kVisibleLines; ++row) {
        const int i = scroll_ + row;
        if (i >= count) {
            break;
        }
        const int y = kTopY + row * kLineH;
        if (tab_ == 0) {
            int n = 0;
            const math::FnDescriptor* cat = math::catalog(&n);
            font.draw_string(fb, 4, y, cat[i].signature, kGreen);
            font.draw_string(fb, 4 + kSummaryCol * font.width(), y, cat[i].summary, kWhite);
        } else {
            draw_text_line(fb, font, y, tab_ == 1 ? kKeysLines[i] : kSyntaxLines[i]);
        }
    }

    // Scroll indicator when content overflows.
    if (max_scroll() > 0) {
        char pos[16];
        std::snprintf(pos, sizeof(pos), "%d/%d", scroll_ + 1, max_scroll() + 1);
        font.draw_string(fb, platform::kScreenW - 4 - font.text_width(pos), kTopY - 14, pos,
                         kGrayLine);
    }

    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, "LT/RT:TAB  UP/DN:SCROLL  ESC:BACK", kGrayLine);
}

HelpScreen& help_screen() {
    static HelpScreen instance;
    return instance;
}

}  // namespace apps
