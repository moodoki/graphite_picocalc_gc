#pragma once

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// Spreadsheet-style list editor over math::lists() l1..l6 (task 3A.4,
// spec §3.1). Three list columns visible (LEFT/RIGHT scrolls across
// the six), rows scroll vertically; the row just past a list's end is
// its append row. Entry: typed `lists` command on the home screen.
//
// Strip-safe (§8): everything visible is cached as text by
// refresh_cells() from on_key/on_activate; render() only draws.
class ListEditorScreen : public ui::Screen {
public:
    ListEditorScreen() { track_dirty(); }

    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    static constexpr int kVisibleRows = 13;
    static constexpr int kVisibleCols = 3;
    static constexpr int kCellChars = 11;

    int cur_list_ = 0;  // 0-5
    int cur_row_ = 0;   // 0..count (count == append row)
    int row_off_ = 0;
    int col_off_ = 0;
    bool editing_ = false;
    ui::InputLine input_;
    const char* msg_ = nullptr;  // Transient error text (entry line)

    // Render caches (refresh_cells).
    char cells_[kVisibleRows][kVisibleCols][kCellChars + 1] = {};
    char headers_[kVisibleCols][16] = {};  // "l6:10000"
    char prompt_[32] = {};                 // "l6(10000)="
    char cur_val_[24] = {};

    void refresh_cells();
    void invalidate_grid();
    void invalidate_entry();
    void begin_edit(const platform::KeyEvent* first_key);
    void commit_edit();
    void delete_row();
    void sort_current(bool asc);
    void save_lists();
};

ListEditorScreen& list_editor();

}  // namespace apps
