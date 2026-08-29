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

// A switch rather than a ternary chain: DisplayMode gained kEng and the
// old chain's default silently labelled it "FLT" (issue #36) while the
// MODE screen showed ENG correctly, so the two disagreed on screen.
// -Wswitch now catches the next value added to the enum instead of
// mislabelling it.
const char* display_mode_label() {
    switch (math::display_mode()) {
        case math::DisplayMode::kFloat:
            return "FLT";
        case math::DisplayMode::kFix:
            return "FIX";
        case math::DisplayMode::kSci:
            return "SCI";
        case math::DisplayMode::kEng:
            return "ENG";
    }
    return "FLT";  // unreachable for a valid enum value
}

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

void draw_status_bar(gfx::Framebuffer& fb, const char* title) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.fill_rect(0, 0, platform::kScreenW, kStatusBarH, kBarBg);

    // Right-hand side FIRST, because it is what the title has to fit
    // inside. It used to be drawn last and simply painted over a long
    // title (#61), which made the failure silent: the bar looked fine
    // until a runtime title -- a file manager path, an SD app's name --
    // happened to be long enough.
    const int batt_x = draw_battery(fb, font);

    // Right-aligned indicators: RAD/DEG and FLT/FIX/SCI/ENG.
    char right[16];
    const char* angle = math::angle_mode() == math::AngleMode::kRadians ? "RAD" : "DEG";
    std::snprintf(right, sizeof(right), "%s %s", angle, display_mode_label());
    const int rx = batt_x - 6 - font.text_width(right);
    font.draw_string(fb, rx, 2, right, kGreen);

    // D26: red subsystem indicators while SD/PSRAM are unhealthy
    // (retries keep running; these clear on recovery).
    //
    // Measured before the title and subtracted from its budget, because
    // between the two the health state is the one that must not be lost.
    // A truncated title costs the user a few characters they can usually
    // infer; a missing SD indicator costs them the reason their files
    // stopped saving.
    char health[16] = {0};
    if (!g_sd_ok && !g_psram_ok) {
        std::snprintf(health, sizeof(health), "SD PSRAM");
    } else if (!g_sd_ok) {
        std::snprintf(health, sizeof(health), "SD");
    } else if (!g_psram_ok) {
        std::snprintf(health, sizeof(health), "PSRAM");
    }
    const int health_w = health[0] != 0 ? font.text_width(health) + 10 : 0;

    // What is left for the title, with 6 px of air before the right block
    // so a full-width title does not touch it.
    char shown[40];
    const int title_budget = rx - 6 - health_w - 4;
    gfx::fit_text(font, title, title_budget, shown, sizeof(shown));
    font.draw_string(fb, 4, 2, shown, kGrayLine);

    if (health[0] != 0) {
        font.draw_string(fb, 4 + font.text_width(shown) + 10, 2, health, kRed);
    }
}

void draw_softkeys(gfx::Framebuffer& fb, const char* const labels[6]) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    const int y = platform::kScreenH - kSoftkeyBarH;
    fb.fill_rect(0, y, platform::kScreenW, kSoftkeyBarH, kBarBg);

    const int cell = platform::kScreenW / 6;
    for (int i = 0; i < 6; ++i) {
        // Divider first, and outside the empty-label check. It used to sit
        // below the `continue`, so a cell with no label lost its left edge
        // and the bar's grid changed shape with its contents (#65) -- the
        // file manager's CUT and REN visibly merged into one wide cell
        // whenever no cut was armed to put MOVE between them.
        if (i > 0) {
            fb.draw_vline(i * cell, y, kSoftkeyBarH, kBlack);
        }
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
    }
}

}  // namespace ui
