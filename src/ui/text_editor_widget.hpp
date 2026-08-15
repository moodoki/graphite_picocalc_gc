#pragma once

#include "platform/keyboard.hpp"
#include "platform/storage.hpp"
#include "gfx/framebuffer.hpp"
#include "ui/prompt_line.hpp"
#include "ui/text_buffer.hpp"

namespace ui {

// Everything app-specific about the editor (spec §3.5). Notepad
// (§3.6) and 6B's Python program editor (§4.3) differ only in this.
// Every read site tolerates null, so a consumer only sets what it
// cares about.
struct TextEditorConfig {
    const char* title = "EDIT";                  // status bar
    const char* save_dir = "/picocalc";          // e.g. "/picocalc/notes"
    const char* file_ext = ".txt";               // e.g. ".py"
    char auto_indent_after = 0;                  // 0 = disabled; ':' for Python
    bool has_run_key = false;                    // F1:RUN shown and wired only if true
    void (*on_run)(const char* path) = nullptr;  // called with the saved path
};

// Shared line-numbered text editor (Phase 6A.5, spec §3.5). Owns the
// buffer, cursor, scrolling, softkey row, and load/save; the screens
// that use it are thin wrappers that configure it and forward keys.
//
// ONE SHARED INSTANCE, not one per screen (see text_editor()). The
// buffer is ~4.6 KB and only one editor is ever in front of the user,
// so duplicating it per consumer is a poor trade on the Pico 1. The
// consequence: pushing a second editor screen while the first is still
// on the stack takes the buffer with it. configure() detects the owner
// change and reloads from that owner's own path, so the visible
// behaviour is "your file comes back", but unsaved edits in the
// displaced editor are lost — hence the unsaved-changes guard on ESC.
// What the owning screen must do after the widget has looked at a key.
// The widget never touches the screen stack itself — it has no business
// knowing whether it is in Notepad or the Python editor.
enum class EditorAction : std::uint8_t {
    kNone,      // not consumed; the screen may handle the key itself
    kConsumed,  // handled internally, nothing for the screen to do
    kExit,      // user wants out — the screen pops
    kLoad,      // F3 — the screen opens the file browser in kPick mode
};

class TextEditorWidget {
public:
    // `owner` identifies the calling screen (pass `this`). A change of
    // owner resets the buffer and reloads from that owner's last path.
    void configure(const TextEditorConfig& cfg, const void* owner);

    EditorAction on_key(const platform::KeyEvent& ev);

    void render(gfx::Framebuffer& fb);

    // Load replaces the buffer and takes `path` as the save target.
    bool load(const char* path);
    // Save to the current path. With no path yet, opens the name
    // prompt and returns false (nothing written).
    bool save();

    bool dirty() const { return buf_.dirty(); }
    const char* path() const { return path_; }

private:
    // Layout: 8x16 font, so 40 columns across a 320 px screen. Four go
    // to the line-number gutter.
    static constexpr int kGutterChars = 4;

    TextBuffer buf_;
    TextEditorConfig cfg_;
    const void* owner_ = nullptr;

    char path_[platform::kMaxPath] = {};
    char status_[40] = {};  // transient "Saved" / error text

    int top_line_ = 0;  // first visible row
    int left_col_ = 0;  // horizontal scroll, for long lines

    PromptLine prompt_;
    // Press-ESC-again guard. A modal yes/no dialog would be heavier
    // than this is worth: the first ESC on a dirty buffer just says so
    // in the status line, the second leaves.
    bool exit_armed_ = false;

    void set_status(const char* msg);
    void scroll_into_view();
    void begin_save_as();
    // Joins save_dir + "/" + name, appending file_ext when absent.
    bool compose_path(const char* name, char* out, std::size_t out_len) const;
    bool write_current();
    void handle_prompt_submit();
    int visible_rows() const;
    int visible_cols() const;
};

// The single shared instance — see the class comment for why.
TextEditorWidget& text_editor();

}  // namespace ui
