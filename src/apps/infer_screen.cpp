#include "apps/infer_screen.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "math/infer.hpp"
#include "math/lists.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/window_screen.hpp"

namespace apps {

namespace {

// Real Greek glyphs for the population parameters, replacing the ASCII
// spellings in the form labels (testdrive 2026-07-21). Display-only.
const char kSigmaLabel[] = {gfx::kGlyphSigmaLower, 0};             // σ
const char kMu0Label[] = {gfx::kGlyphMu, '0', 0};                  // μ0
const char kH1MuLabel[] = {'H', '1', ':', ' ', gfx::kGlyphMu, 0};  // "H1: μ"
const char kH1Mu12Label[] = {'H', '1', ':',           ' ', gfx::kGlyphMu,
                             '1', '-', gfx::kGlyphMu, '2', 0};  // "H1: μ1-μ2"
const char kH1MuDiffLabel[] = {'H', '1', ':', ' ', gfx::kGlyphMu,
                               'd', 'i', 'f', 'f', 0};  // "H1: μdiff"

constexpr int kTopY = 24;
constexpr int kRowH = 23;
constexpr int kResTopY = 24;
constexpr int kResRowH = 18;
constexpr int kSoftkeyY = 300;

using math::stats::Alt;
using math::stats::Interval;
using math::stats::TestResult;

// Numeric slots (shared across kinds so switching keeps values).
constexpr uint8_t kSXbar = 0;
constexpr uint8_t kSS = 1;  // s, or sigma for the z kinds
constexpr uint8_t kSN = 2;
constexpr uint8_t kSMu0 = 3;
constexpr uint8_t kSXbar2 = 4;
constexpr uint8_t kSS2 = 5;
constexpr uint8_t kSN2 = 6;
constexpr uint8_t kSX1 = 7;
constexpr uint8_t kSN1 = 8;
constexpr uint8_t kSX2 = 9;
constexpr uint8_t kSN2b = 10;
constexpr uint8_t kSP0 = 11;
constexpr uint8_t kSConf = 12;
constexpr double kNumDefaults[] = {0, 1, 30, 0, 0, 1, 30, 50, 100, 45, 100, 0.5, 0.95};

constexpr int kZTest = 0;
constexpr int kTTest1 = 1;
constexpr int kTTest2 = 2;
constexpr int kTPaired = 3;
constexpr int kPropZ1 = 4;
constexpr int kPropZ2 = 5;
constexpr int kChiGof = 6;
constexpr int kChi2Way = 7;
constexpr int kAnova = 8;
constexpr int kLinRegT = 9;
constexpr int kZInt = 10;
constexpr int kTInt1 = 11;
constexpr int kTInt2 = 12;
constexpr int kPropZInt1 = 13;
constexpr int kPropZInt2 = 14;
constexpr int kKindCount = 15;
const char* const kKindNames[kKindCount] = {
    "Z-Test",        "T-Test",     "2-Samp T-Test", "Paired T-Test", "1-Prop Z-Test",
    "2-Prop Z-Test", "Chi2 GOF",   "Chi2 2-Way",    "ANOVA",         "LinReg T-Test",
    "Z-Interval",    "T-Interval", "2-Samp T-Int",  "1-Prop Z-Int",  "2-Prop Z-Int",
};
// H1 alternatives: not-equal uses the real ≠ glyph (testdrive
// 2026-07-21); < and > stay ASCII.
const char kNeLabel[] = {gfx::kGlyphNotEqual, 0};
const char* const kAltNames[3] = {kNeLabel, "<", ">"};

bool kind_has_source(int kind) {
    return kind == kTTest1 || kind == kTTest2 || kind == kTInt1 || kind == kTInt2;
}

// Integer field: reject non-integers instead of silently rounding.
bool int_of(math::calc_t v, int* out) {
    if (std::isnan(v) || std::fabs(v) > 1e9) {
        return false;
    }
    const math::calc_t r = std::nearbyint(v);
    if (std::fabs(v - r) > 1e-9) {
        return false;
    }
    *out = static_cast<int>(r);
    return true;
}

}  // namespace

InferScreen::InferScreen() {
    for (int i = 0; i < kNumSlots; ++i) {
        vals_[i] = kNumDefaults[i];
    }
}

int InferScreen::build_rows(Row* rows) const {
    int n = 0;
    rows[n++] = {kRowKind, 0, "Test"};
    if (kind_has_source(kind_)) {
        rows[n++] = {kRowSource, 0, "Source"};
    }
    const bool data = source_ == 0;
    switch (kind_) {
        case kZTest:
            rows[n++] = {kRowNum, kSXbar, "xbar"};
            rows[n++] = {kRowNum, kSS, kSigmaLabel};
            rows[n++] = {kRowNum, kSN, "n"};
            rows[n++] = {kRowNum, kSMu0, kMu0Label};
            rows[n++] = {kRowAlt, 0, kH1MuLabel};
            break;
        case kTTest1:
            if (data) {
                rows[n++] = {kRowListA, 0, "List"};
            } else {
                rows[n++] = {kRowNum, kSXbar, "xbar"};
                rows[n++] = {kRowNum, kSS, "s"};
                rows[n++] = {kRowNum, kSN, "n"};
            }
            rows[n++] = {kRowNum, kSMu0, kMu0Label};
            rows[n++] = {kRowAlt, 0, kH1MuLabel};
            break;
        case kTTest2:
        case kTInt2:
            if (data) {
                rows[n++] = {kRowListA, 0, "List 1"};
                rows[n++] = {kRowListB, 0, "List 2"};
            } else {
                rows[n++] = {kRowNum, kSXbar, "xbar1"};
                rows[n++] = {kRowNum, kSS, "s1"};
                rows[n++] = {kRowNum, kSN, "n1"};
                rows[n++] = {kRowNum, kSXbar2, "xbar2"};
                rows[n++] = {kRowNum, kSS2, "s2"};
                rows[n++] = {kRowNum, kSN2, "n2"};
            }
            rows[n++] = {kRowPooled, 0, "Pooled"};
            if (kind_ == kTTest2) {
                rows[n++] = {kRowAlt, 0, kH1Mu12Label};
            } else {
                rows[n++] = {kRowNum, kSConf, "C-level"};
            }
            break;
        case kTPaired:
            rows[n++] = {kRowListA, 0, "List 1"};
            rows[n++] = {kRowListB, 0, "List 2"};
            rows[n++] = {kRowAlt, 0, kH1MuDiffLabel};
            break;
        case kPropZ1:
            rows[n++] = {kRowNum, kSX1, "x"};
            rows[n++] = {kRowNum, kSN1, "n"};
            rows[n++] = {kRowNum, kSP0, "p0"};
            rows[n++] = {kRowAlt, 0, "H1: p"};
            break;
        case kPropZ2:
        case kPropZInt2:
            rows[n++] = {kRowNum, kSX1, "x1"};
            rows[n++] = {kRowNum, kSN1, "n1"};
            rows[n++] = {kRowNum, kSX2, "x2"};
            rows[n++] = {kRowNum, kSN2b, "n2"};
            if (kind_ == kPropZ2) {
                rows[n++] = {kRowAlt, 0, "H1: p1-p2"};
            } else {
                rows[n++] = {kRowNum, kSConf, "C-level"};
            }
            break;
        case kChiGof:
            rows[n++] = {kRowListA, 0, "Obs list"};
            rows[n++] = {kRowListB, 0, "Exp list"};
            break;
        case kChi2Way:
            rows[n++] = {kRowCount, 0, "Columns"};
            break;
        case kAnova:
            rows[n++] = {kRowCount, 0, "Groups"};
            break;
        case kLinRegT:
            rows[n++] = {kRowListA, 0, "X list"};
            rows[n++] = {kRowListB, 0, "Y list"};
            rows[n++] = {kRowAlt, 0, "H1: slope"};
            break;
        case kZInt:
            rows[n++] = {kRowNum, kSXbar, "xbar"};
            rows[n++] = {kRowNum, kSS, kSigmaLabel};
            rows[n++] = {kRowNum, kSN, "n"};
            rows[n++] = {kRowNum, kSConf, "C-level"};
            break;
        case kTInt1:
            if (data) {
                rows[n++] = {kRowListA, 0, "List"};
            } else {
                rows[n++] = {kRowNum, kSXbar, "xbar"};
                rows[n++] = {kRowNum, kSS, "s"};
                rows[n++] = {kRowNum, kSN, "n"};
            }
            rows[n++] = {kRowNum, kSConf, "C-level"};
            break;
        case kPropZInt1:
            rows[n++] = {kRowNum, kSX1, "x"};
            rows[n++] = {kRowNum, kSN1, "n"};
            rows[n++] = {kRowNum, kSConf, "C-level"};
            break;
        default:
            break;
    }
    rows[n++] = {kRowCalc, 0, "Calculate"};
    return n;
}

void InferScreen::on_activate() {
    showing_results_ = false;
    editing_ = false;
    msg_ = nullptr;
    invalidate_all();
}

void InferScreen::adjust(const Row& row, int dir) {
    msg_ = nullptr;
    switch (row.type) {
        case kRowKind:
            kind_ = (kind_ + dir + kKindCount) % kKindCount;
            row_ = 0;
            break;
        case kRowSource:
            source_ = 1 - source_;
            break;
        case kRowListA:
            list_a_ = (list_a_ + dir + math::ListStore::kCount) % math::ListStore::kCount;
            break;
        case kRowListB:
            list_b_ = (list_b_ + dir + math::ListStore::kCount) % math::ListStore::kCount;
            break;
        case kRowAlt:
            alt_ = (alt_ + dir + 3) % 3;
            break;
        case kRowPooled:
            pooled_ = 1 - pooled_;
            break;
        case kRowCount:
            count_ = (count_ - 2 + dir + 5) % 5 + 2;  // 2..6
            break;
        default:
            break;
    }
    // The row list may have changed shape under the cursor.
    Row rows[kMaxRows];
    const int n = build_rows(rows);
    if (row_ >= n) {
        row_ = n - 1;
    }
}

void InferScreen::begin_edit(const Row& row, bool from_empty) {
    if (from_empty) {
        input_.set_text("");
    } else {
        char buf[24];
        math::format_number(vals_[row.slot], buf, sizeof(buf));
        input_.set_text(buf);
    }
    editing_ = true;
}

void InferScreen::commit_edit(const Row& row) {
    math::calc_t v = 0;
    if (math::eval_field(input_.text(), &v)) {
        vals_[row.slot] = v;
        msg_ = nullptr;
    }
    editing_ = false;
}

void InferScreen::add_line(const char* text) {
    if (line_count_ < kMaxLines) {
        std::snprintf(lines_[line_count_], sizeof(lines_[0]), "%s", text);
        ++line_count_;
    }
}

void InferScreen::add_kv(const char* key, math::calc_t v) {
    char num[24];
    math::format_number(v, num, sizeof(num));
    char buf[kLineChars];
    std::snprintf(buf, sizeof(buf), "%s=%s", key, num);
    add_line(buf);
}

void InferScreen::calculate() {
    line_count_ = 0;
    msg_ = nullptr;
    const auto alt = static_cast<Alt>(alt_);
    auto& ls = math::lists();
    const bool data = source_ == 0;

    // Integer fields (validated before dispatch).
    int n1 = 0;
    int n2 = 0;
    int x1 = 0;
    int x2 = 0;

    TestResult t;
    Interval ci;
    bool is_interval = false;
    const char* stat_name = "z";

    switch (kind_) {
        case kZTest:
            if (!int_of(vals_[kSN], &n1)) {
                msg_ = "n must be an integer";
                return;
            }
            t = math::stats::z_test_1samp(vals_[kSXbar], vals_[kSMu0], vals_[kSS], n1, alt);
            break;
        case kTTest1:
            stat_name = "t";
            if (data) {
                t = math::stats::t_test_1samp(ls.list(list_a_), vals_[kSMu0], alt);
            } else {
                if (!int_of(vals_[kSN], &n1)) {
                    msg_ = "n must be an integer";
                    return;
                }
                t = math::stats::t_test_1samp_summary(vals_[kSXbar], vals_[kSS], n1, vals_[kSMu0],
                                                      alt);
            }
            break;
        case kTTest2:
            stat_name = "t";
            if (data) {
                t = math::stats::t_test_2samp(ls.list(list_a_), ls.list(list_b_), pooled_ != 0,
                                              alt);
            } else {
                if (!int_of(vals_[kSN], &n1) || !int_of(vals_[kSN2], &n2)) {
                    msg_ = "n must be an integer";
                    return;
                }
                t = math::stats::t_test_2samp_summary(vals_[kSXbar], vals_[kSS], n1, vals_[kSXbar2],
                                                      vals_[kSS2], n2, pooled_ != 0, alt);
            }
            break;
        case kTPaired:
            stat_name = "t";
            t = math::stats::t_test_paired(ls.list(list_a_), ls.list(list_b_), alt);
            break;
        case kPropZ1:
            if (!int_of(vals_[kSX1], &x1) || !int_of(vals_[kSN1], &n1)) {
                msg_ = "x, n must be integers";
                return;
            }
            t = math::stats::prop_test_1samp(x1, n1, vals_[kSP0], alt);
            break;
        case kPropZ2:
            if (!int_of(vals_[kSX1], &x1) || !int_of(vals_[kSN1], &n1) ||
                !int_of(vals_[kSX2], &x2) || !int_of(vals_[kSN2b], &n2)) {
                msg_ = "x, n must be integers";
                return;
            }
            t = math::stats::prop_test_2samp(x1, n1, x2, n2, alt);
            break;
        case kChiGof:
            stat_name = "chi2";
            t = math::stats::chisq_gof(ls.list(list_a_), ls.list(list_b_));
            break;
        case kChi2Way: {
            stat_name = "chi2";
            const math::Array* cols[6];
            for (int i = 0; i < count_; ++i) {
                cols[i] = &ls.list(i);
            }
            t = math::stats::chisq_test_2way(cols, count_);
            break;
        }
        case kAnova: {
            stat_name = "F";
            const math::Array* groups[6];
            for (int i = 0; i < count_; ++i) {
                groups[i] = &ls.list(i);
            }
            t = math::stats::anova_oneway(groups, count_);
            break;
        }
        case kLinRegT:
            stat_name = "t";
            t = math::stats::linreg_test(ls.list(list_a_), ls.list(list_b_), alt);
            break;
        case kZInt:
            is_interval = true;
            if (!int_of(vals_[kSN], &n1)) {
                msg_ = "n must be an integer";
                return;
            }
            ci = math::stats::ci_mean_z(vals_[kSXbar], vals_[kSS], n1, vals_[kSConf]);
            break;
        case kTInt1:
            is_interval = true;
            if (data) {
                ci = math::stats::ci_mean_t(ls.list(list_a_), vals_[kSConf]);
            } else {
                if (!int_of(vals_[kSN], &n1)) {
                    msg_ = "n must be an integer";
                    return;
                }
                ci = math::stats::ci_mean_t_summary(vals_[kSXbar], vals_[kSS], n1, vals_[kSConf]);
            }
            break;
        case kTInt2:
            is_interval = true;
            if (data) {
                ci = math::stats::ci_diff_means(ls.list(list_a_), ls.list(list_b_), vals_[kSConf],
                                                pooled_ != 0);
            } else {
                if (!int_of(vals_[kSN], &n1) || !int_of(vals_[kSN2], &n2)) {
                    msg_ = "n must be an integer";
                    return;
                }
                ci = math::stats::ci_diff_means_summary(vals_[kSXbar], vals_[kSS], n1,
                                                        vals_[kSXbar2], vals_[kSS2], n2,
                                                        vals_[kSConf], pooled_ != 0);
            }
            break;
        case kPropZInt1:
            is_interval = true;
            if (!int_of(vals_[kSX1], &x1) || !int_of(vals_[kSN1], &n1)) {
                msg_ = "x, n must be integers";
                return;
            }
            ci = math::stats::ci_proportion(x1, n1, vals_[kSConf]);
            break;
        case kPropZInt2:
            is_interval = true;
            if (!int_of(vals_[kSX1], &x1) || !int_of(vals_[kSN1], &n1) ||
                !int_of(vals_[kSX2], &x2) || !int_of(vals_[kSN2b], &n2)) {
                msg_ = "x, n must be integers";
                return;
            }
            ci = math::stats::ci_diff_proportions(x1, n1, x2, n2, vals_[kSConf]);
            break;
        default:
            return;
    }

    char buf[kLineChars];
    if (is_interval) {
        if (!ci.ok) {
            msg_ = ci.error;
            return;
        }
        add_line(kKindNames[static_cast<unsigned>(kind_) % kKindCount]);
        char lo[17];
        char hi[17];
        math::format_number(ci.low, lo, sizeof(lo));
        math::format_number(ci.high, hi, sizeof(hi));
        std::snprintf(buf, sizeof(buf), "(%s, %s)", lo, hi);
        add_line(buf);
        add_kv("center", ci.point_estimate);
        add_kv("moe", ci.margin_of_error);
        add_kv("C", ci.confidence);
    } else {
        if (!t.ok) {
            msg_ = t.error;
            return;
        }
        add_line(kKindNames[static_cast<unsigned>(kind_) % kKindCount]);
        // H1 line for the alternatives-bearing tests.
        Row rows[kMaxRows];
        const int nrows = build_rows(rows);
        for (int i = 0; i < nrows; ++i) {
            if (rows[i].type == kRowAlt) {
                std::snprintf(buf, sizeof(buf), "%s %s", rows[i].label, kAltNames[alt_]);
                add_line(buf);
                break;
            }
        }
        add_kv(stat_name, t.statistic);
        add_kv("p", t.p_value);
        if (!std::isnan(t.df)) {
            add_kv("df", t.df);
        }
        if (!std::isnan(t.df2)) {
            add_kv("df2", t.df2);
        }
        if (!std::isnan(t.estimate)) {
            add_kv("est", t.estimate);
        }
        if (!std::isnan(t.se)) {
            add_kv("se", t.se);
        }
        if (t.n2 > 0) {
            std::snprintf(buf, sizeof(buf), "n1=%d n2=%d", t.n1, t.n2);
        } else {
            std::snprintf(buf, sizeof(buf), "n=%d", t.n1);
        }
        add_line(buf);
    }
    showing_results_ = true;
}

bool InferScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    if (showing_results_) {
        if (ev.key == Key::kEscape || ev.key == Key::kEnter) {
            showing_results_ = false;
            return true;
        }
        return false;
    }

