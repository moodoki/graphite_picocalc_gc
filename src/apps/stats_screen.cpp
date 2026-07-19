#include "apps/stats_screen.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/format.hpp"
#include "math/lists.hpp"
#include "math/stats.hpp"
#include "math/types.hpp"
#include "apps/graph_model.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/window_screen.hpp"
#include "graph/graph_state.hpp"

namespace apps {

namespace {

constexpr int kTopY = 44;
constexpr int kRowH = 28;
constexpr int kResTopY = 24;
constexpr int kResRowH = 16;
constexpr int kSoftkeyY = 300;

using math::stats::RegressionType;

// Only meaningful for analysis >= 2; the clamp keeps the enum cast in
// range for the static analyzer.
RegressionType reg_type(int analysis) {
    return static_cast<RegressionType>(analysis >= 2 ? analysis - 2 : 0);
}

}  // namespace

const char* StatsScreen::analysis_name() const {
    if (analysis_ == 0) {
        return "1-Var Stats";
    }
    if (analysis_ == 1) {
        return "2-Var Stats";
    }
    return math::stats::regression_name(reg_type(analysis_));
}

int StatsScreen::build_rows(RowKind* rows) const {
    int n = 0;
    rows[n++] = kRowAnalysis;
    rows[n++] = kRowXList;
    if (analysis_ == 0) {
        rows[n++] = kRowFreq;
    } else {
        rows[n++] = kRowYList;
    }
    if (is_regression()) {
        rows[n++] = kRowStore;
    }
    rows[n++] = kRowCalc;
    return n;
}

void StatsScreen::on_activate() {
    showing_results_ = false;
    msg_ = nullptr;
    invalidate_all();
}

void StatsScreen::adjust(RowKind kind, int dir) {
    switch (kind) {
        case kRowAnalysis:
            analysis_ = (analysis_ + dir + kAnalysisCount) % kAnalysisCount;
            break;
        case kRowXList:
            x_list_ = (x_list_ + dir + math::ListStore::kCount) % math::ListStore::kCount;
            break;
        case kRowYList:
            y_list_ = (y_list_ + dir + math::ListStore::kCount) % math::ListStore::kCount;
            break;
        case kRowFreq:  // OFF, l1..l6 (7 states, OFF = -1)
            freq_list_ = (freq_list_ + 1 + dir + 7) % 7 - 1;
            break;
        case kRowStore:  // OFF, y1..y7 (8 states, OFF = -1)
            store_slot_ = (store_slot_ + 1 + dir + 8) % 8 - 1;
            break;
        default:
            break;
    }
}

void StatsScreen::add_line(const char* text) {
    if (line_count_ < kMaxLines) {
        std::snprintf(lines_[line_count_], sizeof(lines_[0]), "%s", text);
        ++line_count_;
    }
}

void StatsScreen::add_kv(const char* key, double v) {
    char num[24];
    math::format_number(v, num, sizeof(num));
    char buf[kLineChars];
    std::snprintf(buf, sizeof(buf), "%s=%s", key, num);
    add_line(buf);
}

void StatsScreen::calculate() {
    line_count_ = 0;
    msg_ = nullptr;
    auto& ls = math::lists();
    char buf[kLineChars];

    if (analysis_ == 0) {
        const auto s = freq_list_ >= 0
                           ? math::stats::one_var_weighted(ls.list(x_list_), ls.list(freq_list_))
                           : math::stats::one_var(ls.list(x_list_));
        if (!s.ok) {
            msg_ = s.error;
            return;
        }
        if (freq_list_ >= 0) {
            std::snprintf(buf, sizeof(buf), "1-Var Stats l%c (freq l%c)",
                          static_cast<char>('1' + x_list_), static_cast<char>('1' + freq_list_));
        } else {
            std::snprintf(buf, sizeof(buf), "1-Var Stats l%c", static_cast<char>('1' + x_list_));
        }
        add_line(buf);
        add_kv("n", s.n);
        add_kv("mean", s.mean);
        add_kv("Sx", s.sample_stddev);
        add_kv("sigx", s.pop_stddev);
        add_kv("sum_x", s.sum);
        add_kv("sum_x2", s.sum_sq);
        add_kv("min", s.min_val);
        add_kv("Q1", s.q1);
        add_kv("med", s.median);
        add_kv("Q3", s.q3);
        add_kv("max", s.max_val);
    } else if (analysis_ == 1) {
        const auto s = math::stats::two_var(ls.list(x_list_), ls.list(y_list_));
        if (!s.ok) {
            msg_ = s.error;
            return;
        }
        std::snprintf(buf, sizeof(buf), "2-Var Stats l%c,l%c", static_cast<char>('1' + x_list_),
                      static_cast<char>('1' + y_list_));
        add_line(buf);
        add_kv("n", s.n);
        add_kv("mean_x", s.mean_x);
        add_kv("Sx", s.sample_stddev_x);
        add_kv("sigx", s.pop_stddev_x);
        add_kv("sum_x", s.sum_x);
        add_kv("sum_x2", s.sum_x2);
        add_kv("min_x", s.min_x);
        add_kv("max_x", s.max_x);
        add_kv("mean_y", s.mean_y);
        add_kv("Sy", s.sample_stddev_y);
        add_kv("sigy", s.pop_stddev_y);
        add_kv("sum_y", s.sum_y);
        add_kv("sum_y2", s.sum_y2);
        add_kv("min_y", s.min_y);
        add_kv("max_y", s.max_y);
        add_kv("sum_xy", s.sum_xy);
    } else {
        const auto type = reg_type(analysis_);
        const auto r = math::stats::regress(ls.list(x_list_), ls.list(y_list_), type);
        if (!r.ok) {
            msg_ = r.error;
            return;
        }
        std::snprintf(buf, sizeof(buf), "%s  %s", math::stats::regression_name(type),
                      math::stats::regression_form(type));
        add_line(buf);
        for (int i = 0; i < r.coeff_count; ++i) {
            add_kv(math::stats::coeff_name(type, i), r.coeffs[i]);
        }
        if (!std::isnan(r.r)) {
            add_kv("r", r.r);
        }
        if (!std::isnan(r.r_squared)) {
            add_kv("r^2", r.r_squared);
        }
        add_kv("n", r.n);
        if (!r.converged) {
            add_line("warning: iteration cap hit");
        }

        // Model expression: display (wrapped) and optional Y-slot
        // store (task 3B.8). The angle-mode conversion only matters
        // for SinReg (spec §10).
        char model[128];
        math::stats::format_model(r, math::angle_mode() == math::AngleMode::kDegrees, model,
                                  sizeof(model));
        add_line("model:");
        const size_t len = std::strlen(model);
        for (size_t at = 0; at < len; at += 36) {
            char part[kLineChars];
            std::snprintf(part, sizeof(part), " %.36s", model + at);
            add_line(part);
        }
        if (store_slot_ >= 0) {
            auto& y = graph::state().y;
            std::snprintf(y.expr[store_slot_], sizeof(y.expr[store_slot_]), "%s", model);
            y.enabled[store_slot_] = true;
            save_graph_state();
            std::snprintf(buf, sizeof(buf), "stored -> y%d", store_slot_ + 1);
            add_line(buf);
        }
    }
    showing_results_ = true;
    scroll_ = 0;
}

bool StatsScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    if (showing_results_) {
        switch (ev.key) {
            case Key::kUp:
                scroll_ = scroll_ > 0 ? scroll_ - 1 : 0;
                return true;
            case Key::kDown:
                if (scroll_ < line_count_ - kResVisible) {
                    ++scroll_;
                }
                return true;
            case Key::kEscape:
            case Key::kEnter:
                showing_results_ = false;
                return true;
            default:
                return false;
        }
    }

