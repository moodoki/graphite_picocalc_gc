#include "apps/files_screen.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"
#include "ui/text_buffer.hpp"
#include "ui/text_editor_widget.hpp"
#include "apps/file_list.hpp"
#include "apps/notepad_screen.hpp"
#include "apps/program_screen.hpp"

namespace apps {

namespace {
constexpr int kTopY = 24;
constexpr int kRowH = 16;
constexpr const char* kDefaultDir = "/picocalc";

int visible_rows() {
    return (platform::kScreenH - kTopY - ui::kSoftkeyBarH - 4) / kRowH;
}

// Listing colours by entry kind (issue #45). A folder and a script are
// the two things you look for by eye; .dat is the calculator's own
// state, coloured to say "not yours to hand-edit".
platform::Color color_for(FileKind kind) {
    using namespace platform::colors;
    switch (kind) {
        case FileKind::kDir:
            return platform::Color::from_rgb(90, 170, 255);
        case FileKind::kScript:
            return kGreen;
        case FileKind::kText:
            return kWhite;
        case FileKind::kCalcData:
            return platform::Color::from_rgb(255, 190, 40);
        case FileKind::kOther:
            break;
    }
    return kGrayLine;
}

void copy_path(char* dst, const char* src) {
    std::snprintf(dst, platform::kMaxPath, "%s", src != nullptr ? src : kDefaultDir);
}

}  // namespace

void FileBrowserScreen::configure(FileBrowserMode mode, const char* start_dir,
                                  const char* ext_filter, PickedFn on_picked) {
    mode_ = mode;
    ext_filter_ = ext_filter;
    on_picked_ = on_picked;
    copy_path(start_dir_, start_dir);
    copy_path(cur_dir_, start_dir_);
    // A new caller must not inherit the previous one's half-finished
    // rename prompt or its "Deleted" status line.
    prompt_.close();
    prompt_kind_ = Prompt::kNone;
    status_[0] = 0;
    pending_move_[0] = 0;
    pending_move_is_dir_ = false;
}

int FileBrowserScreen::depth() const {
    // Levels below start_dir_, counted by separators rather than by a
    // path stack: the parent is always "truncate at the last '/'", so
    // no stack is needed to walk back up.
    int cur = 0;
    for (const char* p = cur_dir_; *p != 0; ++p) {
        if (*p == '/') {
            ++cur;
        }
    }
    int base = 0;
    for (const char* p = start_dir_; *p != 0; ++p) {
        if (*p == '/') {
            ++base;
        }
    }
    return cur - base;
}

bool FileBrowserScreen::join(const char* name, char* out, std::size_t out_len) const {
    // "/picocalc" + "/" + name. Refuse rather than silently truncate —
    // a truncated path would address the wrong file.
    const std::size_t need = std::strlen(cur_dir_) + 1 + std::strlen(name) + 1;
    if (need > out_len) {
        return false;
    }
    std::snprintf(out, out_len, "%s/%s", cur_dir_, name);
    return true;
}

void FileBrowserScreen::relist() {
    if (!platform::storage().mounted()) {
        count_ = -1;
        scroll_ = 0;
        selected_ = 0;
        return;
    }
    count_ = platform::storage().list_dir(cur_dir_, entries_, kMaxEntries);

    // Apply the extension filter by compacting in place. Directories
    // always survive it — they are how you reach the matching files.
    if (count_ > 0 && ext_filter_ != nullptr) {
        int kept = 0;
        for (int i = 0; i < count_; ++i) {
            if (entries_[i].is_dir || platform::has_ext(entries_[i].name, ext_filter_)) {
                if (kept != i) {
                    entries_[kept] = entries_[i];
                }
                ++kept;
            }
        }
        count_ = kept;
    }

    // Directories first, then by name (issue #46). FatFs hands back
    // whatever order the directory table happens to be in, which is
    // creation order — fine for four files, unusable for thirty.
    if (count_ > 1) {
        sort_entries(entries_, count_);
    }

    scroll_ = 0;
    selected_ = 0;
}

void FileBrowserScreen::descend(const char* name) {
    if (depth() >= kMaxDepth) {
        return;
    }
    char next[platform::kMaxPath];
    if (!join(name, next, sizeof(next))) {
        return;
    }
    copy_path(cur_dir_, next);
    relist();
}

bool FileBrowserScreen::ascend() {
    if (depth() <= 0) {
        return false;
    }
    char* slash = std::strrchr(cur_dir_, '/');
    if (slash == nullptr || slash == cur_dir_) {
        return false;
    }
    *slash = 0;
    relist();
    return true;
}

void FileBrowserScreen::set_status(const char* msg) {
    std::snprintf(status_, sizeof(status_), "%s", msg != nullptr ? msg : "");
    status_is_error_ = false;
}

void FileBrowserScreen::set_error(const char* msg) {
    std::snprintf(status_, sizeof(status_), "%s", msg != nullptr ? msg : "");
    status_is_error_ = true;
}

const platform::Storage::DirEntry* FileBrowserScreen::current() const {
    if (selected_ < 0 || selected_ >= count_) {
        return nullptr;
    }
    return &entries_[selected_];
}

void FileBrowserScreen::begin_move() {
    const auto* e = current();
    if (e == nullptr) {
        return;
    }
    if (!join(e->name, pending_move_, sizeof(pending_move_))) {
        pending_move_[0] = 0;
        set_error("Path too long");
        return;
    }
    pending_move_is_dir_ = e->is_dir;
    set_status("Cut");
}

void FileBrowserScreen::complete_move() {
    if (pending_move_[0] == 0) {
        return;
    }
    char dest[platform::kMaxPath];
    switch (check_move(pending_move_, pending_move_is_dir_, cur_dir_, dest, sizeof(dest))) {
        case MoveCheck::kSameFolder:
            set_error("Already here");
            return;
        case MoveCheck::kIntoItself:
            set_error("Into itself");
            return;
        case MoveCheck::kTooLong:
            set_error("Path too long");
            return;
        case MoveCheck::kBadSource:
            pending_move_[0] = 0;
            set_error("Bad source");
            return;
        case MoveCheck::kOk:
            break;
    }

    // Distinguish the two ways rename_file returns false, because they
    // need different things from the user: a name already taken here,
    // versus a source that has gone since it was cut.
    if (!platform::storage().file_exists(pending_move_)) {
        pending_move_[0] = 0;
        set_error("Source gone");
        return;
    }
    if (platform::storage().file_exists(dest)) {
        set_error("Name taken here");
        return;
    }
    if (!platform::storage().rename_file(pending_move_, dest)) {
        set_error("Move failed");
        return;
    }

    // Land the cursor on what was just moved rather than at the top —
    // the same reflex as do_delete() staying where the user was.
    char moved_name[64];
    std::snprintf(moved_name, sizeof(moved_name), "%s", basename_of(dest));
    pending_move_[0] = 0;
    relist();
    for (int i = 0; i < count_; ++i) {
        if (std::strcmp(entries_[i].name, moved_name) == 0) {
            selected_ = i;
            scroll_ = std::max(0, selected_ - visible_rows() + 1);
            break;
        }
    }
    set_status("Moved");
}

void FileBrowserScreen::open_selected() {
    const auto* e = current();
    if (e == nullptr || e->is_dir) {
        return;
    }
    const FileKind kind = classify(*e);
    if (kind == FileKind::kCalcData) {
        // .dat is the calculator's own state, written and read by the
        // firmware. Opening it in a text editor would show bytes and
        // saving would corrupt it, so this refuses rather than obliges.
        set_error("Calculator data");
        return;
    }
    if (kind != FileKind::kScript && kind != FileKind::kText) {
        set_error("No app for this");
        return;
    }

    char path[platform::kMaxPath];
    if (!join(e->name, path, sizeof(path))) {
        set_error("Path too long");
        return;
    }

    // The editor truncates silently at kCapacity and reports success,
    // so a file bigger than the buffer would open short and SAVE short,
    // losing the tail. The editor's own F3:LOAD lives with that inside
    // its own directory; opening anything on the card widens it far
    // enough to be worth refusing (elements.csv is already 5 KB).
    const long size = platform::storage().file_size(path);
    if (size > ui::TextBuffer::kCapacity) {
        set_error("Too big to edit");
        return;
    }

    // Push the app, THEN load: on_activate reconfigures the shared
    // widget and reloads that screen's own last path, which would undo
    // a load done first. The browser stays underneath, so ESC comes
    // back to the folder rather than to wherever the browser was
    // opened from.
    if (kind == FileKind::kScript) {
        program_screen().open_editor();  // never inherit a stale app mode
        ui::screen_manager().push(&program_screen());
    } else {
        ui::screen_manager().push(&notepad_screen());
    }
    ui::text_editor().load(path);
}

void FileBrowserScreen::begin_rename() {
    const auto* e = current();
    if (e == nullptr) {
        return;
    }
    prompt_kind_ = Prompt::kRename;
    prompt_.open("rename=", e->name);
}

void FileBrowserScreen::begin_new_folder() {
    prompt_kind_ = Prompt::kNewFolder;
    prompt_.open("newdir=");
}

void FileBrowserScreen::begin_delete() {
    const auto* e = current();
    if (e == nullptr) {
        return;
    }
    prompt_kind_ = Prompt::kConfirmDelete;
    // Confirm-gated (D55). A typed y/n rather than a second keypress:
    // deletion is the one irreversible action here, so it should not be
    // reachable by a repeated stray press of the same key.
    prompt_.open(e->is_dir ? "delete dir? y/n " : "delete file? y/n ");
}

void FileBrowserScreen::do_delete() {
    const auto* e = current();
    if (e == nullptr) {
        return;
    }
    char path[platform::kMaxPath];
    if (!join(e->name, path, sizeof(path))) {
        set_error("Path too long");
        return;
    }
    const bool is_dir = e->is_dir;
    const bool ok =
        is_dir ? platform::storage().delete_dir(path) : platform::storage().delete_file(path);
    if (ok) {
        const int keep = selected_;
        relist();
        // Stay where the user was rather than jumping to the top.
        selected_ = keep < count_ ? keep : (count_ > 0 ? count_ - 1 : 0);
        set_status("Deleted");
    } else {
        set_error(is_dir ? "Dir not empty" : "Delete failed");
    }
}

void FileBrowserScreen::commit_prompt() {
    const char* name = prompt_.text();

    if (prompt_kind_ == Prompt::kConfirmDelete) {
        const bool yes = name[0] == 'y' || name[0] == 'Y';
        prompt_.close();
        prompt_kind_ = Prompt::kNone;
        if (yes) {
            do_delete();
        }
        return;
    }

    if (name[0] == 0) {
        prompt_.set_error("Empty");
        return;
    }
    for (const char* p = name; *p != 0; ++p) {
        if (*p == '/' || *p == '\\' || *p == ':') {
            prompt_.set_error("Bad name");
            return;
        }
    }

    char dest[platform::kMaxPath];
    if (!join(name, dest, sizeof(dest))) {
        prompt_.set_error("Too long");
        return;
    }

    if (prompt_kind_ == Prompt::kNewFolder) {
        if (platform::storage().file_exists(dest)) {
            prompt_.set_error("Exists");
            return;
        }
        if (!platform::storage().ensure_dir(dest)) {
            prompt_.set_error("Failed");
            return;
        }
        set_status("Folder created");
    } else if (prompt_kind_ == Prompt::kRename) {
        const auto* e = current();
        if (e == nullptr) {
            prompt_.close();
            prompt_kind_ = Prompt::kNone;
            return;
        }
        char src[platform::kMaxPath];
        if (!join(e->name, src, sizeof(src))) {
            prompt_.set_error("Too long");
            return;
        }
        if (!platform::storage().rename_file(src, dest)) {
            // rename_file refuses an existing destination, which is the
            // overwhelmingly likely cause here.
            prompt_.set_error("Exists or failed");
            return;
        }
        set_status("Renamed");
    }

    prompt_.close();
    prompt_kind_ = Prompt::kNone;
    relist();
}

void FileBrowserScreen::on_activate() {
    if (cur_dir_[0] == 0) {
        // Never configured — the diagnostic's original defaults.
        configure(FileBrowserMode::kBrowse, kDefaultDir, nullptr, nullptr);
    }
    relist();
}

bool FileBrowserScreen::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return false;
    }

    // The prompt is modal and swallows everything, so a stray softkey
    // can't act on a selection while a delete is being confirmed.
    if (prompt_.active()) {
        bool submitted = false;
        bool cancelled = false;
        prompt_.on_key(ev, &submitted, &cancelled);
        if (submitted) {
            commit_prompt();
        } else if (cancelled) {
            prompt_kind_ = Prompt::kNone;
        }
        return true;
    }

    // Any key retires the previous result: it described the action
    // before this one. PromptLine does the same with a stale error.
    status_[0] = 0;

    switch (ev.key) {
        case Key::kF2:
            begin_move();
            return true;
        case Key::kF3:
            complete_move();
            return true;
        case Key::kF4:
            begin_rename();
            return true;
        case Key::kF5:
            begin_new_folder();
            return true;
        case Key::kDel:
            begin_delete();
            return true;
        case Key::kUp:
            if (selected_ > 0) {
                --selected_;
                scroll_ = std::min(scroll_, selected_);
            }
            return true;
        case Key::kDown:
            if (selected_ < count_ - 1) {
                ++selected_;
                scroll_ = std::max(scroll_, selected_ - visible_rows() + 1);
            }
            return true;
        case Key::kLeft:
        case Key::kBackspace:
            // Up one level. ESC deliberately does NOT do this — it pops
            // the screen, so the §3.3 exit convention stays uniform
            // however deep the user has navigated.
            ascend();
            return true;
        case Key::kEnter:
            if (selected_ >= 0 && selected_ < count_) {
                if (entries_[selected_].is_dir) {
                    descend(entries_[selected_].name);
                } else if (mode_ == FileBrowserMode::kPick && on_picked_ != nullptr) {
                    char path[platform::kMaxPath];
                    if (join(entries_[selected_].name, path, sizeof(path))) {
                        // Pop first so the caller's screen is on top
                        // when the callback runs — it may want to push
                        // something of its own.
                        PickedFn fn = on_picked_;
                        ui::screen_manager().pop();
                        fn(path);
                    }
                } else {
                    // kBrowse: hand the file to whatever edits it
                    // (issue #48). This was a deliberate no-op under
                    // D55; kPick is untouched, so the editor's own
                    // F3:LOAD still returns a path to its caller.
                    open_selected();
                }
            }
            return true;
        case Key::kEscape:
            ui::screen_manager().pop();
            return true;
        default:
            return false;
    }
}

