#pragma once

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace math {
class Array;
}

namespace apps {

// Grid editor over math::matrices() [A]..[J] plus a read-only MatAns
// view (task 4A.6, phase4-spec §3.2). One matrix at a time; arrows
// move the cell cursor, TAB cycles the slot, F7 redimensions, F8
// clears. Entry: typed `matrix` (alias `mat`) command on the home
// screen.
//
// Strip-safe (§8): everything visible is cached as text by
// refresh_cells() from on_key/on_activate; render() only draws.
class MatrixEditorScreen : public ui::Screen {
public:
    MatrixEditorScreen() { track_dirty(); }

    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    static constexpr int kVisibleRows = 12;
    static constexpr int kVisibleCols = 3;
    static constexpr int kCellChars = 11;
    static constexpr int kAnsSlot = 10;  // After [A]..[J]

    int cur_slot_ = 0;  // 0-9 = [A]-[J], kAnsSlot = MatAns (read-only)
    int cur_row_ = 0;
    int cur_col_ = 0;
    int row_off_ = 0;
    int col_off_ = 0;
    bool editing_ = false;
    bool dim_prompt_ = false;  // Entry line is asking for "rows,cols"
    ui::InputLine input_;
    const char* msg_ = nullptr;  // Transient error text (entry line)

    // Render caches (refresh_cells).
    char title_[32] = {};  // "[A]  3x3"
    char cells_[kVisibleRows][kVisibleCols][kCellChars + 1] = {};
    char col_heads_[kVisibleCols][16] = {};  // "C12"
    char row_heads_[kVisibleRows][16] = {};  // "R12"
    char prompt_[32] = {};                   // "A(2,3)=" / "rows,cols="
    char cur_val_[24] = {};

    const math::Array& current() const;
    bool read_only() const { return cur_slot_ == kAnsSlot; }
    void refresh_cells();
    void invalidate_grid();
    void invalidate_entry();
    void clamp_cursor();
    void begin_edit(const platform::KeyEvent* first_key);
    void commit_edit();
    void commit_dim();
    void save_matrices();
};

MatrixEditorScreen& matrix_editor();

}  // namespace apps
