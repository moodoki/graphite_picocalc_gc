#include "apps/cas_menu.hpp"

#include <cstdio>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "apps/home_screen.hpp"

namespace apps {

namespace {

constexpr int kTopY = 44;
constexpr int kRowH = 28;
constexpr int kSoftkeyY = 300;

struct CasOp {
    const char* label;   // menu row text
    const char* opener;  // text inserted into the input line
};

// The six CAS operations (spec §10). Selecting one inserts its call opener;
// the inline-CAS path (4D.21) evaluates it on Enter.
constexpr CasOp kOps[] = {
    {"Simplify", "simplify("}, {"Expand", "expand("},  {"Factor", "factor("},
    {"d/dx", "diff("},         {"Integral", "integ("}, {"Solve", "solve("},
};
constexpr int kOpCount = static_cast<int>(sizeof(kOps) / sizeof(kOps[0]));

}  // namespace

void CasMenuScreen::on_activate() {
    // Keep sel_ across visits (repeat ops are common); nothing to reset.
}

void CasMenuScreen::select(int i) {
    ui::screen_manager().pop();
    home_screen().insert_text(kOps[i].opener);
}

bool CasMenuScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kUp:
            sel_ = (sel_ + kOpCount - 1) % kOpCount;
            invalidate_all();
            return true;
        case Key::kDown:
            sel_ = (sel_ + 1) % kOpCount;
            invalidate_all();
            return true;
        case Key::kEnter:
            select(sel_);
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            if (ev.ch >= '1' && ev.ch <= '0' + kOpCount) {
                select(ev.ch - '1');
                return true;
            }
            return false;
    }
}

void CasMenuScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "CAS");

    for (int i = 0; i < kOpCount; ++i) {
        const int y = kTopY + i * kRowH;
        if (i == sel_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH - 4,
                         platform::Color::from_rgb(0, 0, 60));
        }
        char line[32];
        std::snprintf(line, sizeof(line), "%d: %s", i + 1, kOps[i].label);
        font.draw_string(fb, 24, y, line, i == sel_ ? kWhite : kGrayLine);
    }

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "1-6/ENTER:INSERT ESC:BACK", kGrayLine);
}

CasMenuScreen& cas_menu() {
    static CasMenuScreen instance;
    return instance;
}

}  // namespace apps
