#pragma once

#include <cstddef>
#include <cstdint>

#include "platform/storage.hpp"
#include "ui/prompt_line.hpp"
#include "ui/screen.hpp"

namespace apps {

// SD file browser (Phase 6A.6, spec §3.7, D55 — generalized in place
// from the read-only single-level FilesScreen added 2026-07-18).
//
// One component, two modes:
//   kBrowse — the standalone `files` diagnostic. ENTER on a file is a
//             no-op (view only), preserving the original behavior.
//   kPick   — a caller wants a path back: ENTER on a file invokes
//             on_picked and pops. Used by the text editor's F3:LOAD.
//
// Both modes navigate directories. Descent is capped (kMaxDepth) below
// start_dir so the path buffer has a real bound; /picocalc/apps/<name>
// is the deepest thing this SD layout defines, at 2 levels.
enum class FileBrowserMode : std::uint8_t { kBrowse, kPick };

class FileBrowserScreen : public ui::Screen {
public:
    using PickedFn = void (*)(const char* path);

    // Deepest descent below start_dir. 4 leaves one spare level of
    // user-created subfolder past the deepest path the layout defines.
    static constexpr int kMaxDepth = 4;

    // Applies immediately and resets navigation to start_dir. Call
    // before pushing, not after — on_activate() re-lists from whatever
    // is configured.
    void configure(FileBrowserMode mode, const char* start_dir, const char* ext_filter,
                   PickedFn on_picked);

    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    static constexpr int kMaxEntries = 32;

    platform::Storage::DirEntry entries_[kMaxEntries] = {};
    int count_ = 0;   // -1 = list failed / no card
    int scroll_ = 0;  // First visible entry
    int selected_ = 0;

    FileBrowserMode mode_ = FileBrowserMode::kBrowse;
    PickedFn on_picked_ = nullptr;
    const char* ext_filter_ = nullptr;  // e.g. ".txt"; null = show everything

    char start_dir_[platform::kMaxPath] = {};  // floor for ascending
    char cur_dir_[platform::kMaxPath] = {};

    // ---- Move, as cut-and-paste (issue #47) ----
    //
    // The full source path, empty when nothing is armed. Armed state
    // survives navigating and opening a file, because walking to the
    // destination folder IS the gesture — it does not survive
    // configure(), so a new caller never inherits someone else's
    // pending move.
    char pending_move_[platform::kMaxPath] = {};
    bool pending_move_is_dir_ = false;

    void begin_move();     // F2: arm the selection
    void complete_move();  // F3: move it into the current folder

    // ---- Open in the associated app (issue #48) ----
    //
    // ENTER on a file in kBrowse mode used to be a deliberate no-op
    // (D55, "view only"). It now hands .py to the Python editor and
    // text to Notepad, and says why for the kinds it will not open.
    void open_selected();

    // ---- Management (6A.7, D55), available in both modes ----
    ui::PromptLine prompt_;
    enum class Prompt : std::uint8_t { kNone, kRename, kNewFolder, kConfirmDelete };
    Prompt prompt_kind_ = Prompt::kNone;
    char status_[40] = {};  // transient result/error text
    bool status_is_error_ = false;

    void relist();
    void descend(const char* name);
    bool ascend();  // false when already at start_dir
    int depth() const;
    // Joins cur_dir_ + "/" + name into out. False if it wouldn't fit.
    bool join(const char* name, char* out, std::size_t out_len) const;

    // Transient feedback on the softkey bar. Green says it happened,
    // red says it did not — the same split PromptLine already draws,
    // and the reason a refusal like "Too big to edit" should not look
    // like a report of success. Either is cleared by the next keypress.
    void set_status(const char* msg);
    void set_error(const char* msg);
    const platform::Storage::DirEntry* current() const;
    void begin_rename();
    void begin_new_folder();
    void begin_delete();
    void commit_prompt();
    void do_delete();
};

// The single shared instance. Only one browser is ever in front of the
// user, so the 2.3 KB entry table is not duplicated per call site.
FileBrowserScreen& file_browser();

// Configure-and-return helpers, so push() call sites stay one line.
FileBrowserScreen& browse_files(const char* start_dir = "/picocalc");
FileBrowserScreen& pick_file(const char* start_dir, const char* ext_filter,
                             FileBrowserScreen::PickedFn on_picked);

// Back-compat alias for the `files` typed command and the launcher's
// Files entry, both of which want the diagnostic's original defaults.
inline FileBrowserScreen& files_screen() {
    return browse_files();
}

}  // namespace apps
