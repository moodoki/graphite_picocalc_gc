#include "apps/program_screen.hpp"

#include <cstdio>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/output_log.hpp"
#include "ui/screen_manager.hpp"
#include "ui/text_editor_widget.hpp"
#include "apps/files_screen.hpp"
#include "apps/graph_screen.hpp"
#include "scripting/calc_api.h"
#include "scripting/micropython_embed.hpp"

namespace apps {

namespace {

constexpr const char* kProgramsDir = "/picocalc/programs";
constexpr const char* kProgramsExt = ".py";

// Python's own indent step. Two rather than four because the editor is
// 36 usable columns wide once the line-number gutter is taken out, and
// four columns of indent costs an eighth of the line at every level.
constexpr int kIndentWidth = 2;

constexpr int kTextTop = ui::kStatusBarH + 4;

ui::OutputLog g_log;

ui::TextEditorConfig make_config() {
    ui::TextEditorConfig cfg;
    cfg.title = "PY";
    cfg.save_dir = kProgramsDir;
    cfg.file_ext = kProgramsExt;
    cfg.auto_indent_after = ':';
    cfg.indent_width = kIndentWidth;
    cfg.has_run_key = true;
    // Captureless, so it converts to the plain function pointer the
    // config holds; it can reach the screen without state because there
    // is exactly one, same as Notepad's picker callback.
    cfg.on_run = [](const char*) { program_screen().run_current(); };
    return cfg;
}

void on_script_picked(const char* path) {
    ui::text_editor().load(path);
}

void log_output(const char* text, std::size_t len) {
    g_log.append(text, len);
}

}  // namespace

void ProgramScreen::on_activate() {
    // Lazy bring-up (D57/D72, 6B.14). The heap bytes are static and
    // always reserved; what this avoids is interpreter state existing
    // for a user who never opens a program.
    scripting::python().init();

    if (app_mode_) {
        // Deliberately NOT configure()d: an SD app must not touch the
        // editor's buffer, so a user who was halfway through a script
        // still has it when they come back.
        //
        // Running here rather than in queue_app() is what makes
        // calc.show_graph() work from an SD app: finish_run() pushes the
        // graph screen, and pushing from inside our caller's push would
        // nest screen management inside itself.
        if (pending_app_ != nullptr) {
            const char* path = pending_app_;
            pending_app_ = nullptr;
            if (prepare_run()) {
                last_run_ok_ = scripting::python().exec_file(path);
                finish_run();
            }
        }
        // No pending app means we are being re-activated — the app
        // pushed a graph and the user came back from it. The output
        // pane is still what should be on screen.
        return;
    }

    // Re-configured on every activation: coming back from the file
    // picker lands here, and configure() keeps the buffer when the owner
    // is unchanged.
    ui::text_editor().configure(make_config(), this);
}

void ProgramScreen::queue_app(const char* path, const char* name) {
    app_mode_ = true;
    pending_app_ = path;
    app_name_ = name;
}

void ProgramScreen::open_editor() {
    app_mode_ = false;
    canvas_mode_ = false;
    showing_output_ = false;
    pending_app_ = nullptr;
    app_name_ = nullptr;
    set_dirty_tracking(false);
}

void ProgramScreen::exit_app() {
    app_mode_ = false;
    canvas_mode_ = false;
    showing_output_ = false;
    pending_app_ = nullptr;
    app_name_ = nullptr;
    set_dirty_tracking(false);
    // 6B.14, same as leaving the editor: the runtime comes down so the
    // next app starts from a fully-free heap. The bytes stay reserved
    // either way (D72).
    scripting::python().shutdown();
    ui::screen_manager().pop();
}

int ProgramScreen::visible_rows() const {
    const int bottom = platform::kScreenH - ui::kSoftkeyBarH - 4;
    return (bottom - kTextTop) / gfx::main_font().height();
}

bool ProgramScreen::prepare_run() {
    g_log.clear();
    top_line_ = 0;
    showing_output_ = true;

    if (!scripting::python().init()) {
        last_run_ok_ = false;
        std::snprintf(header_, sizeof(header_), "interpreter unavailable");
        return false;
    }
    scripting::python().set_output_callback(&log_output);
    return true;
}

void ProgramScreen::run_current() {
    if (!prepare_run()) {
        return;
    }
    last_run_ok_ = scripting::python().exec(ui::text_editor().text());
    finish_run();
}

void ProgramScreen::finish_run() {
    scripting::python().set_output_callback(nullptr);

    std::snprintf(header_, sizeof(header_), "%s   heap %u free%s", last_run_ok_ ? "done" : "raised",
                  static_cast<unsigned>(scripting::python().heap_free()),
                  g_log.truncated() ? "  (trimmed)" : "");

    // A failed run scrolls to the END, where the traceback is; a clean
    // one stays at the top, where a table or a report starts. Neither
    // choice is right for both, and the failing case is the one where
    // the user cannot get the information any other way.
    if (!last_run_ok_) {
        const int over = g_log.line_count() - visible_rows();
        top_line_ = over > 0 ? over : 0;
    }

    // calc.show_graph() (6B.6) is deferred to here rather than switching
    // screens from inside the binding: a binding runs inside the VM, inside
    // this on_key, so an immediate push would nest screen management inside
    // itself and still render nothing until the script returned. Same shape
    // as 6B.12 buffering output instead of streaming it.
    //
    // Only on a clean run. A script that raised has a traceback the user
    // needs to read, and hiding it behind a graph would be the wrong call
    // even though the plot commands before the failure did take effect.
    if (last_run_ok_ && calc_api_take_show_graph() != 0) {
        ui::screen_manager().push(&graph_screen());
        return;
    }

    // A script that drew (6B.8, D80) has already put its pixels on the panel,
    // outside the render path. Showing the output pane over them would erase
    // the thing the script produced, so this screen goes quiet instead: dirty
    // tracking on, nothing marked, which makes render_frame skip the frame
    // outright. ESC in on_key gives the panel back.
    //
    // Even on a failed run — the traceback went to serial, and a half-drawn
    // canvas is more informative than the pane replacing it. `py` at the home
    // screen still prints the error.
    if (calc_api_canvas_owns_display() != 0) {
        canvas_mode_ = true;
        showing_output_ = false;
        set_dirty_tracking(true);
    }
}

bool ProgramScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;

