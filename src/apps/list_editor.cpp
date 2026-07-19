#include "apps/list_editor.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/list_ops.hpp"
#include "math/lists.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/window_screen.hpp"

namespace apps {

namespace {

constexpr int kHeaderY = 20;
constexpr int kGridY = 38;
constexpr int kRowH = 16;
constexpr int kEntryY = 264;
constexpr int kSoftkeyY = 300;

constexpr int kNumColW = 36;  // Row-number gutter
constexpr int kCellW = 94;    // 11 chars + padding
constexpr int kGridBottom = kGridY + 13 * kRowH;

// Cell text: format_number, falling back to a short %.4g form when
// the full form overflows the 11-char cell.
void format_cell(double v, char* buf, size_t cap) {
    char full[24];
    math::format_number(v, full, sizeof(full));
    if (std::strlen(full) <= 11) {
        std::snprintf(buf, cap, "%s", full);
    } else {
        std::snprintf(buf, cap, "%.4g", v);
    }
}

}  // namespace

void ListEditorScreen::invalidate_grid() {
    invalidate(kHeaderY - 2, kGridBottom);
}

void ListEditorScreen::invalidate_entry() {
    invalidate(kEntryY, kSoftkeyY);
}

void ListEditorScreen::refresh_cells() {
    for (int c = 0; c < kVisibleCols; ++c) {
        const int li = col_off_ + c;
        const int count = math::lists().list(li).size();
        std::snprintf(headers_[c], sizeof(headers_[c]), "l%d:%d", li + 1, count);
        for (int r = 0; r < kVisibleRows; ++r) {
            const int row = row_off_ + r;
            if (row < count) {
                format_cell(math::lists().list(li).get(row), cells_[r][c], sizeof(cells_[r][c]));
            } else if (row == count) {
                std::snprintf(cells_[r][c], sizeof(cells_[r][c]), "_");
            } else {
                std::snprintf(cells_[r][c], sizeof(cells_[r][c]), "---");
            }
        }
    }
    const int count = math::lists().list(cur_list_).size();
    std::snprintf(prompt_, sizeof(prompt_), "l%d(%d)=", cur_list_ + 1, cur_row_ + 1);
    if (cur_row_ < count) {
        math::format_number(math::lists().list(cur_list_).get(cur_row_), cur_val_,
                            sizeof(cur_val_));
    } else {
        cur_val_[0] = 0;
    }
}

void ListEditorScreen::on_activate() {
    editing_ = false;
    msg_ = nullptr;
    refresh_cells();
    invalidate_all();
}

void ListEditorScreen::save_lists() {
    math::lists().save(platform::storage());
}

void ListEditorScreen::begin_edit(const platform::KeyEvent* first_key) {
    editing_ = true;
    msg_ = nullptr;
    input_.clear();
    if (first_key != nullptr) {
        input_.on_key(*first_key);
    }
    invalidate_entry();
}

void ListEditorScreen::commit_edit() {
    double v = 0;
    if (!math::eval_field(input_.text(), &v)) {
        msg_ = "Bad value";
        invalidate_entry();
        return;
    }
    math::Array& lst = math::lists().list(cur_list_);
    const int count = lst.size();
    if (cur_row_ >= count) {
        if (count >= math::Array::kMaxElements) {
            msg_ = "List full (10000)";
            editing_ = false;
            invalidate_entry();
            return;
        }
        if (!lst.resize(count + 1)) {
            // The only other resize failure is the PSRAM tier being
            // unavailable (cold boot, D14) or exhausted.
            msg_ = "List memory unavailable";
            editing_ = false;
            invalidate_entry();
            return;
        }
    }
    lst.set(cur_row_, v);
    editing_ = false;
    save_lists();
    // Advance down, TI-style.
    ++cur_row_;
    if (cur_row_ - row_off_ >= kVisibleRows) {
        ++row_off_;
    }
    refresh_cells();
    invalidate_grid();
    invalidate_entry();
}

void ListEditorScreen::delete_row() {
    math::Array& lst = math::lists().list(cur_list_);
    const int count = lst.size();
    if (cur_row_ >= count) {
        return;
    }
    static double buf[256];
    for (int at = cur_row_ + 1; at < count; at += 256) {
        const int m = count - at < 256 ? count - at : 256;
        lst.read_range(at, m, buf);
        lst.write_range(at - 1, m, buf);
    }
    lst.resize(count - 1);
    save_lists();
    refresh_cells();
    invalidate_grid();
    invalidate_entry();
}

void ListEditorScreen::sort_current(bool asc) {
    math::Array& lst = math::lists().list(cur_list_);
    const bool ok = asc ? math::listops::sort_asc(lst) : math::listops::sort_desc(lst);
    if (!ok) {
        msg_ = "Sort needs PSRAM";
        invalidate_entry();
        return;
    }
    save_lists();
    refresh_cells();
    invalidate_grid();
    invalidate_entry();
}

bool ListEditorScreen::on_key(const platform::KeyEvent& ev) {
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
            msg_ = nullptr;
            invalidate_entry();
            return true;
        }
        if (input_.on_key(ev)) {
            invalidate_entry();
            return true;
        }
        return false;
    }

    msg_ = nullptr;
    const int count = math::lists().list(cur_list_).size();
    switch (ev.key) {
        case Key::kUp:
            if (cur_row_ > 0) {
                --cur_row_;
                row_off_ = std::min(row_off_, cur_row_);
                refresh_cells();
                invalidate_grid();
                invalidate_entry();
            }
            return true;
        case Key::kDown:
            if (cur_row_ < count) {  // count == append row
                ++cur_row_;
                if (cur_row_ - row_off_ >= kVisibleRows) {
                    ++row_off_;
                }
                refresh_cells();
                invalidate_grid();
                invalidate_entry();
            }
            return true;
        case Key::kLeft:
        case Key::kRight: {
            const int dir = ev.key == Key::kRight ? 1 : -1;
            const int next = cur_list_ + dir;
            if (next < 0 || next >= math::ListStore::kCount) {
                return true;
            }
            cur_list_ = next;
            if (cur_list_ < col_off_) {
                col_off_ = cur_list_;
            } else if (cur_list_ - col_off_ >= kVisibleCols) {
                col_off_ = cur_list_ - kVisibleCols + 1;
            }
            cur_row_ = std::min(cur_row_, math::lists().list(cur_list_).size());
            row_off_ = std::min(row_off_, cur_row_);
            refresh_cells();
            invalidate_grid();
            invalidate_entry();
            return true;
        }
        case Key::kEnter:
            begin_edit(nullptr);
            return true;
        case Key::kDel:
            delete_row();
            return true;
        case Key::kF6:
            sort_current(true);
            return true;
        case Key::kF7:
            sort_current(false);
            return true;
        case Key::kF8:
            math::lists().list(cur_list_).resize(0);
            cur_row_ = 0;
            row_off_ = 0;
            save_lists();
            refresh_cells();
            invalidate_grid();
            invalidate_entry();
            return true;
        // Global F-key scheme (D20).
        case Key::kF1:
            push_mode_editor();
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
            ui::screen_manager().push(&graph_screen());
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            // Typing starts an edit of the current cell directly.
            if (ev.ch != 0) {
                begin_edit(&ev);
                return true;
            }
            return false;
    }
}

void ListEditorScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "LISTS");

    // Column headers ("l1:4"), the current list highlighted.
    for (int c = 0; c < kVisibleCols; ++c) {
        const int x = kNumColW + c * kCellW;
        const bool sel = col_off_ + c == cur_list_;
        if (sel) {
            fb.fill_rect(x, kHeaderY - 2, kCellW, kRowH, platform::Color::from_rgb(0, 0, 90));
        }
        font.draw_string(fb, x + 4, kHeaderY, headers_[c], sel ? kWhite : kGreen);
    }
    fb.draw_hline(0, kGridY - 2, platform::kScreenW, kGrayLine);

    // Grid rows: row numbers (1-based) + cached cell text.
    for (int r = 0; r < kVisibleRows; ++r) {
        const int y = kGridY + r * kRowH;
        char num[12];
        std::snprintf(num, sizeof(num), "%d", row_off_ + r + 1);
        font.draw_string(fb, kNumColW - 4 - font.text_width(num), y, num, kGridLine);
        for (int c = 0; c < kVisibleCols; ++c) {
            const int x = kNumColW + c * kCellW;
            const bool sel = col_off_ + c == cur_list_ && row_off_ + r == cur_row_;
            if (sel) {
                fb.fill_rect(x, y - 1, kCellW, kRowH, platform::Color::from_rgb(0, 0, 60));
            }
            const char* text = cells_[r][c];
            // Exact matches only — a leading '-' also starts negative
            // numbers, which dimmed them to the placeholder gray
            // (HW 2026-07-19).
            const bool placeholder = std::strcmp(text, "_") == 0 || std::strcmp(text, "---") == 0;
            font.draw_string(fb, x + 4, y, text, placeholder ? kGridLine : kWhite);
        }
    }

    // Entry line: prompt + input (editing) or the current value.
    fb.draw_hline(0, kEntryY, platform::kScreenW, kGrayLine);
    const int ty = kEntryY + 8;
    font.draw_string(fb, 2, ty, prompt_, kGreen);
    const int px = 2 + font.text_width(prompt_) + 2;
    if (editing_) {
        input_.render(fb, px, ty, platform::kScreenW - px - 4, font, true);
    } else {
        font.draw_string(fb, px, ty, cur_val_, kWhite);
    }
    if (msg_ != nullptr) {
        font.draw_string(fb, platform::kScreenW - font.text_width(msg_) - 4, ty, msg_, kRed);
    }

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "ENTER:EDIT DEL:ROW F6/F7:SORT F8:CLR", kGrayLine);
}

ListEditorScreen& list_editor() {
    static ListEditorScreen instance;
    return instance;
}

}  // namespace apps
