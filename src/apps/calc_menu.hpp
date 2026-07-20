#pragma once

#include "ui/screen.hpp"
#include "graph/analysis.hpp"

namespace apps {

// Graph-analysis (CALC) menu — Phase 4B, phase4-spec §4.4. Pushed from
// the graph screen (F6) or the typed `calc` command; picking an
// operation pops back to the graph and starts the interactive
// analysis session there.
class CalcMenuScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    int sel_ = 0;

    void select(graph::AnalysisOp op);
};

CalcMenuScreen& calc_menu();

}  // namespace apps
