#include "apps/table_setup.hpp"

#include <cstdio>
#include <cstdlib>

#include "gfx/font.hpp"
#include "ui/screen_manager.hpp"
#include "math/format.hpp"
#include "apps/graph_model.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {
constexpr int kTopY = 40;
constexpr int kRowH = 36;

constexpr int kRowStart = 0;
constexpr int kRowStep = 1;
constexpr int kRowAsk = 2;

double* row_value(int row) {
    auto& t = graph::state().table;
    return row == kRowStart ? &t.start : &t.step;
}
}  // namespace

void TableSetupScreen::on_activate() {
    editing_ = false;
}

void TableSetupScreen::begin_edit() {
    if (selected_ == kRowAsk) {
        return;  // Toggle row, not a numeric field.
    }
    char buf[24];
    math::format_number(*row_value(selected_), buf, sizeof(buf));
    input_.set_text(buf);
    editing_ = true;
}

void TableSetupScreen::commit_edit() {
    *row_value(selected_) = std::strtod(input_.text(), nullptr);
    editing_ = false;
    save_graph_state();
}

bool TableSetupScreen::on_key(const platform::KeyEvent& ev) {
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
            if (selected_ < kNumRows - 1) {
                ++selected_;
            }
            return true;
        case Key::kLeft:
        case Key::kRight:
            if (selected_ == kRowAsk) {
                graph::state().table.ask_mode = !graph::state().table.ask_mode;
                save_graph_state();
            }
            return true;
        case Key::kEnter:
        case Key::kF1:
            if (selected_ == kRowAsk) {
                graph::state().table.ask_mode = !graph::state().table.ask_mode;
                save_graph_state();
            } else {
                begin_edit();
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void TableSetupScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();
    const auto& t = graph::state().table;

    fb.clear(kBlack);
    fb.fill_rect(0, 0, platform::kScreenW, 16, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 4, 2, "TABLE SETUP", kGrayLine);

    const char* const names[kNumRows] = {"Start", "Step", "Independent"};
    for (int i = 0; i < kNumRows; ++i) {
        const int y = kTopY + i * kRowH;
        if (i == selected_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        font.draw_string(fb, 12, y, names[i], kGreen);

        const int vx = platform::kScreenW / 2;
        if (i == kRowAsk) {
            font.draw_string(fb, vx, y, t.ask_mode ? "ASK" : "AUTO", kWhite);
        } else if (i == selected_ && editing_) {
            input_.render(fb, vx, y, platform::kScreenW - vx - 8, font, true);
        } else {
            char buf[24];
            math::format_number(*row_value(i), buf, sizeof(buf));
            font.draw_string(fb, vx, y, buf, kWhite);
        }
    }

    font.draw_string(fb, 12, kTopY + kNumRows * kRowH + 8, "ENTER edit/toggle   ESC back to table",
                     kGrayLine);

    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, "F1:EDIT  ESC:BACK", kGrayLine);
}

TableSetupScreen& table_setup_screen() {
    static TableSetupScreen instance;
    return instance;
}

}  // namespace apps
