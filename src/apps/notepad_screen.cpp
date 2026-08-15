#include "apps/notepad_screen.hpp"

#include "ui/screen_manager.hpp"
#include "ui/text_editor_widget.hpp"
#include "apps/files_screen.hpp"

namespace apps {

namespace {

constexpr const char* kNotesDir = "/picocalc/notes";
constexpr const char* kNotesExt = ".txt";

ui::TextEditorConfig make_config() {
    ui::TextEditorConfig cfg;
    cfg.title = "NOTE";
    cfg.save_dir = kNotesDir;
    cfg.file_ext = kNotesExt;
    cfg.auto_indent_after = 0;  // plain text — 6B's Python editor sets ':'
    cfg.has_run_key = false;    // nothing to run (D54)
    cfg.on_run = nullptr;
    return cfg;
}

// The picker hands the chosen path straight to the shared widget.
// Captureless so it converts to FileBrowserScreen::PickedFn, and it can
// reach the widget without state because there is exactly one.
void on_note_picked(const char* path) {
    ui::text_editor().load(path);
}

}  // namespace

void NotepadScreen::on_activate() {
    // Re-configure on every activation: coming back from the file
    // picker lands here, and configure() keeps the buffer when the
    // owner is unchanged.
    ui::text_editor().configure(make_config(), this);
}

bool NotepadScreen::on_key(const platform::KeyEvent& ev) {
    switch (ui::text_editor().on_key(ev)) {
        case ui::EditorAction::kExit:
            // ESC lands on the launcher, because that is what pushed
            // us (§3.3) — no special routing needed here.
            ui::screen_manager().pop();
            return true;
        case ui::EditorAction::kLoad:
            ui::screen_manager().push(&pick_file(kNotesDir, kNotesExt, on_note_picked));
            return true;
        case ui::EditorAction::kConsumed:
            invalidate_all();
            return true;
        case ui::EditorAction::kNone:
            return false;
    }
    return false;
}

void NotepadScreen::render(gfx::Framebuffer& fb) {
    ui::text_editor().render(fb);
}

NotepadScreen& notepad_screen() {
    static NotepadScreen instance;
    return instance;
}

}  // namespace apps
