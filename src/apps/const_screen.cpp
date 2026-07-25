#include "apps/const_screen.hpp"

#include <cstdio>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/catalog.hpp"
#include "math/format.hpp"
#include "apps/home_screen.hpp"

namespace apps {

namespace {
constexpr int kTopY = 24;
constexpr int kRowH = 18;
constexpr int kVisibleRows = 15;
}  // namespace

void ConstScreen::on_activate() {
    invalidate_all();
}

bool ConstScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    int count = 0;
    const auto* cs = math::constants(&count);
    switch (ev.key) {
        case Key::kUp:
            if (selected_ > 0) {
                --selected_;
                top_ = selected_ < top_ ? selected_ : top_;
                invalidate_all();
            }
            return true;
        case Key::kDown:
            if (selected_ + 1 < count) {
                ++selected_;
                if (selected_ - top_ >= kVisibleRows) {
                    top_ = selected_ - kVisibleRows + 1;
                }
                invalidate_all();
            }
            return true;
        case Key::kEnter:
            home_screen().insert_text(cs[selected_].name);
            ui::screen_manager().pop();
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void ConstScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "CONSTANTS");

    int count = 0;
    const auto* cs = math::constants(&count);
    for (int r = 0; r < kVisibleRows; ++r) {
        const int i = top_ + r;
        if (i >= count) {
            break;
        }
        const int y = kTopY + r * kRowH;
        if (i == selected_) {
            fb.fill_rect(0, y - 2, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        font.draw_string(fb, 4, y, cs[i].name, kGreen);
        char val[24];
        math::format_number(cs[i].value, val, sizeof(val));
        font.draw_string(fb, 90, y, val, kWhite);
        font.draw_string(fb, platform::kScreenW - 4 - font.text_width(cs[i].summary), y,
                         cs[i].summary, kGrayLine);
    }

    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, "ENTER:INSERT ESC:BACK", kGrayLine);
}

ConstScreen& const_screen() {
    static ConstScreen instance;
    return instance;
}

}  // namespace apps
