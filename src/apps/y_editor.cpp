#include "apps/y_editor.hpp"

#include <cstdio>

#include "gfx/font.hpp"
#include "ui/screen_manager.hpp"
#include "apps/graph_model.hpp"
#include "apps/graph_screen.hpp"

namespace apps {

namespace {
constexpr int kStatusH = 16;
constexpr int kRowH = 26;
constexpr int kTopY = 24;
}  // namespace

void YEditorScreen::on_activate() {
    editing_ = false;
}

// Row i's band for partial redraw, including the selection fill that
// starts 2 px above the row text.
void YEditorScreen::invalidate_row(int i) {
    invalidate(kTopY + i * kRowH - 2, kTopY + (i + 1) * kRowH - 2);
}

void YEditorScreen::begin_edit() {
    editing_ = true;
    input_.set_text(y_functions().expr[selected_]);
}

void YEditorScreen::commit_edit() {
    auto& fns = y_functions();
    std::snprintf(fns.expr[selected_], sizeof(fns.expr[selected_]), "%s", input_.text());
    // Auto-enable a slot when a non-empty expression is entered.
    if (fns.expr[selected_][0] != 0) {
        fns.enabled[selected_] = true;
    }
    editing_ = false;
    save_functions();
}

bool YEditorScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    if (editing_) {
        if (ev.key == Key::kEnter) {
            commit_edit();
            invalidate_row(selected_);
            return true;
        }
        if (ev.key == Key::kEscape) {
            editing_ = false;
            invalidate_row(selected_);
            return true;
        }
        if (input_.on_key(ev)) {
            invalidate_row(selected_);
            return true;
        }
        return false;
    }

    switch (ev.key) {
        case Key::kUp:
            if (selected_ > 0) {
                invalidate_row(selected_);
                --selected_;
                invalidate_row(selected_);
            }
            return true;
        case Key::kDown:
            if (selected_ < kNumFuncs - 1) {
                invalidate_row(selected_);
                ++selected_;
                invalidate_row(selected_);
            }
            return true;
        case Key::kEnter:
        case Key::kF1:
            begin_edit();
            invalidate_row(selected_);
            return true;
        case Key::kF2:
            y_functions().enabled[selected_] = !y_functions().enabled[selected_];
            save_functions();
            invalidate_row(selected_);
            return true;
        case Key::kF3:
            y_functions().expr[selected_][0] = 0;
            y_functions().enabled[selected_] = false;
            save_functions();
            invalidate_row(selected_);
            return true;
        case Key::kF4:
            ui::screen_manager().push(&graph_screen());
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void YEditorScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    fb.fill_rect(0, 0, platform::kScreenW, kStatusH, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 4, 2, "Y= FUNCTION EDITOR", kGrayLine);

    for (int i = 0; i < kNumFuncs; ++i) {
        const int y = kTopY + i * kRowH;
        const bool sel = (i == selected_);
        if (sel) {
            fb.fill_rect(0, y - 2, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }

        char label[8];
        std::snprintf(label, sizeof(label), "Y%d=", i + 1);
        font.draw_string(fb, 4, y, label, function_color(i));

        const int expr_x = 4 + 4 * font.width();
        if (sel && editing_) {
            input_.render(fb, expr_x, y, platform::kScreenW - expr_x - 20, font, true);
        } else {
            font.draw_string(fb, expr_x, y, y_functions().expr[i], kWhite);
        }

        // Enable checkbox at the right edge.
        const int box_x = platform::kScreenW - 16;
        fb.draw_rect(box_x, y, 12, 12, kGrayLine);
        if (y_functions().enabled[i]) {
            fb.fill_rect(box_x + 3, y + 3, 6, 6, kGreen);
        }
    }

    // Softkey bar
    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, "F1:EDIT F2:SEL F3:CLR F4:GRAPH", kGrayLine);
}

YEditorScreen& y_editor_screen() {
    static YEditorScreen instance;
    return instance;
}

}  // namespace apps
