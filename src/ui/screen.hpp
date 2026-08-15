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

    // True when this screen has handed the whole panel to something that
    // draws outside the render path — currently only a Python script's
    // canvas (6B.8, D80). The main loop consults it before repainting
    // chrome on its own clock: a script that owns the screen must not get
    // the status bar drawn over its output a minute later.
    virtual bool owns_display() const { return false; }

protected:
    // Opt in to partial redraws. A tracking screen must invalidate() every
    // row band its on_key changes — unmarked rows are neither re-rendered
    // nor pushed, and a screen that marks nothing is not rendered at all
    // (ScreenManager::render_frame returns early on an empty band).
    //
    // Normally called once from a constructor. It is two-way because that
    // empty-band case is exactly how a script's canvas survives: ProgramScreen
    // turns tracking ON while a script owns the panel, marks nothing, and
    // turns it OFF again on the way out. Leaving it on would mean every
    // screen state change had to name its own rows, which ProgramScreen does
    // not do.
    // Turning tracking ON also clears the pending band. It has to: while
    // tracking was off, take_dirty() reset the band to the FULL SCREEN every
    // frame, so a screen that switches on and then marks nothing would still
    // inherit one last full repaint — which on hardware (2026-08-16) painted
    // the editor straight over the canvas a script had just drawn. Switching
    // to tracking means "I name my own rows from here", and that starts now.
    void set_dirty_tracking(bool on) {
        tracks_dirty_ = on;
        if (on) {
            dirty_y0_ = 0;
            dirty_y1_ = 0;
        }
    }
    void track_dirty() { set_dirty_tracking(true); }

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
