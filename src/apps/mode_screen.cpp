#include "apps/mode_screen.hpp"

#include <cstdio>

#include "pico/bootrom.h"

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/format.hpp"
#include "math/types.hpp"
#include "apps/graph_model.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {
constexpr int kTopY = 40;
constexpr int kRowH = 36;

// Row indices
constexpr int kRowAngle = 0;
constexpr int kRowDisplay = 1;
constexpr int kRowFixDigits = 2;
constexpr int kRowGraphMode = 3;
constexpr int kRowReboot = 4;
}  // namespace

void ModeScreen::adjust(int dir) const {
    switch (selected_) {
        case kRowAngle:
            math::set_angle_mode(math::angle_mode() == math::AngleMode::kRadians
                                     ? math::AngleMode::kDegrees
                                     : math::AngleMode::kRadians);
            break;
        case kRowDisplay: {
            int m = static_cast<int>(math::display_mode()) + dir;
            const int count = 3;
            m = (m % count + count) % count;
            math::set_display_mode(static_cast<math::DisplayMode>(m));
            break;
        }
        case kRowFixDigits:
            math::set_fix_digits(math::fix_digits() + dir);
            break;
        case kRowGraphMode: {
            constexpr int kModeCount = 3;  // Function, Parametric, Polar
            int m = static_cast<int>(graph::state().mode) + dir;
            m = (m % kModeCount + kModeCount) % kModeCount;
            graph::state().mode = static_cast<graph::Mode>(m);
            save_graph_state();
            break;
        }
        case kRowReboot:
            if (dir != 0) {  // Only on ENTER/select, handled in on_key
            }
            break;
        default:
            break;
    }
}

bool ModeScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kUp:
            if (selected_ > 0) {
                --selected_;
            }
            return true;
        case Key::kDown:
            if (selected_ < kNumRows - 1) {
                ++selected_;
            }
            return true;
        case Key::kLeft:
            adjust(-1);
            return true;
        case Key::kRight:
            adjust(+1);
            return true;
        case Key::kEnter:
            if (selected_ == kRowReboot) {
                // Reboot into the RP2 USB bootloader (BOOTSEL) so a new
                // UF2 can be dropped without touching the board button.
                reset_usb_boot(0, 0);
            } else {
                adjust(+1);
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void ModeScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "MODE");

    const char* const rows[kNumRows][2] = {
        {"Angle", nullptr},
        {"Display", nullptr},
        {"Fix digits", nullptr},
        {"Graph mode", nullptr},
        {"Reboot to bootloader", nullptr},
    };

    char angle_val[8];
    std::snprintf(angle_val, sizeof(angle_val), "%s",
                  math::angle_mode() == math::AngleMode::kRadians ? "RADIAN" : "DEGREE");
    char disp_val[8];
    std::snprintf(disp_val, sizeof(disp_val), "%s",
                  math::display_mode() == math::DisplayMode::kFix   ? "FIX"
                  : math::display_mode() == math::DisplayMode::kSci ? "SCI"
                                                                    : "FLOAT");
    char fix_val[8];
    std::snprintf(fix_val, sizeof(fix_val), "%d", math::fix_digits());
    const char* gmode_val = "FUNC";
    if (graph::state().mode == graph::Mode::kParametric) {
        gmode_val = "PARAM";
    } else if (graph::state().mode == graph::Mode::kPolar) {
        gmode_val = "POLAR";
    }

    const char* const values[kNumRows] = {angle_val, disp_val, fix_val, gmode_val, "[ENTER]"};

    for (int i = 0; i < kNumRows; ++i) {
        const int y = kTopY + i * kRowH;
        if (i == selected_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        font.draw_string(fb, 12, y, rows[i][0], kWhite);
        font.draw_string(fb, platform::kScreenW - 12 - font.text_width(values[i]), y, values[i],
                         kGreen);
    }

    font.draw_string(fb, 12, kTopY + kNumRows * kRowH + 8, "LEFT/RIGHT change   ESC back",
                     kGrayLine);

    const char* const keys[6] = {"", "", "", "", "", ""};
    ui::draw_softkeys(fb, keys);
}

ModeScreen& mode_screen() {
    static ModeScreen instance;
    return instance;
}

}  // namespace apps
