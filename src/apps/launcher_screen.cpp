#include "apps/launcher_screen.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "platform/app_registry.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"

namespace apps {

namespace {

constexpr int kTopY = 44;
constexpr int kRowH = 28;
constexpr int kListBottom = platform::kScreenH - ui::kSoftkeyBarH - 4;

int visible_rows() {
    return (kListBottom - kTopY) / kRowH;
}

}  // namespace

void LauncherScreen::on_activate() {
    // The SD tier can appear or vanish between visits (§4.5), so clamp
    // rather than assuming last visit's selection is still in range.
    const int n = platform::AppRegistry::count();
    if (selected_ >= n) {
        selected_ = n > 0 ? n - 1 : 0;
    }
    scroll_into_view();
}

void LauncherScreen::scroll_into_view() {
    const int rows = visible_rows();
    if (selected_ < top_) {
        top_ = selected_;
    } else if (selected_ >= top_ + rows) {
        top_ = std::max(selected_ - rows + 1, 0);
    }
}

void LauncherScreen::launch(int index) {
    const platform::AppEntry* entry = platform::AppRegistry::get(index);
    if (entry == nullptr || entry->launch == nullptr) {
        return;
    }
    // Stay on the stack: the app pushes on top of us, so its ESC lands
    // back here (§3.3). Do NOT pop first, the way the CAS/CALC menus do.
    entry->launch(*entry);
}

bool LauncherScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    const int n = platform::AppRegistry::count();
    switch (ev.key) {
        case Key::kUp:
            if (n > 0) {
                selected_ = (selected_ + n - 1) % n;
                scroll_into_view();
                invalidate_all();
            }
            return true;
        case Key::kDown:
            if (n > 0) {
                selected_ = (selected_ + 1) % n;
                scroll_into_view();
                invalidate_all();
            }
            return true;
        case Key::kEnter:
            if (n > 0) {
                launch(selected_);
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            // Digit accelerator, matching the CAS/CALC menus. Only for
            // the first nine rows — past that the arrows are the way.
            if (ev.ch >= '1' && ev.ch <= '9' && (ev.ch - '1') < n) {
                launch(ev.ch - '1');
                return true;
            }
            return false;
    }
}

void LauncherScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "APPS");

    const int n = platform::AppRegistry::count();
    if (n == 0) {
        font.draw_string(fb, 24, kTopY, "No apps registered", kGrayLine);
    }

    const int rows = visible_rows();
    for (int row = 0; row < rows; ++row) {
        const int i = top_ + row;
        if (i >= n) {
            break;
        }
        const platform::AppEntry* entry = platform::AppRegistry::get(i);
        if (entry == nullptr) {
            break;
        }
        const int y = kTopY + row * kRowH;
        if (i == selected_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH - 4,
                         platform::Color::from_rgb(0, 0, 60));
        }
        char line[40];
        if (i < 9) {
            std::snprintf(line, sizeof(line), "%d: %s", i + 1, entry->name);
        } else {
            std::snprintf(line, sizeof(line), "   %s", entry->name);
        }
        font.draw_string(fb, 24, y, line, i == selected_ ? kWhite : kGrayLine);
    }

    // Scroll position, right-aligned, only when the list overflows.
    if (n > rows) {
        char pos[16];
        std::snprintf(pos, sizeof(pos), "%d/%d", selected_ + 1, n);
        const int w = static_cast<int>(std::strlen(pos)) * font.width();
        font.draw_string(fb, platform::kScreenW - w - 4, kTopY - 20, pos, kGrayLine);
    }

    const char* const keys[6] = {"", "", "", "", "", ""};
    ui::draw_softkeys(fb, keys);
    font.draw_string(fb, 2, platform::kScreenH - ui::kSoftkeyBarH + 4, "ENTER:OPEN  ESC:BACK",
                     kGrayLine);
}

LauncherScreen& launcher_screen() {
    static LauncherScreen instance;
    return instance;
}

}  // namespace apps