    Row rows[kMaxRows];
    const int nrows = build_rows(rows);
    if (row_ >= nrows) {
        row_ = nrows - 1;
    }
    const Row& cur = rows[row_];

    if (editing_) {
        if (ev.key == Key::kEnter) {
            commit_edit(cur);
            return true;
        }
        if (ev.key == Key::kEscape) {
            editing_ = false;
            return true;
        }
        return input_.on_key(ev);
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
            adjust(cur, -1);
            return true;
        case Key::kRight:
            adjust(cur, +1);
            return true;
        case Key::kEnter:
            if (cur.type == kRowCalc) {
                calculate();
            } else if (cur.type == kRowNum) {
                begin_edit(cur, false);
            } else {
                adjust(cur, +1);
            }
            return true;
        case Key::kDel:
            if (cur.type == kRowNum) {
                begin_edit(cur, true);
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

void InferScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "TEST");

    if (showing_results_) {
        for (int i = 0; i < line_count_; ++i) {
            font.draw_string(fb, 8, kResTopY + i * kResRowH, lines_[i], i == 0 ? kGreen : kWhite);
        }
        fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
        font.draw_string(fb, 2, kSoftkeyY + 4, "ESC:BACK", kGrayLine);
        return;
    }

    Row rows[kMaxRows];
    const int nrows = build_rows(rows);
    for (int i = 0; i < nrows; ++i) {
        const int y = kTopY + i * kRowH;
        const bool sel = i == row_;
        if (sel) {
            fb.fill_rect(0, y - 2, platform::kScreenW, kRowH - 2,
                         platform::Color::from_rgb(0, 0, 60));
        }
        const Row& r = rows[i];
        char value[28] = {};
        const char* label = r.label;
        switch (r.type) {
            case kRowKind:
                std::snprintf(value, sizeof(value), "%s",
                              kKindNames[static_cast<unsigned>(kind_) % kKindCount]);
                break;
            case kRowSource:
                std::snprintf(value, sizeof(value), "%s", source_ == 0 ? "Data" : "Stats");
                break;
            case kRowListA:
                std::snprintf(value, sizeof(value), "l%d", list_a_ + 1);
                break;
            case kRowListB:
                std::snprintf(value, sizeof(value), "l%d", list_b_ + 1);
                break;
            case kRowAlt:
                std::snprintf(value, sizeof(value), "%s", kAltNames[alt_]);
                break;
            case kRowPooled:
                std::snprintf(value, sizeof(value), "%s", pooled_ != 0 ? "Yes" : "No");
                break;
            case kRowCount:
                std::snprintf(value, sizeof(value), "l1..l%d", count_);
                break;
            case kRowNum:
                math::format_number(vals_[r.slot], value, sizeof(value));
                break;
            default:  // kRowCalc
                std::snprintf(value, sizeof(value), "[ENTER]");
                break;
        }
        font.draw_string(fb, 10, y, label, kWhite);
        if (editing_ && sel && r.type == kRowNum) {
            const int vx = platform::kScreenW / 2;
            input_.render(fb, vx, y, platform::kScreenW - vx - 10, font, true);
        } else {
            font.draw_string(fb, platform::kScreenW - 10 - font.text_width(value), y, value,
                             kGreen);
        }
    }

    if (msg_ != nullptr) {
        const int my = kTopY + nrows * kRowH + 6;
        font.draw_string(fb, 10, my, msg_, kRed);
    }

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "L/R:CHANGE ENTER:EDIT/CALC ESC:BACK", kGrayLine);
}

InferScreen& infer_screen() {
    static InferScreen instance;
    return instance;
}

}  // namespace apps
