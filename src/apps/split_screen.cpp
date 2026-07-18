#include "apps/split_screen.hpp"

#include "gfx/font.hpp"
#include "ui/screen_manager.hpp"
#include "apps/graph_screen.hpp"
#include "apps/table_screen.hpp"

namespace apps {

void SplitScreen::on_activate() {
    graph_screen().set_pane(0, kGraphH);
    table_screen().set_pane(kTableY, kTableH);
    // Reset transient pane state the same way a push would.
    graph_screen().on_activate();
    table_screen().on_activate();
    sync_panes();
}

void SplitScreen::on_deactivate() {
    graph_screen().reset_pane();
    table_screen().reset_pane();
}

// Nearest-row trace sync (task 2.20): the shared state is the current
// independent value; whichever pane changed it updates the other.
void SplitScreen::sync_panes() const {
    if (graph_focused_) {
        if (graph_screen().trace_active()) {
            table_screen().highlight_value(graph_screen().trace_value());
        }
    } else {
        graph_screen().sync_trace_to_value(table_screen().selected_value());
    }
}

bool SplitScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    // 2026-07-18 remap: F5 switches pane focus (it's the graph<->table
    // key), Alt+F5 exits the split, F4 always drives trace on the
    // graph pane (the table's own F4 would switch screens — wrong as
    // pane input). F1/F2/F3 forward normally: they push full-screen
    // screens (editor/window/mode) over the split.
    switch (ev.key) {
        case Key::kF5:
            if (ev.alt_held) {  // Alt+F5: toggle split off
                ui::screen_manager().pop();
            } else {
                graph_focused_ = !graph_focused_;
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        case Key::kF4: {
            const bool handled = graph_screen().on_key(ev);
            if (handled) {
                sync_panes();
            }
            return handled;
        }
        default:
            break;
    }
    const bool handled = graph_focused_ ? graph_screen().on_key(ev) : table_screen().on_key(ev);
    if (handled) {
        sync_panes();
    }
    return handled;
}

void SplitScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.set_pane_clip(0, 0, platform::kScreenW, kDividerY);
    graph_screen().render(fb);

    fb.set_pane_clip(0, kTableY, platform::kScreenW, kTableY + kTableH);
    table_screen().render(fb);

    fb.clear_pane_clip();

    // Divider with a white edge marking the focused pane.
    fb.fill_rect(0, kDividerY, platform::kScreenW, kTableY - kDividerY,
                 platform::Color::from_rgb(60, 60, 60));
    const int focus_y = graph_focused_ ? kDividerY : kTableY - 1;
    fb.draw_hline(0, focus_y, platform::kScreenW, kWhite);

    // Softkey bar.
    const int sk = platform::kScreenH - 20;
    fb.fill_rect(0, sk, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, sk + 4,
                     graph_focused_ ? "[GRAPH] F4:TRC F5:PANE aF5/ESC:FULL"
                                    : "[TABLE] F2:SETP F5:PANE aF5/ESC:FULL",
                     kGrayLine);
}

SplitScreen& split_screen() {
    static SplitScreen instance;
    return instance;
}

}  // namespace apps
