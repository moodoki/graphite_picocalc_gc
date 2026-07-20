#include "apps/calc_menu.hpp"

#include <cstdio>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "apps/graph_screen.hpp"
#include "graph/analysis_cursor.hpp"

namespace apps {

namespace {

constexpr int kTopY = 44;
constexpr int kRowH = 28;
constexpr int kSoftkeyY = 300;

constexpr graph::AnalysisOp kOps[graph::kAnalysisOpCount] = {
    graph::AnalysisOp::kValue,    graph::AnalysisOp::kZero,      graph::AnalysisOp::kMinimum,
    graph::AnalysisOp::kMaximum,  graph::AnalysisOp::kIntersect, graph::AnalysisOp::kDerivative,
    graph::AnalysisOp::kIntegral,
};

}  // namespace

void CalcMenuScreen::on_activate() {
    // Keep sel_ across visits (repeat analyses of the same kind are
    // common); nothing else to reset.
}

void CalcMenuScreen::select(graph::AnalysisOp op) {
    // Pop first: the graph's on_activate runs (clearing any stale
    // session), then the new session starts on top of that.
    ui::screen_manager().pop();
    graph_screen().begin_analysis(op);
}

bool CalcMenuScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }
    switch (ev.key) {
        case Key::kUp:
            sel_ = (sel_ + graph::kAnalysisOpCount - 1) % graph::kAnalysisOpCount;
            invalidate_all();
            return true;
        case Key::kDown:
            sel_ = (sel_ + 1) % graph::kAnalysisOpCount;
            invalidate_all();
            return true;
        case Key::kEnter:
            select(kOps[sel_]);
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            if (ev.ch >= '1' && ev.ch <= '0' + graph::kAnalysisOpCount) {
                sel_ = ev.ch - '1';
                select(kOps[sel_]);
                return true;
            }
            return false;
    }
}

void CalcMenuScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);
    ui::draw_status_bar(fb, "ANALYZE");

    for (int i = 0; i < graph::kAnalysisOpCount; ++i) {
        const int y = kTopY + i * kRowH;
        if (i == sel_) {
            fb.fill_rect(0, y - 4, platform::kScreenW, kRowH - 4,
                         platform::Color::from_rgb(0, 0, 60));
        }
        char line[32];
        std::snprintf(line, sizeof(line), "%d: %s", i + 1, graph::analysis_op_name(kOps[i]));
        font.draw_string(fb, 24, y, line, i == sel_ ? kWhite : kGrayLine);
    }

    fb.fill_rect(0, kSoftkeyY, platform::kScreenW, 20, platform::Color::from_rgb(30, 30, 30));
    font.draw_string(fb, 2, kSoftkeyY + 4, "1-7/ENTER:SELECT ESC:BACK", kGrayLine);
}

CalcMenuScreen& calc_menu() {
    static CalcMenuScreen instance;
    return instance;
}

}  // namespace apps