void FileBrowserScreen::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();

    fb.clear(kBlack);

    char title[40];
    std::snprintf(title, sizeof(title), "%s %s", mode_ == FileBrowserMode::kPick ? "OPEN" : "FILES",
                  cur_dir_);
    ui::draw_status_bar(fb, title);

    if (count_ < 0) {
        font.draw_string(fb, 8, kTopY, platform::storage().mounted() ? "read error" : "no SD card",
                         kRed);
    } else if (count_ == 0) {
        font.draw_string(fb, 8, kTopY, ext_filter_ != nullptr ? "(no matching files)" : "(empty)",
                         kGrayLine);
    }

    // Which visible name, if any, is the one armed for a move. Resolved
    // once here rather than joining a path per row: render() runs on
    // every frame this screen is up.
    const char* armed_name = nullptr;
    if (pending_move_[0] != 0) {
        const char* base = basename_of(pending_move_);
        // base == pending_move_ means there was no '/' to split on.
        // join() always writes one, but the subtraction below would
        // underflow to a huge size_t if that ever stopped being true.
        if (base != pending_move_) {
            const auto dir_len = static_cast<std::size_t>(base - pending_move_ - 1);
            if (std::strlen(cur_dir_) == dir_len &&
                std::strncmp(pending_move_, cur_dir_, dir_len) == 0) {
                armed_name = base;
            }
        }
    }

    char line[24];
    const int first = scroll_ > 0 ? scroll_ : 0;
    for (int i = first; i < count_ && i - first < visible_rows(); ++i) {
        const int y = kTopY + (i - first) * kRowH;
        if (i == selected_) {
            fb.fill_rect(0, y - 1, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        const bool armed = armed_name != nullptr && std::strcmp(entries_[i].name, armed_name) == 0;
        if (armed) {
            // Marked for a move: dimmed, with a marker in the 8px gutter
            // the names are already indented past.
            font.draw_string(fb, 0, y, ">", kGrayLine);
        }
        font.draw_string(fb, 8, y, entries_[i].name,
                         armed ? kGrayLine : color_for(classify(entries_[i])));
        if (entries_[i].is_dir) {
            std::snprintf(line, sizeof(line), "[DIR]");
        } else {
            format_size(entries_[i].size, line, sizeof(line));
        }
        font.draw_string(fb, platform::kScreenW - font.text_width(line) - 8, y, line, kGrayLine);
    }

    const int bar_y = platform::kScreenH - ui::kSoftkeyBarH;
    if (prompt_.active()) {
        fb.fill_rect(0, bar_y, platform::kScreenW, ui::kSoftkeyBarH,
                     platform::Color::from_rgb(30, 30, 30));
        prompt_.render(fb, 2, bar_y + 2, platform::kScreenW - 4, font);
        return;
    }

    // MOVE appears only while something is armed, which is what tells
    // the user a cut is still pending after walking to another folder.
    // Both labels are within draw_softkeys' 6-character cell (#52).
    const char* const keys[6] = {"",    "CUT",   pending_move_[0] != 0 ? "MOVE" : "",
                                 "REN", "MKDIR", ""};
    ui::draw_softkeys(fb, keys);

    // Position counter in the (unbound) F6 cell. It used to be drawn at
    // y=2, which is inside the 16px status bar the chrome right-aligns
    // the battery and RAD/FLT into — a straight overlap that only shows
    // up in a directory of more than visible_rows() entries (issue #44).
    int status_right = platform::kScreenW - 2;
    if (count_ > visible_rows()) {
        std::snprintf(line, sizeof(line), "%d/%d", selected_ + 1, count_);
        const int cx = platform::kScreenW - font.text_width(line) - 6;
        font.draw_string(fb, cx, bar_y + 4, line, kGrayLine);
        status_right = cx - 4;
    }
    if (status_[0] != 0) {
        // Transient text sits to the left of the counter, on a repainted
        // strip: it is wider than one softkey cell and would otherwise
        // print over the REN/MKDIR labels it overlaps.
        const int sx = status_right - font.text_width(status_);
        fb.fill_rect(sx - 2, bar_y, status_right - sx + 4, ui::kSoftkeyBarH,
                     platform::Color::from_rgb(30, 30, 30));
        font.draw_string(fb, sx, bar_y + 4, status_, status_is_error_ ? kRed : kGreen);
    }
}

FileBrowserScreen& file_browser() {
    static FileBrowserScreen instance;
    return instance;
}

FileBrowserScreen& browse_files(const char* start_dir) {
    FileBrowserScreen& s = file_browser();
    s.configure(FileBrowserMode::kBrowse, start_dir, nullptr, nullptr);
    return s;
}

FileBrowserScreen& pick_file(const char* start_dir, const char* ext_filter,
                             FileBrowserScreen::PickedFn on_picked) {
    FileBrowserScreen& s = file_browser();
    s.configure(FileBrowserMode::kPick, start_dir, ext_filter, on_picked);
    return s;
}

}  // namespace apps
