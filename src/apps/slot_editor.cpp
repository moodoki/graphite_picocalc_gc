#include "apps/slot_editor.hpp"

#include "gfx/font.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "apps/graph_screen.hpp"

namespace apps {

namespace {
constexpr int kStatusH = 16;
constexpr int kTopY = 24;

// A row whose expression doesn't compile is drawn red instead of being
// silently skipped at plot time (HW feedback 2026-07-18). Same engine
// compile the recompute path uses; empty rows are fine.
bool field_compiles(const char* text) {
    if (text == nullptr || text[0] == 0) {
        return true;
    }
    void* h = math::engine().compile(text);
    if (h == nullptr) {
        return false;
    }
    math::engine().free_compiled(h);
    return true;
}
}  // namespace

void SlotEditorScreen::on_activate() {
    editing_ = false;
}

// Row i's band for partial redraw, including the selection fill that
// starts 2 px above the row text.
void SlotEditorScreen::invalidate_row(int i) {
    invalidate(kTopY + i * row_h_ - 2, kTopY + (i + 1) * row_h_ - 2);
}

void SlotEditorScreen::begin_edit() {
    editing_ = true;
    input_.set_text(field_text(selected_));
}

void SlotEditorScreen::commit_edit() {
    set_field_text(selected_, input_.text());
    editing_ = false;
}

bool SlotEditorScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    if (editing_) {
        if (ev.key == Key::kEnter) {
            const int committed = selected_;
            commit_edit();
            invalidate_row(committed);
            after_commit(committed);
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
            if (selected_ < field_count_ - 1) {
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
            toggle_field(selected_);
            invalidate_row(selected_);
            return true;
        case Key::kF3:
            clear_field(selected_);
            invalidate_row(selected_);
            return true;
        case Key::kF4:
            // Toggle-style jump: popping back when the graph is right
            // beneath keeps repeated editor<->graph hops from stacking.
            ui::screen_manager().switch_to(&graph_screen());
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void SlotEditorScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    fb.fill_rect(0, 0, platform::kScreenW, kStatusH, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 4, 2, title(), kGrayLine);

    for (int i = 0; i < field_count_; ++i) {
        const int y = kTopY + i * row_h_;
        const bool sel = (i == selected_);
        if (sel) {
            fb.fill_rect(0, y - 2, platform::kScreenW, row_h_, platform::Color::from_rgb(0, 0, 60));
        }

        char label[8];
        field_label(i, label, sizeof(label));
        font.draw_string(fb, 4, y, label, field_label_color(i));

        const int expr_x = 4 + label_width_chars() * font.width();
        if (sel && editing_) {
            input_.render(fb, expr_x, y, platform::kScreenW - expr_x - 20, font, true);
        } else {
            const char* text = field_text(i);
            font.draw_string(fb, expr_x, y, text, field_compiles(text) ? kWhite : kRed);
        }

        // Enable checkbox at the right edge.
        if (field_has_checkbox(i)) {
            const int box_x = platform::kScreenW - 16;
            fb.draw_rect(box_x, y, 12, 12, kGrayLine);
            if (field_checked(i)) {
                fb.fill_rect(box_x + 3, y + 3, 6, 6, kGreen);
            }
        }
    }

    // Softkey bar
    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, "F1:EDIT F2:SEL F3:CLR F4:GRAPH", kGrayLine);
}

}  // namespace apps
