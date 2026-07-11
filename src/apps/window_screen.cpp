#include "apps/window_screen.hpp"

#include <cstdio>
#include <cstdlib>

#include "gfx/font.hpp"
#include "ui/screen_manager.hpp"
#include "math/format.hpp"
#include "apps/graph_model.hpp"
#include "apps/graph_screen.hpp"

namespace apps {

namespace {
constexpr int kTopY = 32;
constexpr int kRowH = 32;
}  // namespace

double* WindowScreen::field_ptr(int i) const {
    auto& w = graph_window();
    switch (i) {
        case 0:
            return &w.x_min;
        case 1:
            return &w.x_max;
        case 2:
            return &w.y_min;
        case 3:
            return &w.y_max;
        case 4:
            return &w.x_scl;
        default:
            return &w.y_scl;
    }
}

const char* WindowScreen::field_name(int i) {
    static constexpr const char* kNames[kNumFields] = {"Xmin", "Xmax", "Ymin",
                                                       "Ymax", "Xscl", "Yscl"};
    return kNames[i];
}

void WindowScreen::on_activate() {
    editing_ = false;
}

void WindowScreen::begin_edit() {
    char buf[24];
    math::format_number(*field_ptr(selected_), buf, sizeof(buf));
    input_.set_text(buf);
    editing_ = true;
}

void WindowScreen::commit_edit() {
    *field_ptr(selected_) = std::strtod(input_.text(), nullptr);
    editing_ = false;
    save_window();
}

bool WindowScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    if (editing_) {
        if (ev.key == Key::kEnter) {
            commit_edit();
            return true;
        }
        if (ev.key == Key::kEscape) {
            editing_ = false;
            return true;
        }
        return input_.on_key(ev);
    }

    switch (ev.key) {
        case Key::kUp:
            if (selected_ > 0) {
                --selected_;
            }
            return true;
        case Key::kDown:
            if (selected_ < kNumFields - 1) {
                ++selected_;
            }
            return true;
        case Key::kEnter:
        case Key::kF1:
            begin_edit();
            return true;
        case Key::kEscape:
            graph_screen().invalidate();
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void WindowScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    fb.fill_rect(0, 0, platform::kScreenW, 16, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 4, 2, "WINDOW SETTINGS", kGrayLine);

    for (int i = 0; i < kNumFields; ++i) {
        const int y = kTopY + i * kRowH;
        const bool sel = (i == selected_);
        if (sel) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        font.draw_string(fb, 8, y, field_name(i), kGreen);
        font.draw_char(fb, 8 + 5 * font.width(), y, '=', kWhite);

        const int vx = 8 + 7 * font.width();
        if (sel && editing_) {
            input_.render(fb, vx, y, platform::kScreenW - vx - 8, font, true);
        } else {
            char buf[24];
            math::format_number(*field_ptr(i), buf, sizeof(buf));
            font.draw_string(fb, vx, y, buf, kWhite);
        }
    }

    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, "F1/ENTER:EDIT  ESC:BACK TO GRAPH", kGrayLine);
}

WindowScreen& window_screen() {
    static WindowScreen instance;
    return instance;
}

}  // namespace apps
