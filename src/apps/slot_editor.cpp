#include "apps/slot_editor.hpp"

#include <cstring>

#include "gfx/font.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/window_screen.hpp"

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
    // Syntax validity only — a stale complex value in the sweep
    // variable must not flag a valid expression red (4D.15).
    void* h = math::engine().compile(text, math::Engine::kNoComplexCheck);
    if (h == nullptr) {
        return false;
    }
    math::engine().free_compiled(h);
    return true;
}
}  // namespace

bool SlotEditorScreen::field_valid(int /*i*/, const char* text) const {
    return field_compiles(text);
}

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
        // 2026-07-18 remap: row ops move to non-F keys (ENTER edit,
        // SPACE toggle, DEL clear) so F2-F5 can follow the global
        // scheme. F1 (the global "editor" key) is swallowed — we're
        // already here.
        case Key::kEnter:
            begin_edit();
            invalidate_row(selected_);
            return true;
        case Key::kSpace:
            toggle_field(selected_);
            invalidate_row(selected_);
            return true;
        case Key::kDel:
            clear_field(selected_);
            invalidate_row(selected_);
            return true;
        case Key::kF1:
            return true;
        case Key::kF2:
            ui::screen_manager().push(&window_screen());
            return true;
        case Key::kF3:
            ui::screen_manager().push(&mode_screen());
            return true;
        case Key::kF4:
            goto_graph_trace();
            return true;
        case Key::kF5:
            // Toggle-style jump: popping back when the graph is right
            // beneath keeps repeated editor<->graph hops from stacking.
            ui::screen_manager().switch_to(&graph_screen());
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            if (field_key(selected_, ev)) {
                invalidate_row(selected_);
                return true;
            }
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

        char label[12];  // "u(nMin)=" needs 9 (4D.8)
        field_label(i, label, sizeof(label));
        font.draw_string(fb, 4, y, label, field_label_color(i));

        const int expr_x = 4 + label_width_chars() * font.width();
        if (sel && editing_) {
            input_.render(fb, expr_x, y, platform::kScreenW - expr_x - 20, font, true);
        } else {
            const char* text = field_text(i);
            const platform::Color color = field_valid(i, text) ? kWhite : kRed;
            // Truncate to the space left of the enable checkbox — long
            // expressions (stored regression models) ran beneath it
            // (HW 2026-07-19).
            const int max_chars = (platform::kScreenW - 20 - expr_x) / font.width();
            char shown[40];
            if (static_cast<int>(std::strlen(text)) > max_chars && max_chars > 1 &&
                max_chars < static_cast<int>(sizeof(shown))) {
                // One ellipsis glyph instead of three ASCII dots, so one
                // more character of the expression shows.
                std::memcpy(shown, text, static_cast<size_t>(max_chars) - 1);
                shown[max_chars - 1] = gfx::kGlyphEllipsis;
                shown[max_chars] = 0;
                font.draw_string(fb, expr_x, y, shown, color);
            } else {
                font.draw_string(fb, expr_x, y, text, color);
            }
        }

        // Enable checkbox at the right edge; shade marker beside it.
        if (field_has_checkbox(i)) {
            const int box_x = platform::kScreenW - 16;
            fb.draw_rect(box_x, y, 12, 12, kGrayLine);
            if (field_checked(i)) {
                fb.fill_rect(box_x + 3, y + 3, 6, 6, kGreen);
            }
            const char marker = field_marker(i);
            if (marker != 0) {
                font.draw_char(fb, box_x - font.width() - 2, y, marker, field_label_color(i));
            }
        }
    }

    // Softkey bar
    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4, softkey_text(), kGrayLine);
}

}  // namespace apps
