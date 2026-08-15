#pragma once

#include "ui/screen.hpp"

namespace apps {

// Python program editor and runner (Phase 6B.11-6B.14, spec §4.3).
//
// A thin ui::TextEditorWidget wrapper, the same shape as
// NotepadScreen: .py under /picocalc/programs/, auto-indent after ':',
// and F1:RUN wired to the interpreter. Registered with AppRegistry as
// "Python".
//
// The output pane is a second render MODE on this screen rather than a
// screen of its own, so ESC keeps meaning one thing at each depth:
// output -> editor -> launcher.
//
// Output is buffered and shown when the run ENDS, not streamed. While a
// script runs we are inside on_key and nothing renders at all —
// streaming would need the display pumped from the output callback, and
// that belongs with whatever first needs it (a long-running sensor log,
// most likely) rather than here.
// A script that draws (6B.8, D80) takes this further: it owns the whole
// panel until it ends. Its pixels went straight to the display, outside the
// render path, so this screen must then repaint NOTHING — it turns dirty
// tracking on and marks no rows, which makes ScreenManager::render_frame skip
// the frame entirely, and reports owns_display() so the main loop leaves the
// status bar alone too. ESC gives the panel back.
class ProgramScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;
    bool owns_display() const override { return canvas_mode_; }

    // Runs the editor's current text. Called through the widget's
    // on_run hook once it has saved, so what runs and what is on disk
    // agree.
    void run_current();

private:
    bool showing_output_ = false;
    bool canvas_mode_ = false;
    int top_line_ = 0;

    // Cached in run_current(), never built in render() — render() is
    // called once per 8-px strip, 40 times a frame on the Pico 1 (D47).
    char header_[48] = {};
    bool last_run_ok_ = true;

    int visible_rows() const;
    void render_output(gfx::Framebuffer& fb);
};

ProgramScreen& program_screen();

}  // namespace apps
