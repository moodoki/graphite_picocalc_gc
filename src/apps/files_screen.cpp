#include "apps/files_screen.hpp"

#include <algorithm>
#include <cstdio>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"

namespace apps {

namespace {
constexpr int kTopY = 24;
constexpr int kRowH = 16;
constexpr const char* kDir = "/picocalc";

int visible_rows() {
    return (platform::kScreenH - kTopY - ui::kSoftkeyBarH - 4) / kRowH;
}
}  // namespace

void FilesScreen::on_activate() {
    count_ = platform::storage().mounted()
                 ? platform::storage().list_dir(kDir, entries_, kMaxEntries)
                 : -1;
    scroll_ = 0;
    selected_ = 0;
}

bool FilesScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kUp:
            if (selected_ > 0) {
                --selected_;
                scroll_ = std::min(scroll_, selected_);
            }
            return true;
        case Key::kDown:
            if (selected_ < count_ - 1) {
                ++selected_;
                scroll_ = std::max(scroll_, selected_ - visible_rows() + 1);
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void FilesScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "FILES /picocalc");

    if (count_ < 0) {
        font.draw_string(fb, 8, kTopY, platform::storage().mounted() ? "read error" : "no SD card",
                         kRed);
    } else if (count_ == 0) {
        font.draw_string(fb, 8, kTopY, "(empty)", kGrayLine);
    }

    char line[24];
    const int first = scroll_ > 0 ? scroll_ : 0;
    for (int i = first; i < count_ && i - first < visible_rows(); ++i) {
        const int y = kTopY + (i - first) * kRowH;
        if (i == selected_) {
            fb.fill_rect(0, y - 1, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        font.draw_string(fb, 8, y, entries_[i].name, kWhite);
        if (entries_[i].is_dir) {
            std::snprintf(line, sizeof(line), "[DIR]");
        } else {
            std::snprintf(line, sizeof(line), "%lu B",
                          static_cast<unsigned long>(entries_[i].size));
        }
        font.draw_string(fb, platform::kScreenW - font.text_width(line) - 8, y, line, kGrayLine);
    }

    if (count_ > visible_rows()) {
        std::snprintf(line, sizeof(line), "%d/%d", selected_ + 1, count_);
        font.draw_string(fb, platform::kScreenW - font.text_width(line) - 8, 2, line, kGrayLine);
    }

    const char* const keys[6] = {"", "", "", "", "", ""};
    ui::draw_softkeys(fb, keys);
}

FilesScreen& files_screen() {
    static FilesScreen instance;
    return instance;
}

}  // namespace apps