    // While a script's canvas is up this screen draws nothing, so the only
    // key that means anything is the one that takes the panel back.
    if (canvas_mode_) {
        if (ev.pressed && ev.key == Key::kEscape) {
            if (app_mode_) {
                exit_app();
                return true;
            }
            canvas_mode_ = false;
            set_dirty_tracking(false);
            invalidate_all();
        }
        return true;
    }

    if (showing_output_) {
        switch (ev.key) {
            case Key::kEscape:
            case Key::kF1:
                // §3.3: from an SD app, ESC goes back to the launcher.
                // The editor underneath is not this app's — the user
                // never opened it, and it may hold unrelated work.
                if (app_mode_) {
                    exit_app();
                    return true;
                }
                showing_output_ = false;
                invalidate_all();
                return true;
            case Key::kUp:
                if (top_line_ > 0) {
                    --top_line_;
                    invalidate_all();
                }
                return true;
            case Key::kDown: {
                const int over = g_log.line_count() - visible_rows();
                if (over > 0 && top_line_ < over) {
                    ++top_line_;
                    invalidate_all();
                }
                return true;
            }
            default:
                // Nothing else should reach the editor underneath — a
                // stray keystroke must not silently edit the script the
                // user is reading output for.
                return true;
        }
    }

    switch (ui::text_editor().on_key(ev)) {
        case ui::EditorAction::kExit:
            // 6B.14: the runtime comes down on the way out, so a second
            // visit starts from a fully-free heap. The bytes stay
            // reserved either way (D72).
            scripting::python().shutdown();
            ui::screen_manager().pop();
            return true;
        case ui::EditorAction::kLoad:
            ui::screen_manager().push(&pick_file(kProgramsDir, kProgramsExt, on_script_picked));
            return true;
        case ui::EditorAction::kConsumed:
            // NOT when the key that was consumed was RUN and the script took
            // the panel: the editor would repaint straight over the canvas it
            // just drew. Found on hardware 2026-08-16 — the drawing appeared
            // and was immediately erased, and because canvas_mode_ was
            // correctly on underneath, keys stayed dead until ESC.
            if (!canvas_mode_) {
                invalidate_all();
            }
            return true;
        case ui::EditorAction::kNone:
            return false;
    }
    return false;
}

void ProgramScreen::render_output(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();
    const int fh = font.height();
    const int fw = font.width();

    fb.clear(kBlack);
    // An SD app is titled with its own name, so the pane confirms which
    // tile actually ran — the one thing the launcher cannot show.
    ui::draw_status_bar(fb, app_mode_ && app_name_ != nullptr ? app_name_ : "PY OUTPUT");

    font.draw_string(fb, 2, kTextTop, header_, last_run_ok_ ? kGreen : kRed);

    const int rows = visible_rows() - 1;  // the header takes one
    const int cols = platform::kScreenW / fw;
    for (int r = 0; r < rows; ++r) {
        const int i = top_line_ + r;
        if (i >= g_log.line_count()) {
            break;
        }
        const int y = kTextTop + (r + 1) * fh;
        // Lines in the flat log are not NUL-terminated, so they are
        // drawn a character at a time rather than handed to
        // draw_string. Over-long lines are clipped, not wrapped.
        const char* src = g_log.line(i);
        const int n = g_log.line_length(i);
        for (int c = 0; c < cols && c < n; ++c) {
            font.draw_char(fb, c * fw, y, src[c], kWhite);
        }
    }

    const char* const keys[6] = {app_mode_ ? "BACK" : "EDIT", "", "", "", "", ""};
    ui::draw_softkeys(fb, keys);
}

void ProgramScreen::render(gfx::Framebuffer& fb) {
    // App mode never falls through to the editor: it was never
    // configure()d for this app, and its buffer belongs to the user's
    // own work. The output pane is all an SD app has.
    if (showing_output_ || app_mode_) {
        render_output(fb);
        return;
    }
    ui::text_editor().render(fb);
}

ProgramScreen& program_screen() {
    static ProgramScreen instance;
    return instance;
}

}  // namespace apps
