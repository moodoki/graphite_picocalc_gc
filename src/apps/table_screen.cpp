#include "apps/table_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "apps/graph_model.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/split_screen.hpp"
#include "apps/table_setup.hpp"

namespace apps {

namespace {
// Layout offsets within the pane [top_, top_ + height_).
constexpr int kHeaderOff = 20;
constexpr int kRowsOff = 36;
constexpr int kRowH = 16;       // One 8x16 cell; Spleen's built-in leading spaces the rows
constexpr int kColW = 80;       // 10 characters per column
constexpr int kDetailOff = 22;  // Detail line from the pane bottom

// Cell text: format_number capped to the column (9 chars + separator).
void cell_text(double v, char* buf, size_t buf_len) {
    char tmp[24];
    math::format_number(v, tmp, sizeof(tmp));
    std::snprintf(buf, buf_len, "%.9s", tmp);
}
}  // namespace

bool TableScreen::ask_mode() const {
    return graph::state().table.ask_mode;
}

int TableScreen::visible_rows() const {
    const int fit = (height_ - kRowsOff - kDetailOff) / kRowH;
    const int n = fit < kVisibleRows ? fit : kVisibleRows;
    return n > 0 ? n : 1;
}

double TableScreen::selected_value() const {
    const auto& t = graph::state().table;
    if (ask_mode()) {
        const int idx = base_ + sel_;
        return idx >= 0 && idx < ask_count_ ? ask_values_[idx] : 0.0;
    }
    return t.start + (base_ + sel_) * t.step;
}

void TableScreen::highlight_value(double v) {
    if (ask_mode()) {
        if (ask_count_ == 0) {
            return;
        }
        int best = 0;
        for (int i = 1; i < ask_count_; ++i) {
            if (std::fabs(ask_values_[i] - v) < std::fabs(ask_values_[best] - v)) {
                best = i;
            }
        }
        if (best < base_ || best >= base_ + visible_rows()) {
            base_ = best - visible_rows() / 2;
            dirty_ = true;
        }
        sel_ = best - base_;
        return;
    }
    const auto& t = graph::state().table;
    if (t.step == 0) {
        return;
    }
    const int n = static_cast<int>(std::floor((v - t.start) / t.step + 0.5));
    if (n < base_ || n >= base_ + visible_rows()) {
        base_ = n - visible_rows() / 2;
        dirty_ = true;
    }
    sel_ = n - base_;
}

void TableScreen::on_activate() {
    dirty_ = true;
    entering_ = false;
    col_off_ = 0;
}

void TableScreen::regenerate() {
    const auto& st = graph::state();
    col_count_ = table_column_count(st);
    const int max_off = col_count_ > kVisibleCols ? col_count_ - kVisibleCols : 0;
    col_off_ = col_off_ > max_off ? max_off : col_off_;

    if (ask_mode()) {
        base_ = std::max(0, std::min(base_, ask_count_ - visible_rows()));
        row_count_ = 0;
        for (int i = 0; i < visible_rows() && base_ + i < ask_count_; ++i) {
            indep_[i] = ask_values_[base_ + i];
            evaluate_table_row(st, indep_[i], values_[i], kMaxTableColumns);
            ++row_count_;
        }
        if (sel_ >= row_count_ && row_count_ > 0) {
            sel_ = row_count_ - 1;
        }
        sel_ = std::max(sel_, 0);
    } else {
        row_count_ = visible_rows();
        for (int i = 0; i < row_count_; ++i) {
            indep_[i] = st.table.start + (base_ + i) * st.table.step;
            evaluate_table_row(st, indep_[i], values_[i], kMaxTableColumns);
        }
    }
    dirty_ = false;
}

void TableScreen::move_selection(int dir) {
    if (ask_mode()) {
        if (dir < 0 && sel_ > 0) {
            --sel_;
        } else if (dir < 0 && base_ > 0) {
            --base_;
            dirty_ = true;
        } else if (dir > 0 && sel_ < row_count_ - 1) {
            ++sel_;
        } else if (dir > 0 && base_ + visible_rows() < ask_count_) {
            ++base_;
            dirty_ = true;
        }
        return;
    }
    // Auto mode: infinite scroll in both directions.
    if (dir < 0) {
        if (sel_ > 0) {
            --sel_;
        } else {
            --base_;
            dirty_ = true;
        }
    } else {
        if (sel_ < visible_rows() - 1) {
            ++sel_;
        } else {
            ++base_;
            dirty_ = true;
        }
    }
}

void TableScreen::commit_entry() {
    entering_ = false;
    if (input_.empty()) {
        return;
    }
    // Full expression entry; a bad expression adds nothing.
    math::calc_t v = 0;
    if (!math::eval_field(input_.text(), &v)) {
        return;
    }
    if (ask_count_ < kMaxAskRows) {
        ask_values_[ask_count_++] = v;
    } else {
        // Full: drop the oldest entry.
        for (int i = 1; i < kMaxAskRows; ++i) {
            ask_values_[i - 1] = ask_values_[i];
        }
        ask_values_[kMaxAskRows - 1] = v;
    }
    // Land the selection on the new (last) entry.
    base_ = ask_count_ > visible_rows() ? ask_count_ - visible_rows() : 0;
    sel_ = (ask_count_ < visible_rows() ? ask_count_ : visible_rows()) - 1;
    dirty_ = true;
}

bool TableScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    if (entering_) {
        if (ev.key == Key::kEnter) {
            commit_entry();
            return true;
        }
        if (ev.key == Key::kEscape) {
            entering_ = false;
            return true;
        }
        return input_.on_key(ev);
    }

