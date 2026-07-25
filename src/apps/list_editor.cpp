#include "apps/list_editor.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/complex_expr.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/functions.hpp"
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

// Complex cell (4D.24): format_complex, falling back to a short form
// when the full form overflows the cell. The fallback follows the
// number mode too — a polar-mode cell must never regress to a+bi
// (observation 2026-07-26).
void format_cell_c(const math::Complex& z, char* buf, size_t cap) {
    char full[48];
    math::format_complex(z, math::number_mode(), full, sizeof(full));
    if (std::strlen(full) <= 11) {
        std::snprintf(buf, cap, "%s", full);
    } else if (math::number_mode() == math::NumberMode::kPolar) {
        double theta = z.argument();
        if (math::angle_mode() == math::AngleMode::kDegrees) {
            theta = math::fn::deg(theta);
        }
        std::snprintf(buf, cap, "%.3g%c%.3g", z.modulus(), math::kAngleGlyph, theta);
    } else {
        std::snprintf(buf, cap, "%.3g%+.3g%c", z.re, z.im, math::kImagUnitGlyph);
    }
}

// The list's cell text, complex-aware.
void cell_text(const math::Array& lst, int row, char* buf, size_t cap) {
    if (lst.dtype() == math::Dtype::kComplex) {
        format_cell_c(lst.cget(row), buf, cap);
    } else {
        format_cell(lst.get(row), buf, cap);
    }
}

}  // namespace

void ListEditorScreen::invalidate_grid() {
    invalidate(kHeaderY - 2, kGridBottom);
}

// Perf fix (2026-07-22): invalidate_grid() covers all kVisibleRows (13,
// ~14 strips of 20 on Pico 1) — fine for operations that touch every row
// (sort, delete-shift, clear), wasteful for plain cursor movement or a
// single-cell edit, which only ever change the old and new selected row.
// invalidate_header() and invalidate_row() let call sites narrow the
// band to just what actually changed.
void ListEditorScreen::invalidate_header() {
    invalidate(kHeaderY - 2, kGridY - 1);
}

void ListEditorScreen::invalidate_row(int visible_row) {
    if (visible_row < 0 || visible_row >= kVisibleRows) {
        return;
    }
    const int y = kGridY + visible_row * kRowH;
    invalidate(y - 1, y - 1 + kRowH);
}

void ListEditorScreen::invalidate_entry() {
    invalidate(kEntryY, kSoftkeyY);
}

int ListEditorScreen::total_cols() const {
    return math::ListStore::kCount + math::named_lists().count();
}

int ListEditorScreen::ref_of(int col) const {
    if (col < math::ListStore::kCount) {
        return col;
    }
    return math::kNamedRefBase + math::named_lists().nth(col - math::ListStore::kCount);
}

math::Array& ListEditorScreen::col_list(int col) const {
    return math::list_by_ref(ref_of(col));
}

void ListEditorScreen::refresh_cells() {
    for (int c = 0; c < kVisibleCols; ++c) {
        const int col = col_off_ + c;
        if (col >= total_cols()) {
            headers_[c][0] = 0;
            for (auto& row : cells_) {
                row[c][0] = 0;
            }
            continue;
        }
        const int count = col_list(col).size();
        char lname[8];
        math::list_ref_name(ref_of(col), lname, sizeof(lname));
        std::snprintf(headers_[c], sizeof(headers_[c]), "%s:%d", lname, count);
        for (int r = 0; r < kVisibleRows; ++r) {
            const int row = row_off_ + r;
            if (row < count) {
                cell_text(col_list(col), row, cells_[r][c], sizeof(cells_[r][c]));
            } else if (row == count) {
                std::snprintf(cells_[r][c], sizeof(cells_[r][c]), "_");
            } else {
                std::snprintf(cells_[r][c], sizeof(cells_[r][c]), "---");
            }
        }
    }
    if (name_prompt_ != 0) {
        std::snprintf(prompt_, sizeof(prompt_), "name=");
        cur_val_[0] = 0;
        return;
    }
    const int count = col_list(cur_list_).size();
    char lname[8];
    math::list_ref_name(ref_of(cur_list_), lname, sizeof(lname));
    std::snprintf(prompt_, sizeof(prompt_), "%s(%d)=", lname, cur_row_ + 1);
    if (cur_row_ < count) {
        const math::Array& cur = col_list(cur_list_);
        if (cur.dtype() == math::Dtype::kComplex) {
            math::format_complex(cur.cget(cur_row_), math::number_mode(), cur_val_,
                                 sizeof(cur_val_));
        } else {
            math::format_number(cur.get(cur_row_), cur_val_, sizeof(cur_val_));
        }
    } else {
        cur_val_[0] = 0;
    }
}

void ListEditorScreen::on_activate() {
    editing_ = false;
    name_prompt_ = 0;
    msg_ = nullptr;
    if (cur_list_ >= total_cols()) {  // A named list may have been removed
        cur_list_ = total_cols() - 1;
        col_off_ = std::min(col_off_, cur_list_);
    }
    refresh_cells();
    invalidate_all();
}

