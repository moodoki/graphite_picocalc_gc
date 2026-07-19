#include "apps/dist_screen.hpp"

#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "math/engine.hpp"
#include "math/format.hpp"
#include "apps/graph_screen.hpp"
#include "apps/mode_screen.hpp"
#include "apps/nav.hpp"
#include "apps/window_screen.hpp"

namespace apps {

namespace {

constexpr int kTopY = 44;
constexpr int kRowH = 28;
constexpr int kSoftkeyY = 300;
constexpr int kMaxParams = 4;

// Parameter slots (indexes into DistScreen::vals_) with display labels
// and defaults. TI-style open tails use +/-1e99.
constexpr uint8_t kPX = 0;
constexpr uint8_t kPLo = 1;
constexpr uint8_t kPHi = 2;
constexpr uint8_t kPArea = 3;
constexpr uint8_t kPMu = 4;
constexpr uint8_t kPSd = 5;
constexpr uint8_t kPDf = 6;
constexpr uint8_t kPDf1 = 7;
constexpr uint8_t kPDf2 = 8;
constexpr uint8_t kPK = 9;
constexpr uint8_t kPN = 10;
constexpr uint8_t kPP = 11;
constexpr uint8_t kPLam = 12;
const char* const kParamNames[] = {"x",   "lo",  "hi", "area", "mu", "sd",    "df",
                                   "df1", "df2", "k",  "n",    "p",  "lambda"};
constexpr double kParamDefaults[] = {0, -1e99, 1e99, 0.95, 0, 1, 10, 3, 10, 1, 10, 0.5, 3};

struct FnSpec {
    const char* label;           // "pdf" / "pmf" / "cdf" / "inv"
    const char* fn_name;         // Catalog name ("normal_cdf")
    uint8_t params[kMaxParams];  // Param slots, in call order
    uint8_t nparams;
};

struct DistSpec {
    const char* name;
    FnSpec fns[3];
    uint8_t fn_count;
};

const DistSpec kDists[] = {
    {"Normal",
     {{"pdf", "normal_pdf", {kPX, kPMu, kPSd}, 3},
      {"cdf", "normal_cdf", {kPLo, kPHi, kPMu, kPSd}, 4},
      {"inv", "normal_inv", {kPArea, kPMu, kPSd}, 3}},
     3},
    {"Student t",
     {{"pdf", "t_pdf", {kPX, kPDf}, 2},
      {"cdf", "t_cdf", {kPLo, kPHi, kPDf}, 3},
      {"inv", "t_inv", {kPArea, kPDf}, 2}},
     3},
    {"Chi-square",
     {{"pdf", "chisq_pdf", {kPX, kPDf}, 2},
      {"cdf", "chisq_cdf", {kPLo, kPHi, kPDf}, 3},
      {"inv", "chisq_inv", {kPArea, kPDf}, 2}},
     3},
    {"F",
     {{"pdf", "f_pdf", {kPX, kPDf1, kPDf2}, 3},
      {"cdf", "f_cdf", {kPLo, kPHi, kPDf1, kPDf2}, 4},
      {"inv", "f_inv", {kPArea, kPDf1, kPDf2}, 3}},
     3},
    {"Binomial",
     {{"pmf", "binomial_pmf", {kPK, kPN, kPP}, 3}, {"cdf", "binomial_cdf", {kPK, kPN, kPP}, 3}},
     2},
    {"Poisson",
     {{"pmf", "poisson_pmf", {kPK, kPLam}, 2}, {"cdf", "poisson_cdf", {kPK, kPLam}, 2}},
     2},
    {"Geometric",
     {{"pmf", "geometric_pmf", {kPK, kPP}, 2}, {"cdf", "geometric_cdf", {kPK, kPP}, 2}},
     2},
};
constexpr int kDistCount = sizeof(kDists) / sizeof(kDists[0]);

static_assert(sizeof(kParamNames) / sizeof(kParamNames[0]) == DistScreen::kParamSlots &&
                  sizeof(kParamDefaults) / sizeof(kParamDefaults[0]) == DistScreen::kParamSlots,
              "param tables out of sync");

}  // namespace

int DistScreen::param_count() const {
    return kDists[dist_].fns[fn_].nparams;
}

int DistScreen::row_count() const {
    return 2 + param_count() + 1;
}

void DistScreen::clear_result() {
    result_[0] = 0;
    expr_[0] = 0;
    msg_ = nullptr;
}

DistScreen::DistScreen() {
    for (int i = 0; i < kParamSlots; ++i) {
        vals_[i] = kParamDefaults[i];
    }
}

void DistScreen::on_activate() {
    editing_ = false;
    clear_result();
    invalidate_all();
}

void DistScreen::begin_edit(bool from_empty) {
    const FnSpec& fs = kDists[dist_].fns[fn_];
    const int pi = fs.params[row_ - 2];
    if (from_empty) {
        input_.set_text("");
    } else {
        char buf[24];
        math::format_number(vals_[pi], buf, sizeof(buf));
        input_.set_text(buf);
    }
    editing_ = true;
}

void DistScreen::commit_edit() {
    const FnSpec& fs = kDists[dist_].fns[fn_];
    const int pi = fs.params[row_ - 2];
    math::calc_t v = 0;
    if (math::eval_field(input_.text(), &v)) {
        vals_[pi] = v;
        clear_result();
    }
    editing_ = false;
}

void DistScreen::calculate() {
    const FnSpec& fs = kDists[dist_].fns[fn_];
    // Two renderings of the same call: full precision for the engine,
    // display precision for the on-screen expression.
    char call[128];
    int pos = std::snprintf(call, sizeof(call), "%s(", fs.fn_name);
    int dpos = std::snprintf(expr_, sizeof(expr_), "%s(", fs.fn_name);
    for (int i = 0; i < fs.nparams; ++i) {
        const auto v = static_cast<double>(vals_[fs.params[i]]);
        pos += std::snprintf(call + pos, sizeof(call) - static_cast<size_t>(pos), "%s%.17g",
                             i > 0 ? "," : "", v);
        char num[24];
        math::format_number(v, num, sizeof(num));
        dpos += std::snprintf(expr_ + dpos, sizeof(expr_) - static_cast<size_t>(dpos), "%s%s",
                              i > 0 ? "," : "", num);
    }
    std::snprintf(call + pos, sizeof(call) - static_cast<size_t>(pos), ")");
    std::snprintf(expr_ + dpos, sizeof(expr_) - static_cast<size_t>(dpos), ")");

    // Through the engine (same path as typing it; updates Ans like
    // TI's DISTR paste-and-run).
    const auto r = math::engine().evaluate(call);
    if (!r.ok) {
        msg_ = r.error;
        result_[0] = 0;
        return;
    }
    char num[24];
    math::format_number(r.value, num, sizeof(num));
    std::snprintf(result_, sizeof(result_), "= %s", num);
    msg_ = nullptr;
}

bool DistScreen::on_key(const platform::KeyEvent& ev) {
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
            return true;
        }
        return input_.on_key(ev);
    }

    const int rows = row_count();
    const bool on_param = row_ >= 2 && row_ < rows - 1;
    switch (ev.key) {
        case Key::kUp:
            if (row_ > 0) {
                --row_;
            }
            return true;
        case Key::kDown:
            if (row_ < rows - 1) {
                ++row_;
            }
            return true;
        case Key::kLeft:
        case Key::kRight: {
            const int dir = ev.key == Key::kRight ? 1 : -1;
            if (row_ == 0) {
                dist_ = (dist_ + dir + kDistCount) % kDistCount;
                if (fn_ >= kDists[dist_].fn_count) {
                    fn_ = kDists[dist_].fn_count - 1;
                }
                clear_result();
            } else if (row_ == 1) {
                const int n = kDists[dist_].fn_count;
                fn_ = (fn_ + dir + n) % n;
                clear_result();
            }
            // Row index can exceed the new form's row count.
            if (row_ >= row_count()) {
                row_ = row_count() - 1;
            }
            return true;
        }
        case Key::kEnter:
            if (row_ == rows - 1) {
                calculate();
            } else if (on_param) {
                begin_edit(false);
            } else {
                // Cycle forward, stats-screen style.
                const platform::KeyEvent right{Key::kRight, 0, true, false, false, false};
                on_key(right);
            }
            return true;
        case Key::kDel:
            if (on_param) {
                begin_edit(true);
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

void DistScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "DIST");

    const DistSpec& ds = kDists[dist_];
    const FnSpec& fs = ds.fns[fn_];
    const int rows = row_count();
    for (int i = 0; i < rows; ++i) {
        const int y = kTopY + i * kRowH;
        if (i == row_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH - 4,
                         platform::Color::from_rgb(0, 0, 60));
        }
        const char* label = nullptr;
        char value[28] = {};
        const bool param_edit = editing_ && i == row_ && i >= 2 && i < rows - 1;
        if (i == 0) {
            label = "Distribution";
            std::snprintf(value, sizeof(value), "%s", ds.name);
        } else if (i == 1) {
            label = "Function";
            std::snprintf(value, sizeof(value), "%s", fs.label);
        } else if (i < rows - 1) {
            label = kParamNames[fs.params[i - 2]];
            math::format_number(vals_[fs.params[i - 2]], value, sizeof(value));
        } else {
            label = "Calculate";
            std::snprintf(value, sizeof(value), "[ENTER]");
        }
        font.draw_string(fb, 12, y, label, kWhite);
        if (param_edit) {
            const int vx = platform::kScreenW / 2;
            input_.render(fb, vx, y, platform::kScreenW - vx - 12, font, true);
        } else {
            font.draw_string(fb, platform::kScreenW - 12 - font.text_width(value), y, value,
                             kGreen);
        }
    }

    // Result block below the form: the equivalent typed call + value.
    const int ry = kTopY + rows * kRowH + 8;
    if (expr_[0] != 0) {
        font.draw_string(fb, 12, ry, expr_, kGridLine);
    }
    if (result_[0] != 0) {
        font.draw_string(fb, 12, ry + 18, result_, kWhite);
    }
    if (msg_ != nullptr) {
        font.draw_string(fb, 12, ry + 18, msg_, kRed);
    }

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "L/R:CHANGE ENTER:EDIT/CALC ESC:BACK", kGrayLine);
}

DistScreen& dist_screen() {
    static DistScreen instance;
    return instance;
}

}  // namespace apps
