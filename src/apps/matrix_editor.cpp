#include "apps/matrix_editor.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "platform/storage.hpp"
#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/functions.hpp"
#include "math/matrix.hpp"
#include "math/unified_home.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/window_screen.hpp"

namespace apps {

namespace {

constexpr int kTitleY = 20;
constexpr int kColHeadY = 36;
constexpr int kGridY = 52;
constexpr int kRowH = 16;
constexpr int kEntryY = 264;
constexpr int kSoftkeyY = 300;

constexpr int kNumColW = 36;  // Row-header gutter
constexpr int kCellW = 94;    // 11 chars + padding
constexpr int kGridBottom = kGridY + 12 * kRowH;

// DIM reshape staging (never aliases the store slot being resized).
math::Array g_reshape;

// Cell text: format_number, falling back to a short %.4g form when
// the full form overflows the 11-char cell (same as the list editor).
void format_cell(double v, char* buf, size_t cap) {
    char full[24];
    math::format_number(v, full, sizeof(full));
    if (std::strlen(full) <= 11) {
        std::snprintf(buf, cap, "%s", full);
    } else {
        std::snprintf(buf, cap, "%.4g", v);
    }
}

// Complex cell (4D.25): full form when it fits, else a short
// mode-aware fallback (same shape as the list editor's).
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

// The matrix's cell text, complex-aware.
void cell_text(const math::Array& m, int row, int col, char* buf, size_t cap) {
    if (m.dtype() == math::Dtype::kComplex) {
        format_cell_c(m.cget(row, col), buf, cap);
    } else {
        format_cell(m.get(row, col), buf, cap);
    }
}

}  // namespace

const math::Array& MatrixEditorScreen::current() const {
    return cur_slot_ == kAnsSlot ? math::mat_ans() : math::matrices().matrix(cur_slot_);
}

void MatrixEditorScreen::invalidate_grid() {
    invalidate(kTitleY - 2, kGridBottom);
}

void MatrixEditorScreen::invalidate_entry() {
    invalidate(kEntryY, kSoftkeyY);
}

void MatrixEditorScreen::clamp_cursor() {
    const auto& m = current();
    const int max_row = m.dim(0) > 0 ? m.dim(0) - 1 : 0;
    const int max_col = m.dim(1) > 0 ? m.dim(1) - 1 : 0;
    cur_row_ = std::min(cur_row_, max_row);
    cur_col_ = std::min(cur_col_, max_col);
    row_off_ = std::min(row_off_, cur_row_);
    col_off_ = std::min(col_off_, cur_col_);
    if (cur_row_ - row_off_ >= kVisibleRows) {
        row_off_ = cur_row_ - kVisibleRows + 1;
    }
    if (cur_col_ - col_off_ >= kVisibleCols) {
        col_off_ = cur_col_ - kVisibleCols + 1;
    }
}

void MatrixEditorScreen::refresh_cells() {
    const auto& m = current();
    const int rows = m.dim(0);
    const int cols = m.dim(1);

    if (cur_slot_ == kAnsSlot) {
        if (m.size() == 0) {
            std::snprintf(title_, sizeof(title_), "Ans  (no matrix result)");
        } else {
            std::snprintf(title_, sizeof(title_), "Ans  %dx%d  read-only", rows, cols);
        }
    } else if (m.size() == 0) {
        std::snprintf(title_, sizeof(title_), "[%c]  empty - F7:DIM", 'A' + cur_slot_);
    } else {
        std::snprintf(title_, sizeof(title_), "[%c]  %dx%d", 'A' + cur_slot_, rows, cols);
    }

    for (int c = 0; c < kVisibleCols; ++c) {
        if (col_off_ + c < cols) {
            std::snprintf(col_heads_[c], sizeof(col_heads_[c]), "C%d", col_off_ + c + 1);
        } else {
            col_heads_[c][0] = 0;
        }
    }
    for (int r = 0; r < kVisibleRows; ++r) {
        if (row_off_ + r < rows) {
            std::snprintf(row_heads_[r], sizeof(row_heads_[r]), "R%d", row_off_ + r + 1);
        } else {
            row_heads_[r][0] = 0;
        }
        for (int c = 0; c < kVisibleCols; ++c) {
            const int row = row_off_ + r;
            const int col = col_off_ + c;
            if (row < rows && col < cols) {
                cell_text(m, row, col, cells_[r][c], sizeof(cells_[r][c]));
            } else {
                cells_[r][c][0] = 0;
            }
        }
    }

    if (dim_prompt_) {
        std::snprintf(prompt_, sizeof(prompt_), "rows,cols=");
        cur_val_[0] = 0;
    } else if (m.size() == 0) {
        prompt_[0] = 0;
        cur_val_[0] = 0;
    } else {
        const char name = cur_slot_ == kAnsSlot ? '@' : static_cast<char>('A' + cur_slot_);
        if (cur_slot_ == kAnsSlot) {
            std::snprintf(prompt_, sizeof(prompt_), "Ans(%d,%d)=", cur_row_ + 1, cur_col_ + 1);
        } else {
            std::snprintf(prompt_, sizeof(prompt_), "%c(%d,%d)=", name, cur_row_ + 1, cur_col_ + 1);
        }
        if (m.dtype() == math::Dtype::kComplex) {
            math::format_complex(m.cget(cur_row_, cur_col_), math::number_mode(), cur_val_,
                                 sizeof(cur_val_));
        } else {
            math::format_number(m.get(cur_row_, cur_col_), cur_val_, sizeof(cur_val_));
        }
    }
}

void MatrixEditorScreen::on_activate() {
    editing_ = false;
    dim_prompt_ = false;
    msg_ = nullptr;
    clamp_cursor();
    refresh_cells();
    invalidate_all();
}

void MatrixEditorScreen::save_matrices() const {
    // Every call site only ever just mutated cur_slot_ (edit/dim/clear;
    // Ans is read-only so it never reaches here) — one-file-per-matrix
    // persistence (2026-07-22) means that's the only matrix that needs
    // writing.
    math::matrices().save(platform::storage(), cur_slot_);
}

void MatrixEditorScreen::begin_edit(const platform::KeyEvent* first_key) {
    if (read_only()) {
        msg_ = "Ans is read-only";
        invalidate_entry();
        return;
    }
    if (current().size() == 0) {
        msg_ = "F7:DIM to create";
        invalidate_entry();
        return;
    }
    editing_ = true;
    msg_ = nullptr;
    input_.clear();
    if (first_key != nullptr) {
        input_.on_key(*first_key);
    }
    invalidate_entry();
}

void MatrixEditorScreen::commit_edit() {
    double v = 0;
    bool complex_val = false;
    math::Complex cv;
    if (!math::eval_field(input_.text(), &v)) {
        // Complex entry (4D.25): `i`-bearing values and complex-valued
        // variables don't ride the real field evaluator.
        math::Complex cval;
        const char* cerr = nullptr;
        if (!math::unified::evaluate_scalar(input_.text(), &cval, &cerr)) {
            msg_ = "Bad value";
            invalidate_entry();
            return;
        }
        if (cval.is_real()) {
            v = cval.re;
        } else if (math::number_mode() == math::NumberMode::kReal) {
            msg_ = "Non-real result";  // D30 precedent
            invalidate_entry();
            return;
        } else {
            complex_val = true;
            cv = cval;
        }
    }
    math::Array& m = math::matrices().matrix(cur_slot_);
    // A complex element migrates the whole matrix to the complex tier
    // (PSRAM-only, D37) before the write.
    if (complex_val && !math::matops::make_complex(m)) {
        msg_ = m.size() > math::Array::kMaxComplexElements ? "Complex max 5000 cells"
                                                           : "Out of matrix memory";
        editing_ = false;
        invalidate_entry();
        return;
    }
    if (m.dtype() == math::Dtype::kComplex) {
        m.cset(cur_row_, cur_col_, complex_val ? cv : math::Complex(v));
    } else {
        m.set(cur_row_, cur_col_, v);
    }
    editing_ = false;
    save_matrices();
    // Advance right, wrapping to the next row (TI-style).
    if (cur_col_ + 1 < m.dim(1)) {
        ++cur_col_;
    } else if (cur_row_ + 1 < m.dim(0)) {
        cur_col_ = 0;
        col_off_ = 0;
        ++cur_row_;
    }
    clamp_cursor();
    refresh_cells();
    invalidate_grid();
    invalidate_entry();
}

void MatrixEditorScreen::commit_dim() {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*s", static_cast<int>(sizeof(buf)) - 1, input_.text());
    char* comma = std::strchr(buf, ',');
    double rv = 0;
    double cv = 0;
    bool ok = comma != nullptr;
    if (ok) {
        *comma = 0;
        ok = math::eval_field(buf, &rv) && math::eval_field(comma + 1, &cv);
    }
    const int rows = static_cast<int>(rv);
    const int cols = static_cast<int>(cv);
    if (!ok || rv != rows || cv != cols || rows < 1 || cols < 1 || rows > math::matops::kMaxDim ||
        cols > math::matops::kMaxDim) {
        msg_ = "Need rows,cols (1-99)";
        invalidate_entry();
        return;
    }
    dim_prompt_ = false;
    math::Array& m = math::matrices().matrix(cur_slot_);
    const char* err = nullptr;
    if (!math::matops::reshape(m, rows, cols, g_reshape, &err) ||
        !math::matops::copy(g_reshape, m)) {
        g_reshape.clear();
        msg_ = err != nullptr ? err : "Out of matrix memory";
        invalidate_entry();
        return;
    }
    g_reshape.clear();
    save_matrices();
    clamp_cursor();
    refresh_cells();
    invalidate_grid();
    invalidate_entry();
}

bool MatrixEditorScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    if (editing_ || dim_prompt_) {
        if (ev.key == Key::kEnter) {
            if (dim_prompt_) {
                commit_dim();
            } else {
                commit_edit();
            }
            return true;
        }
        if (ev.key == Key::kEscape) {
            editing_ = false;
            dim_prompt_ = false;
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
    const auto& m = current();
    switch (ev.key) {
        case Key::kUp:
            if (cur_row_ > 0) {
                --cur_row_;
                clamp_cursor();
                refresh_cells();
                invalidate_grid();
                invalidate_entry();
            }
            return true;
        case Key::kDown:
            if (cur_row_ + 1 < m.dim(0)) {
                ++cur_row_;
                clamp_cursor();
                refresh_cells();
                invalidate_grid();
                invalidate_entry();
            }
            return true;
        case Key::kLeft:
            if (cur_col_ > 0) {
                --cur_col_;
                clamp_cursor();
                refresh_cells();
                invalidate_grid();
                invalidate_entry();
            }
            return true;
        case Key::kRight:
            if (cur_col_ + 1 < m.dim(1)) {
                ++cur_col_;
                clamp_cursor();
                refresh_cells();
                invalidate_grid();
                invalidate_entry();
            }
            return true;
        case Key::kTab:
            cur_slot_ = (cur_slot_ + 1) % (kAnsSlot + 1);
            cur_row_ = 0;
            cur_col_ = 0;
            row_off_ = 0;
            col_off_ = 0;
            refresh_cells();
            invalidate_grid();
            invalidate_entry();
            return true;
        case Key::kEnter:
            begin_edit(nullptr);
            return true;
        case Key::kF7:
            if (read_only()) {
                msg_ = "Ans is read-only";
                invalidate_entry();
                return true;
            }
            dim_prompt_ = true;
            input_.clear();
            refresh_cells();
            invalidate_entry();
            return true;
        case Key::kF8:
            if (read_only()) {
                msg_ = "Ans is read-only";
                invalidate_entry();
                return true;
            }
            math::matrices().matrix(cur_slot_).clear();
            // Clearing also reverts a complex matrix to the real tier
            // (mirrors the list editor's F8, 4D.24/25).
            math::matrices().matrix(cur_slot_).set_dtype(math::Dtype::kDouble);
            cur_row_ = 0;
            cur_col_ = 0;
            row_off_ = 0;
            col_off_ = 0;
            save_matrices();
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

void MatrixEditorScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "MATRIX");

    font.draw_string(fb, 4, kTitleY, title_, kGreen);

    // Column headers, current column highlighted.
    for (int c = 0; c < kVisibleCols; ++c) {
        if (col_heads_[c][0] == 0) {
            continue;
        }
        const int x = kNumColW + c * kCellW;
        const bool sel = col_off_ + c == cur_col_;
        font.draw_string(fb, x + 4, kColHeadY, col_heads_[c], sel ? kWhite : kGridLine);
    }
    fb.draw_hline(0, kGridY - 2, platform::kScreenW, kGrayLine);

    // Grid rows: row headers + cached cell text.
    for (int r = 0; r < kVisibleRows; ++r) {
        const int y = kGridY + r * kRowH;
        if (row_heads_[r][0] != 0) {
            const bool rsel = row_off_ + r == cur_row_;
            font.draw_string(fb, kNumColW - 4 - font.text_width(row_heads_[r]), y, row_heads_[r],
                             rsel ? kWhite : kGridLine);
        }
        for (int c = 0; c < kVisibleCols; ++c) {
            if (cells_[r][c][0] == 0) {
                continue;
            }
            const int x = kNumColW + c * kCellW;
            const bool sel = col_off_ + c == cur_col_ && row_off_ + r == cur_row_;
            if (sel) {
                fb.fill_rect(x, y - 1, kCellW, kRowH, platform::Color::from_rgb(0, 0, 60));
            }
            font.draw_string(fb, x + 4, y, cells_[r][c], kWhite);
        }
    }

    // Entry line: prompt + input (editing) or the current value.
    fb.draw_hline(0, kEntryY, platform::kScreenW, kGrayLine);
    const int ty = kEntryY + 8;
    font.draw_string(fb, 2, ty, prompt_, kGreen);
    const int px = 2 + font.text_width(prompt_) + 2;
    if (editing_ || dim_prompt_) {
        input_.render(fb, px, ty, platform::kScreenW - px - 4, font, true);
    } else {
        font.draw_string(fb, px, ty, cur_val_, kWhite);
    }
    if (msg_ != nullptr) {
        font.draw_string(fb, platform::kScreenW - font.text_width(msg_) - 4, ty, msg_, kRed);
    }

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "TAB:MAT ENTER:EDIT F7:DIM F8:CLR", kGrayLine);
}

MatrixEditorScreen& matrix_editor() {
    static MatrixEditorScreen instance;
    return instance;
}

}  // namespace apps