void ListEditorScreen::save_lists() const {
    // Every call site only ever just mutated cur_list_ (edit/delete/sort/
    // clear) — one-file-per-list persistence (2026-07-22) means that's
    // the only list that needs writing.
    const int ref = ref_of(cur_list_);
    if (ref >= math::kNamedRefBase) {
        math::named_lists().save(platform::storage(), ref - math::kNamedRefBase);
    } else {
        math::lists().save(platform::storage(), ref);
    }
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

void ListEditorScreen::commit_name() {
    char name[16];
    std::snprintf(name, sizeof(name), "%s", input_.text());
    if (!math::NamedLists::valid_name(name)) {
        msg_ = "Bad name (2-5 chars a-z)";
        invalidate_entry();
        return;
    }
    if (math::named_lists().find(name) >= 0) {
        msg_ = "Name in use";
        invalidate_entry();
        return;
    }
    auto& nl = math::named_lists();
    if (name_prompt_ == 1) {  // New list
        const int idx = nl.create(name);
        if (idx < 0) {
            msg_ = "Too many lists (20)";
            invalidate_entry();
            return;
        }
        // Jump to the new column.
        for (int c = math::ListStore::kCount; c < total_cols(); ++c) {
            if (ref_of(c) == math::kNamedRefBase + idx) {
                cur_list_ = c;
                break;
            }
        }
        cur_row_ = 0;
        row_off_ = 0;
        if (cur_list_ - col_off_ >= kVisibleCols) {
            col_off_ = cur_list_ - kVisibleCols + 1;
        }
    } else {  // Rename the selected named list
        nl.rename(ref_of(cur_list_) - math::kNamedRefBase, name);
    }
    nl.save_index(platform::storage());
    name_prompt_ = 0;
    refresh_cells();
    invalidate_grid();
    invalidate_entry();
}

void ListEditorScreen::commit_edit() {
    math::Array& lst = col_list(cur_list_);
    double v = 0;
    bool complex_val = false;
    math::Complex cv;
    if (!math::eval_field(input_.text(), &v)) {
        // Complex entry (4D.24): `i`-bearing values and complex-valued
        // variables don't ride the real field evaluator.
        const auto cr = math::complexexpr::evaluate(input_.text());
        if (!cr.ok) {
            msg_ = "Bad value";
            invalidate_entry();
            return;
        }
        if (cr.value.is_real()) {
            v = cr.value.re;
        } else if (math::number_mode() == math::NumberMode::kReal) {
            msg_ = "Non-real result";  // D30 precedent
            invalidate_entry();
            return;
        } else {
            complex_val = true;
            cv = cr.value;
        }
    }
    // A complex element migrates the whole list to the complex tier
    // (PSRAM-only, D37) before the write.
    if (complex_val && !math::listops::make_complex(lst)) {
        msg_ = "List memory unavailable";
        editing_ = false;
        invalidate_entry();
        return;
    }
    const int count = lst.size();
    const int cap = lst.dtype() == math::Dtype::kComplex ? math::Array::kMaxComplexElements
                                                         : math::Array::kMaxElements;
    if (cur_row_ >= count) {
        if (count >= cap) {
            msg_ = lst.dtype() == math::Dtype::kComplex ? "List full (5000)" : "List full (10000)";
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
    if (lst.dtype() == math::Dtype::kComplex) {
        lst.cset(cur_row_, complex_val ? cv : math::Complex(v));
    } else {
        lst.set(cur_row_, v);
    }
    editing_ = false;
    save_lists();
    const int old_row = cur_row_ - row_off_;
    // Advance down, TI-style.
    ++cur_row_;
    bool scrolled = false;
    if (cur_row_ - row_off_ >= kVisibleRows) {
        ++row_off_;
        scrolled = true;
    }
    refresh_cells();
    if (scrolled) {
        invalidate_grid();
    } else {
        // A commit may have appended (changing the header's "l1:N" count)
        // as well as moving the selection — narrow to just those bands
        // instead of the whole 13-row grid.
        invalidate_header();
        invalidate_row(old_row);
        invalidate_row(cur_row_ - row_off_);
    }
    invalidate_entry();
}

void ListEditorScreen::delete_row() {
    math::Array& lst = col_list(cur_list_);
    const int count = lst.size();
    if (cur_row_ >= count) {
        return;
    }
    if (lst.dtype() == math::Dtype::kComplex) {
        for (int at = cur_row_ + 1; at < count; ++at) {
            lst.cset(at - 1, lst.cget(at));
        }
    } else {
        static double buf[256];
        for (int at = cur_row_ + 1; at < count; at += 256) {
            const int m = count - at < 256 ? count - at : 256;
            lst.read_range(at, m, buf);
            lst.write_range(at - 1, m, buf);
        }
    }
    lst.resize(count - 1);
    save_lists();
    refresh_cells();
    invalidate_grid();
    invalidate_entry();
}

void ListEditorScreen::sort_current(bool asc) {
    math::Array& lst = col_list(cur_list_);
    if (lst.dtype() == math::Dtype::kComplex) {
        msg_ = "Non-real list";  // No ordering on complex (D37)
        invalidate_entry();
        return;
    }
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

    if (editing_ || name_prompt_ != 0) {
        if (ev.key == Key::kEnter) {
            if (name_prompt_ != 0) {
                commit_name();
            } else {
                commit_edit();
            }
            return true;
        }
        if (ev.key == Key::kEscape) {
            editing_ = false;
            name_prompt_ = 0;
            msg_ = nullptr;
            refresh_cells();
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
    const int count = col_list(cur_list_).size();
    switch (ev.key) {
        case Key::kUp:
            if (cur_row_ > 0) {
                const int old_row = cur_row_ - row_off_;
                --cur_row_;
                const int new_row_off = std::min(row_off_, cur_row_);
                const bool scrolled = new_row_off != row_off_;
                row_off_ = new_row_off;
                refresh_cells();
                if (scrolled) {
                    invalidate_grid();
                } else {
                    // Same visible window — only the old and new
                    // selected row's highlight actually changed.
                    invalidate_row(old_row);
                    invalidate_row(cur_row_ - row_off_);
                }
                invalidate_entry();
            }
            return true;
        case Key::kDown:
            if (cur_row_ < count) {  // count == append row
                const int old_row = cur_row_ - row_off_;
                ++cur_row_;
                bool scrolled = false;
                if (cur_row_ - row_off_ >= kVisibleRows) {
                    ++row_off_;
                    scrolled = true;
                }
                refresh_cells();
                if (scrolled) {
                    invalidate_grid();
                } else {
                    invalidate_row(old_row);
                    invalidate_row(cur_row_ - row_off_);
                }
                invalidate_entry();
            }
            return true;
        case Key::kLeft:
        case Key::kRight: {
            const int dir = ev.key == Key::kRight ? 1 : -1;
            const int next = cur_list_ + dir;
            if (next < 0 || next >= total_cols()) {
                return true;
            }
            cur_list_ = next;
            if (cur_list_ < col_off_) {
                col_off_ = cur_list_;
            } else if (cur_list_ - col_off_ >= kVisibleCols) {
                col_off_ = cur_list_ - kVisibleCols + 1;
            }
            cur_row_ = std::min(cur_row_, col_list(cur_list_).size());
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
            // Clearing also reverts a complex list to the real tier.
            col_list(cur_list_).clear();
            col_list(cur_list_).set_dtype(math::Dtype::kDouble);
            col_list(cur_list_).resize(0);
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
            // Named-list management (4D.13): Alt+N new, Alt+R rename,
            // Alt+X delete (plain letters start a cell edit below).
            if (ev.alt_held && (ev.ch == 'n' || ev.ch == 'N')) {
                if (math::named_lists().count() >= math::NamedLists::kMax) {
                    msg_ = "Too many lists (20)";
                    invalidate_entry();
                    return true;
                }
                name_prompt_ = 1;
                input_.clear();
                refresh_cells();
                invalidate_entry();
                return true;
            }
            if (ev.alt_held && (ev.ch == 'r' || ev.ch == 'R')) {
                if (ref_of(cur_list_) < math::kNamedRefBase) {
                    msg_ = "l1-l6 are fixed";
                    invalidate_entry();
                    return true;
                }
                name_prompt_ = 2;
                input_.clear();
                refresh_cells();
                invalidate_entry();
                return true;
            }
            if (ev.alt_held && (ev.ch == 'x' || ev.ch == 'X')) {
                const int ref = ref_of(cur_list_);
                if (ref < math::kNamedRefBase) {
                    msg_ = "l1-l6 are fixed";
                    invalidate_entry();
                    return true;
                }
                const int idx = ref - math::kNamedRefBase;
                math::named_lists().remove(idx);
                math::named_lists().remove_file(platform::storage(), idx);
                math::named_lists().save_index(platform::storage());
                cur_list_ = std::min(cur_list_, total_cols() - 1);
                col_off_ = std::min(col_off_, cur_list_);
                cur_row_ = 0;
                row_off_ = 0;
                refresh_cells();
                invalidate_grid();
                invalidate_entry();
                return true;
            }
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
    if (editing_ || name_prompt_ != 0) {
        input_.render(fb, px, ty, platform::kScreenW - px - 4, font, true);
    } else {
        font.draw_string(fb, px, ty, cur_val_, kWhite);
    }
    if (msg_ != nullptr) {
        font.draw_string(fb, platform::kScreenW - font.text_width(msg_) - 4, ty, msg_, kRed);
    }

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "ENTER:EDIT DEL:ROW F6/F7:SORT Alt+N:NEW", kGrayLine);
}

ListEditorScreen& list_editor() {
    static ListEditorScreen instance;
    return instance;
}

}  // namespace apps