    RowKind rows[6];
    const int nrows = build_rows(rows);
    if (row_ >= nrows) {
        row_ = nrows - 1;
    }
    switch (ev.key) {
        case Key::kUp:
            if (row_ > 0) {
                --row_;
            }
            return true;
        case Key::kDown:
            if (row_ < nrows - 1) {
                ++row_;
            }
            return true;
        case Key::kLeft:
            msg_ = nullptr;
            adjust(rows[row_], -1);
            return true;
        case Key::kRight:
            msg_ = nullptr;
            adjust(rows[row_], +1);
            return true;
        case Key::kEnter:
            if (rows[row_] == kRowCalc) {
                // Calculate is synchronous and can take a while on
                // PSRAM-tier lists / slow LM fits, so push one
                // "Computing..." frame first (D23 revisit, HW
                // 2026-07-19). render() stays idempotent — the flag is
                // plain state, only ever set across this forced frame.
                computing_ = true;
                invalidate_all();
                ui::screen_manager().render_frame();
                computing_ = false;
                calculate();
                invalidate_all();
            } else {
                msg_ = nullptr;
                adjust(rows[row_], +1);
            }
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
            return false;
    }
}

void StatsScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "STATS");

    if (showing_results_) {
        for (int i = 0; i < kResVisible; ++i) {
            const int li = scroll_ + i;
            if (li >= line_count_) {
                break;
            }
            font.draw_string(fb, 8, kResTopY + i * kResRowH, lines_[li], li == 0 ? kGreen : kWhite);
        }
        if (line_count_ > kResVisible) {
            font.draw_string(fb, platform::kScreenW - 12, kResTopY, scroll_ > 0 ? "^" : " ",
                             kGrayLine);
            font.draw_string(fb, platform::kScreenW - 12, kResTopY + (kResVisible - 1) * kResRowH,
                             scroll_ < line_count_ - kResVisible ? "v" : " ", kGrayLine);
        }
        fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
        font.draw_string(fb, 2, kSoftkeyY + 4, "UP/DOWN:SCROLL ESC:BACK", kGrayLine);
        return;
    }

    RowKind rows[6];
    const int nrows = build_rows(rows);
    for (int i = 0; i < nrows; ++i) {
        const int y = kTopY + i * kRowH;
        if (i == row_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH - 4,
                         platform::Color::from_rgb(0, 0, 60));
        }
        const char* label = "Calculate";  // kRowCalc default
        char value[24] = "[ENTER]";
        switch (rows[i]) {
            case kRowAnalysis:
                label = "Analysis";
                std::snprintf(value, sizeof(value), "%s", analysis_name());
                break;
            case kRowXList:
                label = analysis_ == 0 ? "List" : "X list";
                std::snprintf(value, sizeof(value), "l%d", x_list_ + 1);
                break;
            case kRowYList:
                label = "Y list";
                std::snprintf(value, sizeof(value), "l%d", y_list_ + 1);
                break;
            case kRowFreq:
                label = "Freq";
                if (freq_list_ >= 0) {
                    std::snprintf(value, sizeof(value), "l%d", freq_list_ + 1);
                } else {
                    std::snprintf(value, sizeof(value), "OFF");
                }
                break;
            case kRowStore:
                label = "Store to";
                if (store_slot_ >= 0) {
                    std::snprintf(value, sizeof(value), "y%d", store_slot_ + 1);
                } else {
                    std::snprintf(value, sizeof(value), "OFF");
                }
                break;
            default:
                break;  // kRowCalc keeps the initializers
        }
        font.draw_string(fb, 12, y, label, kWhite);
        font.draw_string(fb, platform::kScreenW - 12 - font.text_width(value), y, value, kGreen);
    }

    // Model form hint for regressions; errors from the last Calculate.
    const int hint_y = kTopY + nrows * kRowH + 8;
    if (is_regression()) {
        font.draw_string(fb, 12, hint_y, math::stats::regression_form(reg_type(analysis_)),
                         kGridLine);
    }
    if (computing_) {
        font.draw_string(fb, 12, hint_y + 20, "Computing...",
                         platform::Color::from_rgb(255, 200, 0));
    } else if (msg_ != nullptr) {
        font.draw_string(fb, 12, hint_y + 20, msg_, kRed);
    }

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "L/R:CHANGE ENTER:CALC ESC:BACK", kGrayLine);
}

StatsScreen& stats_screen() {
    static StatsScreen instance;
    return instance;
}

}  // namespace apps