    switch (ev.key) {
        case Key::kUp:
            move_selection(-1);
            return true;
        case Key::kDown:
            move_selection(+1);
            return true;
        case Key::kLeft:
            if (col_off_ > 0) {
                --col_off_;
            }
            return true;
        case Key::kRight:
            if (col_off_ + kVisibleCols < col_count_) {
                ++col_off_;
            }
            return true;
        case Key::kEnter:
            if (ask_mode()) {
                input_.clear();
                entering_ = true;
            }
            return true;
        // Global F-key scheme (2026-07-18 remap): F1 editor, F2 setup
        // (this screen's "window"), F3 mode, F4 trace, F5 graph
        // toggle, Alt+F5 split; DEL removes the row in ASK mode.
        case Key::kF1:
            push_mode_editor();
            return true;
        case Key::kF2:
            ui::screen_manager().push(&table_setup_screen());
            return true;
        case Key::kF3:
            ui::screen_manager().push(&mode_screen());
            return true;
        case Key::kF4:
            goto_graph_trace();
            return true;
        case Key::kF5:
            if (ev.alt_held) {  // Alt+F5: split graph|table
                ui::screen_manager().push(&split_screen());
            } else {
                ui::screen_manager().switch_to(&graph_screen());
            }
            return true;
        case Key::kDel:
            if (ask_mode() && row_count_ > 0) {
                const int idx = base_ + sel_;
                for (int i = idx + 1; i < ask_count_; ++i) {
                    ask_values_[i - 1] = ask_values_[i];
                }
                --ask_count_;
                dirty_ = true;
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void TableScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();
    const auto& st = graph::state();

    if (dirty_) {
        regenerate();
    }

    // Re-establish index bounds for the draws below (also constrains the
    // static analyzer's model of fields between calls).
    col_count_ = std::min(col_count_, static_cast<int>(kMaxTableColumns));
    col_off_ = std::max(col_off_, 0);
    row_count_ = std::min(row_count_, visible_rows());
    sel_ = std::max(0, std::min(sel_, kVisibleRows - 1));

    fb.clear(kBlack);
    fb.fill_rect(0, top_, platform::kScreenW, 16, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 4, top_ + 2, ask_mode() ? "TABLE (ASK)" : "TABLE", kGrayLine);

    // Header: independent label + visible dependent columns.
    font.draw_string(fb, 8, top_ + kHeaderOff, table_independent_label(st), kGreen);
    for (int c = 0; c < kVisibleCols && col_off_ + c < col_count_; ++c) {
        char label[8];
        table_column_label(st, col_off_ + c, label, sizeof(label));
        font.draw_string(fb, 8 + (c + 1) * kColW, top_ + kHeaderOff, label, kGreen);
    }
    if (col_off_ + kVisibleCols < col_count_) {
        font.draw_string(fb, platform::kScreenW - font.width(), top_ + kHeaderOff, ">", kGrayLine);
    }
    if (col_off_ > 0) {
        font.draw_string(fb, 0, top_ + kHeaderOff, "<", kGrayLine);
    }

    // Rows.
    for (int i = 0; i < row_count_; ++i) {
        const int y = top_ + kRowsOff + i * kRowH;
        if (i == sel_ && !entering_) {
            fb.fill_rect(0, y - 1, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        char buf[12];
        cell_text(indep_[i], buf, sizeof(buf));
        font.draw_string(fb, 8, y, buf, kWhite);
        for (int c = 0; c < kVisibleCols && col_off_ + c < col_count_; ++c) {
            cell_text(values_[i][col_off_ + c], buf, sizeof(buf));
            font.draw_string(fb, 8 + (c + 1) * kColW, y, buf, kWhite);
        }
    }
    if (row_count_ == 0 && ask_mode()) {
        font.draw_string(fb, 40, top_ + kRowsOff + 40, "ENTER adds a value", kGrayLine);
    }

    // Detail / entry line (full precision for the selected row).
    const int detail_y = top_ + height_ - kDetailOff;
    fb.fill_rect(0, detail_y - 2, platform::kScreenW, 16, platform::Color::from_rgb(20, 20, 20));
    if (entering_) {
        char prompt[8];
        std::snprintf(prompt, sizeof(prompt), "%s=", table_independent_label(st));
        font.draw_string(fb, 4, detail_y, prompt, kGreen);
        const int ex = 4 + static_cast<int>(sizeof(prompt)) * font.width() / 2;
        input_.render(fb, ex, detail_y, platform::kScreenW - ex - 4, font, true);
    } else if (sel_ < row_count_) {
        char full[24];
        char line[56];
        math::format_number(indep_[sel_], full, sizeof(full));
        const int off =
            std::snprintf(line, sizeof(line), "%s=%s", table_independent_label(st), full);
        if (col_off_ < col_count_) {
            char label[8];
            table_column_label(st, col_off_, label, sizeof(label));
            math::format_number(values_[sel_][col_off_], full, sizeof(full));
            std::snprintf(line + off, sizeof(line) - off, "  %s=%s", label, full);
        }
        font.draw_string(fb, 4, detail_y, line, kWhite);
    }

    // Softkey bar — standard divided cells like every other screen.
    // F3 and F4 both return to the previous view (usually the graph).
    const char* ed = "Y=";
    switch (graph::state().mode) {
        case graph::Mode::kParametric:
            ed = "PAR";
            break;
        case graph::Mode::kPolar:
            ed = "R=";
            break;
        default:
            break;
    }
    const char* const keys[6] = {ed, "SETP", "MODE", "TRC", "GRPH", ""};
    ui::draw_softkeys(fb, keys);
}

TableScreen& table_screen() {
    static TableScreen instance;
    return instance;
}

}  // namespace apps
