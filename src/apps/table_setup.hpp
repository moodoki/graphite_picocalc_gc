#pragma once

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// Table setup (task 2.12, spec §7.1): Start, Step, and Auto/Ask for
// the independent variable. Edits apply immediately and persist via
// the unified GraphState save (task 2.18) — same convention as the
// WINDOW screen, instead of the mock's SAVE/CANCEL pair.
class TableSetupScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

    // Open with the Step row preselected (table screen's F2 shortcut).
    void select_step() { selected_ = 1; }

private:
    static constexpr int kNumRows = 3;  // Start, Step, Independent

    int selected_ = 0;
    bool editing_ = false;
    ui::InputLine input_;

    void begin_edit();
    void commit_edit();
};

TableSetupScreen& table_setup_screen();

}  // namespace apps
