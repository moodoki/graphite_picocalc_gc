#pragma once

#include "ui/input_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// Stat-plot setup (task 3D.13, D27): configures the three StatPlot
// slots (graph::GraphState::stat_plots) — TI's Plot1-3. Entered via
// the typed `plot` command. Rows: slot selector, enable, type, source
// lists, mark, histogram bin width (numeric InputLine; 0 = auto).
// Every change saves graph state; the graph screen draws the enabled
// plots and 'Z' there runs ZoomStat.
class PlotScreen : public ui::Screen {
public:
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;
    void on_activate() override;

private:
    int slot_ = 0;  // Plot1..Plot3
    int row_ = 0;
    bool editing_ = false;
    ui::InputLine input_;

    int row_count() const;
    void adjust(int row, int dir);
};

PlotScreen& plot_screen();

}  // namespace apps
