#include "apps/settings_screen.hpp"

#include <cstdint>
#include <cstdio>

#include "platform/power.hpp"
#include "platform/storage.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"

namespace apps {

namespace {

constexpr int kTopY = 48;
constexpr int kRowH = 36;
constexpr int kNumRows = 3;

constexpr int kRowLcd = 0;
constexpr int kRowKbd = 1;
constexpr int kRowApd = 2;

// Auto-power-down choices in minutes (0 = never).
constexpr uint16_t kApdSteps[] = {0, 1, 2, 5, 10, 30};
constexpr int kApdCount = sizeof(kApdSteps) / sizeof(kApdSteps[0]);

int apd_index(uint16_t minutes) {
    for (int i = 0; i < kApdCount; ++i) {
        if (kApdSteps[i] == minutes) {
            return i;
        }
    }
    return 3;  // Unknown persisted value: treat as the 5 min default
}

// Backlights step in 16ths of full scale (0, 16, ..., 240, 255).
uint8_t level_step(uint8_t level, int dir) {
    int v = (level + 8) / 16 + dir;
    v = v < 0 ? 0 : (v > 16 ? 16 : v);
    return static_cast<uint8_t>(v == 16 ? 255 : v * 16);
}

}  // namespace

void SettingsScreen::on_activate() {
    invalidate_all();
}

void SettingsScreen::adjust(int dir) {
    auto& s = platform::power::settings();
    switch (selected_) {
        case kRowLcd:
            s.lcd_level = level_step(s.lcd_level, dir);
            break;
        case kRowKbd:
            s.kbd_level = level_step(s.kbd_level, dir);
            break;
        case kRowApd: {
            int i = apd_index(s.apd_minutes) + dir;
            i = (i % kApdCount + kApdCount) % kApdCount;
            s.apd_minutes = kApdSteps[i];
            break;
        }
        default:
            break;
    }
    platform::power::request_apply();
    platform::power::save(platform::storage());
    invalidate_all();
}

bool SettingsScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kUp:
            if (selected_ > 0) {
                --selected_;
                invalidate_all();
            }
            return true;
        case Key::kDown:
            if (selected_ < kNumRows - 1) {
                ++selected_;
                invalidate_all();
            }
            return true;
        case Key::kLeft:
            adjust(-1);
            return true;
        case Key::kRight:
        case Key::kEnter:
            adjust(+1);
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void SettingsScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "SETTINGS");

    const auto& s = platform::power::settings();
    const char* const labels[kNumRows] = {"LCD brightness", "Kbd backlight", "Auto power-down"};
    char values[kNumRows][16];
    std::snprintf(values[kRowLcd], sizeof(values[0]), "%d%%", s.lcd_level * 100 / 255);
    std::snprintf(values[kRowKbd], sizeof(values[0]), "%d%%", s.kbd_level * 100 / 255);
    if (s.apd_minutes == 0) {
        std::snprintf(values[kRowApd], sizeof(values[0]), "OFF");
    } else {
        std::snprintf(values[kRowApd], sizeof(values[0]), "%u min", s.apd_minutes);
    }

    for (int i = 0; i < kNumRows; ++i) {
        const int y = kTopY + i * kRowH;
        if (i == selected_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        font.draw_string(fb, 12, y, labels[i], kWhite);
        font.draw_string(fb, platform::kScreenW - 12 - font.text_width(values[i]), y, values[i],
                         kGreen);
    }

    font.draw_string(fb, 12, kTopY + kNumRows * kRowH + 8, "LEFT/RIGHT change   ESC back",
                     kGrayLine);
    font.draw_string(fb, 12, kTopY + kNumRows * kRowH + 28, "Auto power-down dims the screen;",
                     kGridLine);
    font.draw_string(fb, 12, kTopY + kNumRows * kRowH + 44, "any key wakes it.", kGridLine);

    const char* const keys[6] = {"", "", "", "", "", ""};
    ui::draw_softkeys(fb, keys);
}

SettingsScreen& settings_screen() {
    static SettingsScreen instance;
    return instance;
}

}  // namespace apps
