#pragma once

#include "ui/input_line.hpp"
#include "ui/screen.hpp"
#include "apps/table_model.hpp"

namespace apps {

// Table view (tasks 2.14-2.17, spec §7.2). Auto mode generates rows
// from TableConfig start/step with infinite scroll in both directions;
// ask mode accumulates user-entered independent values. Columns adapt
// to the graph mode (x|Y.., T|X1T Y1T.., th|r..) with LEFT/RIGHT
// horizontal scrolling when they overflow. The visible window is
// evaluated into a cache once per change (strip rendering stays cheap).
//
// Keys: UP/DOWN move/scroll; LEFT/RIGHT scroll columns; ENTER (ask)
// enter a value; F5 (ask) delete row; F1 setup; F2 setup at Step;
// F3/ESC back to graph.
class TableScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    static constexpr int kVisibleRows = 17;
    static constexpr int kVisibleCols = 3;  // Dependent columns on screen
    static constexpr int kMaxAskRows = 32;

    // Visible-window cache (regenerated when dirty_).
    double indep_[kVisibleRows] = {};
    double values_[kVisibleRows][kMaxTableColumns] = {};
    int row_count_ = 0;  // Rows actually present (ask mode may have fewer)
    int col_count_ = 0;

    int base_ = 0;     // Auto: index n of the first visible row; ask: window start
    int sel_ = 0;      // Selected visible row
    int col_off_ = 0;  // First visible dependent column
    bool dirty_ = true;

    // Ask mode entries.
    double ask_values_[kMaxAskRows] = {};
    int ask_count_ = 0;
    bool entering_ = false;
    ui::InputLine input_;

    bool ask_mode() const;
    void regenerate();
    void move_selection(int dir);
    void commit_entry();
};

TableScreen& table_screen();

}  // namespace apps
