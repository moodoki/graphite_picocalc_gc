#include "apps/files_screen.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"
#include "ui/screen_manager.hpp"

namespace apps {

namespace {
constexpr int kTopY = 24;
constexpr int kRowH = 16;
constexpr const char* kDefaultDir = "/picocalc";

int visible_rows() {
    return (platform::kScreenH - kTopY - ui::kSoftkeyBarH - 4) / kRowH;
}

// Case-insensitive suffix match — FAT is case-preserving but not
// case-sensitive, so a ".TXT" on the card must still match ".txt".
bool has_ext(const char* name, const char* ext) {
    const std::size_t n = std::strlen(name);
    const std::size_t e = std::strlen(ext);
    if (e == 0 || n < e) {
        return false;
    }
    const char* tail = name + (n - e);
    for (std::size_t i = 0; i < e; ++i) {
        char a = tail[i];
        char b = ext[i];
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return true;
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
            if (entries_[i].is_dir || has_ext(entries_[i].name, ext_filter_)) {
                if (kept != i) {
                    entries_[kept] = entries_[i];
                }
                ++kept;
            }
        }
        count_ = kept;
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
    switch (ev.key) {
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
                }
                // kBrowse on a file: view only, no action (D55).
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

    char line[24];
    const int first = scroll_ > 0 ? scroll_ : 0;
    for (int i = first; i < count_ && i - first < visible_rows(); ++i) {
        const int y = kTopY + (i - first) * kRowH;
        if (i == selected_) {
            fb.fill_rect(0, y - 1, platform::kScreenW, kRowH, platform::Color::from_rgb(0, 0, 60));
        }
        font.draw_string(fb, 8, y, entries_[i].name, kWhite);
        if (entries_[i].is_dir) {
            std::snprintf(line, sizeof(line), "[DIR]");
        } else {
            std::snprintf(line, sizeof(line), "%lu B",
                          static_cast<unsigned long>(entries_[i].size));
        }
        font.draw_string(fb, platform::kScreenW - font.text_width(line) - 8, y, line, kGrayLine);
    }

    if (count_ > visible_rows()) {
        std::snprintf(line, sizeof(line), "%d/%d", selected_ + 1, count_);
        font.draw_string(fb, platform::kScreenW - font.text_width(line) - 8, 2, line, kGrayLine);
    }

    const char* const keys[6] = {"", "", "", "", "", ""};
    ui::draw_softkeys(fb, keys);
    const char* hint = depth() > 0 ? "ENTER:OPEN  LEFT:UP  ESC:BACK" : "ENTER:OPEN  ESC:BACK";
    font.draw_string(fb, 2, platform::kScreenH - ui::kSoftkeyBarH + 4, hint, kGrayLine);
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
