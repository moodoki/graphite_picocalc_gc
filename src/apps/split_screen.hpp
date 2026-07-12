#pragma once

#include "ui/screen.hpp"

namespace apps {

// Split-screen graph|table (tasks 2.19-2.21, spec §8; decisions D16):
// horizontal split — graph pane on top (full 320px width, so the
// column caches and trace x-mapping stay identical to full screen),
// table pane below. Reuses the live GraphScreen/TableScreen singletons
// with pane clip rects; no duplicated caches or forked state.
//
// Keys: F4 switches pane focus; F9/ESC exits back to the previous
// screen; everything else routes to the focused pane. Trace sync is
// nearest-row (2.20 option c): graph trace highlights the closest
// table row; table selection places the graph trace cursor.
class SplitScreen : public ui::Screen {
public:
    void on_activate() override;
    void on_deactivate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    // Panes: graph rows [0, 138), divider [138, 142), table [142, 300).
    static constexpr int kGraphH = 138;
    static constexpr int kDividerY = 138;
    static constexpr int kTableY = 142;
    static constexpr int kTableH = 158;

    bool graph_focused_ = true;

    void sync_panes() const;
};

SplitScreen& split_screen();

}  // namespace apps
