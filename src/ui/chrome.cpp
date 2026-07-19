#include "ui/chrome.hpp"

#include <cstdio>
#include <cstring>

#include "platform/system.hpp"
#include "gfx/font.hpp"
#include "math/format.hpp"
#include "math/types.hpp"

namespace ui {

namespace {
const platform::Color kBarBg = platform::Color::from_rgb(30, 30, 30);
const platform::Color kBattOk = platform::colors::kGreen;
const platform::Color kBattLow = platform::Color::from_rgb(255, 180, 0);
const platform::Color kBattCrit = platform::colors::kRed;
const platform::Color kBattChg = platform::Color::from_rgb(0, 200, 220);

// Battery icon (body + tip) with a level-colored fill and percent text.
// Returns the leftmost x used, so other indicators can right-align to it.
int draw_battery(gfx::Framebuffer& fb, const gfx::Font& font) {
    using namespace platform::colors;
    const auto batt = platform::battery_status();

    platform::Color col = kBattOk;
    if (batt.charging) {
        col = kBattChg;
    } else if (batt.percent >= 0 && batt.percent <= 20) {
        col = kBattCrit;
    } else if (batt.percent >= 0 && batt.percent <= 50) {
        col = kBattLow;
    }

    char txt[8];
    if (batt.percent >= 0) {
        std::snprintf(txt, sizeof(txt), "%d%%", batt.percent);
    } else {
        std::snprintf(txt, sizeof(txt), "--");
    }
    const int tx = platform::kScreenW - font.text_width(txt) - 4;
    font.draw_string(fb, tx, 2, txt, col);

    // Icon: 14x8 body + 2x4 tip, vertically centered in the bar.
    const int ix = tx - 20;
    const int iy = (kStatusBarH - 8) / 2;
    fb.draw_rect(ix, iy, 14, 8, kGrayLine);
    fb.fill_rect(ix + 14, iy + 2, 2, 4, kGrayLine);
    if (batt.percent > 0) {
        const int fill = (batt.percent * 10 + 50) / 100;  // 0..10 px
        fb.fill_rect(ix + 2, iy + 2, fill, 4, col);
    }
    return ix;
}

// D26 storage-health state (healthy until the main loop says otherwise).
bool g_sd_ok = true;
bool g_psram_ok = true;

}  // namespace

void set_health_flags(bool sd_ok, bool psram_ok) {
    g_sd_ok = sd_ok;
    g_psram_ok = psram_ok;
}

void draw_status_bar(gfx::Framebuffer& fb, const char* title, StatusFlags flags) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.fill_rect(0, 0, platform::kScreenW, kStatusBarH, kBarBg);
    font.draw_string(fb, 4, 2, title, kGrayLine);

    // D26: red subsystem indicators while SD/PSRAM are unhealthy
    // (retries keep running; these clear on recovery).
    int hx = 4 + font.text_width(title) + 10;
    if (!g_sd_ok) {
        font.draw_string(fb, hx, 2, "SD", kRed);
        hx += font.text_width("SD") + 8;
    }
    if (!g_psram_ok) {
        font.draw_string(fb, hx, 2, "PSRAM", kRed);
    }

    // Battery, rightmost; other indicators right-align to its left edge.
    const int batt_x = draw_battery(fb, font);

    // Right-aligned indicators: [2nd] [A] RAD/DEG FLOAT/FIX/SCI
    char right[32];
    const char* angle = math::angle_mode() == math::AngleMode::kRadians ? "RAD" : "DEG";
    const char* disp = math::display_mode() == math::DisplayMode::kFix   ? "FIX"
                       : math::display_mode() == math::DisplayMode::kSci ? "SCI"
                                                                         : "FLT";
    std::snprintf(right, sizeof(right), "%s%s%s %s", flags.second ? "2nd " : "",
                  flags.alpha ? "A " : "", angle, disp);
    const int rx = batt_x - 6 - font.text_width(right);
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
