#include "ui/chrome.hpp"

#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "math/format.hpp"
#include "math/types.hpp"

namespace ui {

namespace {
const platform::Color kBarBg = platform::Color::from_rgb(30, 30, 30);
}  // namespace

void draw_status_bar(gfx::Framebuffer& fb, const char* title, StatusFlags flags) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.fill_rect(0, 0, platform::kScreenW, kStatusBarH, kBarBg);
    font.draw_string(fb, 4, 2, title, kGrayLine);

    // Right-aligned indicators: [2nd] [A] RAD/DEG FLOAT/FIX/SCI
    char right[32];
    const char* angle = math::angle_mode() == math::AngleMode::kRadians ? "RAD" : "DEG";
    const char* disp = math::display_mode() == math::DisplayMode::kFix   ? "FIX"
                       : math::display_mode() == math::DisplayMode::kSci ? "SCI"
                                                                         : "FLT";
    std::snprintf(right, sizeof(right), "%s%s%s %s", flags.second ? "2nd " : "",
                  flags.alpha ? "A " : "", angle, disp);
    const int rx = platform::kScreenW - font.text_width(right) - 4;
    font.draw_string(fb, rx, 2, right, kGreen);
}

void draw_softkeys(gfx::Framebuffer& fb, const char* const labels[6]) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    const int y = platform::kScreenH - kSoftkeyBarH;
    fb.fill_rect(0, y, platform::kScreenW, kSoftkeyBarH, kBarBg);

    const int cell = platform::kScreenW / 6;
    for (int i = 0; i < 6; ++i) {
        if (labels[i] == nullptr || labels[i][0] == 0) {
            continue;
        }
        char cell_text[16];
        std::snprintf(cell_text, sizeof(cell_text), "%d:%s", i + 1, labels[i]);
        // Truncate to fit the cell.
        const int max_chars = (cell - 2) / font.width();
        if (static_cast<int>(std::strlen(cell_text)) > max_chars && max_chars > 0) {
            cell_text[max_chars] = 0;
        }
        font.draw_string(fb, i * cell + 2, y + 4, cell_text, kGrayLine);
        if (i > 0) {
            fb.draw_vline(i * cell, y, kSoftkeyBarH, kBlack);
        }
    }
}

}  // namespace ui
