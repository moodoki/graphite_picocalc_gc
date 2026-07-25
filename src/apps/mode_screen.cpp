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
constexpr int kRowNumber = 4;
constexpr int kRowSeqPlot = 5;  // Sequence-mode plot style (4D.7)
constexpr int kRowReboot = 6;
}  // namespace

void ModeScreen::adjust(int dir) const {
    switch (selected_) {
        case kRowAngle:
            math::set_angle_mode(math::angle_mode() == math::AngleMode::kRadians
                                     ? math::AngleMode::kDegrees
                                     : math::AngleMode::kRadians);
            graph::state().angle = math::angle_mode();
            save_graph_state();
            break;
        case kRowDisplay: {
            int m = static_cast<int>(math::display_mode()) + dir;
            const int count = 3;
            m = (m % count + count) % count;
            math::set_display_mode(static_cast<math::DisplayMode>(m));
            graph::state().display = math::display_mode();
            save_graph_state();
            break;
        }
        case kRowFixDigits:
            math::set_fix_digits(math::fix_digits() + dir);
            graph::state().fix_digits = math::fix_digits();
            save_graph_state();
            break;
        case kRowGraphMode: {
            constexpr int kModeCount = 4;  // Function, Parametric, Polar, Seq
            int m = static_cast<int>(graph::state().mode) + dir;
            m = (m % kModeCount + kModeCount) % kModeCount;
            graph::state().mode = static_cast<graph::Mode>(m);
            save_graph_state();
            break;
        }
        case kRowSeqPlot:
            graph::state().seq_style = graph::state().seq_style == 0 ? 1 : 0;
            save_graph_state();
            break;
        case kRowNumber: {
            constexpr int kCount = 3;  // REAL, RECTANGULAR, POLAR
            int m = static_cast<int>(math::number_mode()) + dir;
            m = (m % kCount + kCount) % kCount;
            math::set_number_mode(static_cast<math::NumberMode>(m));
            graph::state().number = math::number_mode();
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
        {"Number", nullptr},
        {"Seq plot", nullptr},
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
    } else if (graph::state().mode == graph::Mode::kSeq) {
        gmode_val = "SEQ";
    }
    const char* seq_plot_val = graph::state().seq_style == 1 ? "WEB" : "TIME";
    char number_buf[8];
    const char* number_val = "REAL";
    if (math::number_mode() == math::NumberMode::kRectangular) {
        // "a+bi" with the real imaginary-unit glyph.
        std::snprintf(number_buf, sizeof(number_buf), "a+b%c", gfx::kGlyphImagI);
        number_val = number_buf;
    } else if (math::number_mode() == math::NumberMode::kPolar) {
        // "r∠θ" with the real angle and theta glyphs (was ASCII "r<t").
        std::snprintf(number_buf, sizeof(number_buf), "r%c%c", gfx::kGlyphAngle, gfx::kGlyphTheta);
        number_val = number_buf;
    }

    const char* const values[kNumRows] = {angle_val,  disp_val,     fix_val,  gmode_val,
                                          number_val, seq_plot_val, "[ENTER]"};

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
