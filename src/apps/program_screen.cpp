#include "apps/program_screen.hpp"

#include <cstdio>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/output_log.hpp"
#include "ui/screen_manager.hpp"
#include "ui/text_editor_widget.hpp"
#include "apps/files_screen.hpp"
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
    // Re-configured on every activation: coming back from the file
    // picker lands here, and configure() keeps the buffer when the owner
    // is unchanged.
    ui::text_editor().configure(make_config(), this);
    // Lazy bring-up (D57/D72, 6B.14). The heap bytes are static and
    // always reserved; what this avoids is interpreter state existing
    // for a user who never opens a program.
    scripting::python().init();
}

int ProgramScreen::visible_rows() const {
    const int bottom = platform::kScreenH - ui::kSoftkeyBarH - 4;
    return (bottom - kTextTop) / gfx::main_font().height();
}

void ProgramScreen::run_current() {
    g_log.clear();
    top_line_ = 0;
    showing_output_ = true;

    if (!scripting::python().init()) {
        last_run_ok_ = false;
        std::snprintf(header_, sizeof(header_), "interpreter unavailable");
        return;
    }

    scripting::python().set_output_callback(&log_output);
    last_run_ok_ = scripting::python().exec(ui::text_editor().text());
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
}

bool ProgramScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;

    if (showing_output_) {
        switch (ev.key) {
            case Key::kEscape:
            case Key::kF1:
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
            invalidate_all();
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
    ui::draw_status_bar(fb, "PY OUTPUT");

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

    const char* const keys[6] = {"EDIT", "", "", "", "", ""};
    ui::draw_softkeys(fb, keys);
}

void ProgramScreen::render(gfx::Framebuffer& fb) {
    if (showing_output_) {
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
