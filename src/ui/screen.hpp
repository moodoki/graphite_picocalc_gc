#pragma once

#include "platform/keyboard.hpp"
#include "gfx/framebuffer.hpp"

namespace ui {

class Screen {
public:
    virtual ~Screen() = default;

    // Called when this screen becomes top of the stack.
    virtual void on_activate() {}
    // Called when popped or covered.
    virtual void on_deactivate() {}

    // Handle a key event. Return true if consumed.
    virtual bool on_key(const platform::KeyEvent& ev) = 0;

    // Render the full frame. In strip mode this is called once per strip;
    // use fb.clip_y0()/clip_y1() to skip out-of-strip work if profiling
    // demands it, otherwise just draw everything.
    virtual void render(gfx::Framebuffer& fb) = 0;

    // ---- Dirty-band partial redraw (task 5.6 part 2) ----
    // The SPI push dominates frame time (~200 ms full screen), so the
    // manager only re-renders and pushes the rows a screen marks dirty.
    // Screens opt in with track_dirty(); everyone else gets full frames.

    // Consume the rows to redraw as [y0, y1). Tracking screens hand over
    // the accumulated band and reset to empty; others always report the
    // full screen.
    void take_dirty(int& y0, int& y1) {
        y0 = dirty_y0_;
        y1 = dirty_y1_;
        dirty_y0_ = 0;
        dirty_y1_ = tracks_dirty_ ? 0 : platform::kScreenH;
    }

    // Force a full redraw (ScreenManager calls this whenever a screen
    // becomes top of the stack).
    void invalidate_all() { invalidate(0, platform::kScreenH); }

    // External band invalidation for chrome the main loop refreshes on
    // its own clock (battery/status bar) — same as invalidate() but
    // callable from outside the screen.
    void invalidate_band(int y0, int y1) { invalidate(y0, y1); }

protected:
    // Opt in to partial redraws (call from the constructor). A tracking
    // screen must invalidate() every row band its on_key changes —
    // unmarked rows are neither re-rendered nor pushed.
    void track_dirty() { tracks_dirty_ = true; }

    // Mark rows [y0, y1) as needing redraw (unioned into the pending band).
    void invalidate(int y0, int y1) {
        if (y1 <= y0) {
            return;
        }
        if (dirty_y0_ >= dirty_y1_) {  // Currently empty
            dirty_y0_ = y0;
            dirty_y1_ = y1;
            return;
        }
        if (y0 < dirty_y0_) {
            dirty_y0_ = y0;
        }
        if (y1 > dirty_y1_) {
            dirty_y1_ = y1;
        }
    }

private:
    bool tracks_dirty_ = false;
    int dirty_y0_ = 0;
    int dirty_y1_ = platform::kScreenH;
};

}  // namespace ui
