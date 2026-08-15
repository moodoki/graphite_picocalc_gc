#include "ui/text_editor_widget.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "gfx/font.hpp"
#include "ui/chrome.hpp"

namespace ui {

namespace {

constexpr int kTextTop = kStatusBarH + 4;

// Files stream in through this rather than a full-size staging array —
// a second TextBuffer::kCapacity buffer would cost 4 KB of bss for the
// life of the program to serve one call. Small enough to be a stack
// local in principle, but core 0 has only 4 KB of stack before it runs
// into core 1's (D47), so it stays out of the frame.
constexpr std::size_t kLoadChunk = 256;
std::uint8_t g_load_chunk[kLoadChunk];

int text_bottom() {
    return platform::kScreenH - kSoftkeyBarH - 4;
}

// Does `name` already end in `ext`, case-insensitively?
bool has_ext(const char* name, const char* ext) {
    const std::size_t n = std::strlen(name);
    const std::size_t e = std::strlen(ext);
    if (e == 0 || n < e) {
        return false;
    }
    for (std::size_t i = 0; i < e; ++i) {
        char a = name[n - e + i];
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

// A filename with no path separators and no empty name.
bool valid_name(const char* s) {
    if (s == nullptr || s[0] == 0) {
        return false;
    }
    for (const char* p = s; *p != 0; ++p) {
        if (*p == '/' || *p == '\\' || *p == ':') {
            return false;
        }
    }
    return true;
}

}  // namespace

int TextEditorWidget::visible_rows() const {
    return (text_bottom() - kTextTop) / gfx::main_font().height();
}

int TextEditorWidget::visible_cols() const {
    return (platform::kScreenW / gfx::main_font().width()) - kGutterChars;
}

void TextEditorWidget::set_status(const char* msg) {
    std::snprintf(status_, sizeof(status_), "%s", msg != nullptr ? msg : "");
}

void TextEditorWidget::configure(const TextEditorConfig& cfg, const void* owner) {
    cfg_ = cfg;
    if (owner_ == owner) {
        return;  // same screen returning to itself — keep the buffer
    }
    // A different screen has taken the shared widget. Start clean and
    // reload that screen's own file if it had one.
    owner_ = owner;
    buf_.clear();
    top_line_ = 0;
    left_col_ = 0;
    exit_armed_ = false;
    prompt_.close();
    status_[0] = 0;
    path_[0] = 0;
}

bool TextEditorWidget::compose_path(const char* name, char* out, std::size_t out_len) const {
    const char* dir = cfg_.save_dir != nullptr ? cfg_.save_dir : "/picocalc";
    const char* ext = cfg_.file_ext != nullptr ? cfg_.file_ext : "";
    const bool add_ext = !has_ext(name, ext);
    const std::size_t need =
        std::strlen(dir) + 1 + std::strlen(name) + (add_ext ? std::strlen(ext) : 0) + 1;
    if (need > out_len) {
        return false;
    }
    std::snprintf(out, out_len, "%s/%s%s", dir, name, add_ext ? ext : "");
    return true;
}

bool TextEditorWidget::load(const char* path) {
    if (path == nullptr || path[0] == 0) {
        return false;
    }
    buf_.clear();
    bool complete = true;
    std::size_t offset = 0;
    for (;;) {
        const int n = platform::storage().read_file_range(path, offset, g_load_chunk, kLoadChunk);
        if (n < 0) {
            set_status("Read failed");
            return false;
        }
        if (n == 0) {
            break;
        }
        if (!buf_.append_text(reinterpret_cast<const char*>(g_load_chunk),
                              static_cast<std::size_t>(n))) {
            complete = false;
            break;
        }
        offset += static_cast<std::size_t>(n);
        if (static_cast<std::size_t>(n) < kLoadChunk) {
            break;  // short read = end of file
        }
    }
    std::snprintf(path_, sizeof(path_), "%s", path);
    buf_.set_cursor(0);
    top_line_ = 0;
    left_col_ = 0;
    exit_armed_ = false;
    set_status(complete ? "Loaded" : "Loaded (truncated)");
    return true;
}

bool TextEditorWidget::write_current() {
    if (path_[0] == 0) {
        return false;
    }
    // The directory may not exist on a fresh card — creating it here
    // means the app never has to think about first-run setup.
    if (cfg_.save_dir != nullptr) {
        platform::storage().ensure_dir(cfg_.save_dir);
    }
    const bool ok = platform::storage().write_file(
        path_, reinterpret_cast<const std::uint8_t*>(buf_.text()), buf_.length());
    if (ok) {
        buf_.mark_clean();
        exit_armed_ = false;
        set_status("Saved");
    } else {
        set_status("Save failed");
    }
    return ok;
}

void TextEditorWidget::begin_save_as() {
    prompt_.open("name=");
}

bool TextEditorWidget::save() {
    if (path_[0] == 0) {
        begin_save_as();
        return false;
    }
    return write_current();
}

void TextEditorWidget::handle_prompt_submit() {
    const char* name = prompt_.text();
    if (!valid_name(name)) {
        prompt_.set_error("Bad name");
        return;
    }
    char composed[platform::kMaxPath];
    if (!compose_path(name, composed, sizeof(composed))) {
        prompt_.set_error("Name too long");
        return;
    }
    std::snprintf(path_, sizeof(path_), "%s", composed);
    prompt_.close();
    write_current();
}

void TextEditorWidget::scroll_into_view() {
    const int row = buf_.cursor_row();
    const int rows = visible_rows();
    if (row < top_line_) {
        top_line_ = row;
    } else if (row >= top_line_ + rows) {
        top_line_ = std::max(row - rows + 1, 0);
    }

    const int col = buf_.cursor_col();
    const int cols = visible_cols();
    if (col < left_col_) {
        left_col_ = col;
    } else if (col >= left_col_ + cols) {
        left_col_ = std::max(col - cols + 1, 0);
    }
}

EditorAction TextEditorWidget::on_key(const platform::KeyEvent& ev) {
    using platform::Key;
    if (!ev.pressed) {
        return EditorAction::kNone;
    }

    if (prompt_.active()) {
        bool submitted = false;
        bool cancelled = false;
        prompt_.on_key(ev, &submitted, &cancelled);
        if (submitted) {
            handle_prompt_submit();
        }
        return EditorAction::kConsumed;
    }

    switch (ev.key) {
        case Key::kEscape:
            if (buf_.dirty() && !exit_armed_) {
                exit_armed_ = true;
                set_status("Unsaved! F2 saves, ESC again discards");
                return EditorAction::kConsumed;
            }
            return EditorAction::kExit;

        case Key::kF1:
            if (cfg_.has_run_key) {
                // Run always operates on what is on disk, so save
                // first — a RUN that executed a stale file would be a
                // genuinely confusing bug.
                if (path_[0] == 0) {
                    begin_save_as();
                    return EditorAction::kConsumed;
                }
                if (write_current() && cfg_.on_run != nullptr) {
                    cfg_.on_run(path_);
                }
            }
            return EditorAction::kConsumed;

        case Key::kF2:
            save();
            return EditorAction::kConsumed;

        case Key::kF3:
            return EditorAction::kLoad;

        case Key::kF4:
            buf_.clear();
            path_[0] = 0;
            top_line_ = 0;
            left_col_ = 0;
            exit_armed_ = false;
            set_status("New");
            begin_save_as();
            return EditorAction::kConsumed;

        case Key::kUp:
            buf_.move_up();
            break;
        case Key::kDown:
            buf_.move_down();
            break;
        case Key::kLeft:
            buf_.move_left();
            break;
        case Key::kRight:
            buf_.move_right();
            break;
        case Key::kHome:
            buf_.move_line_start();
            break;
        case Key::kEnter:
            buf_.insert_newline(cfg_.auto_indent_after, cfg_.indent_width);
            break;
        case Key::kBackspace:
            buf_.backspace();
            break;
        case Key::kDel:
            buf_.del();
            break;
        default:
            if (ev.ch != 0) {
                buf_.insert_char(ev.ch);
                break;
            }
            return EditorAction::kNone;
    }

    // Any edit invalidates the "press ESC again" arming — the user has
    // gone back to work and shouldn't lose the buffer to a stray ESC.
    if (buf_.dirty()) {
        exit_armed_ = false;
    }
    scroll_into_view();
    return EditorAction::kConsumed;
}

void TextEditorWidget::render(gfx::Framebuffer& fb) {
    using namespace platform::colors;
    const auto& font = gfx::main_font();
    const int fh = font.height();
    const int fw = font.width();

    fb.clear(kBlack);

    char title[40];
    const char* name = path_[0] != 0 ? std::strrchr(path_, '/') : nullptr;
    std::snprintf(title, sizeof(title), "%s %s%s", cfg_.title != nullptr ? cfg_.title : "EDIT",
                  name != nullptr ? name + 1 : "(unsaved)", buf_.dirty() ? "*" : "");
    draw_status_bar(fb, title);

    const int rows = visible_rows();
    const int cols = visible_cols();
    const int cur_row = buf_.cursor_row();
    const int cur_col = buf_.cursor_col();

    for (int r = 0; r < rows; ++r) {
        const int i = top_line_ + r;
        if (i >= buf_.line_count()) {
            break;
        }
        const int y = kTextTop + r * fh;

        char num[8];
        std::snprintf(num, sizeof(num), "%3d", i + 1);
        font.draw_string(fb, 0, y, num, i == cur_row ? kWhite : kGridLine);

        // Draw the visible window of the line, char by char — the flat
        // buffer's lines are not NUL-terminated, so there is no string
        // to hand to draw_string.
        const char* src = buf_.line(i);
        const int len = buf_.line_length(i);
        for (int c = 0; c < cols; ++c) {
            const int idx = left_col_ + c;
            if (idx >= len) {
                break;
            }
            font.draw_char(fb, (kGutterChars + c) * fw, y, src[idx], kWhite);
        }
    }

    // Cursor: a 2-px bar, matching InputLine's.
    if (cur_row >= top_line_ && cur_row < top_line_ + rows && cur_col >= left_col_ &&
        cur_col <= left_col_ + cols) {
        const int cx = (kGutterChars + (cur_col - left_col_)) * fw;
        const int cy = kTextTop + (cur_row - top_line_) * fh;
        fb.fill_rect(cx, cy, 2, fh, kCursor);
    }

    // Bottom strip: the modal prompt takes precedence over the status
    // text, which takes precedence over the softkey labels.
    const int bar_y = platform::kScreenH - kSoftkeyBarH;
    if (prompt_.active()) {
        fb.fill_rect(0, bar_y, platform::kScreenW, kSoftkeyBarH,
                     platform::Color::from_rgb(30, 30, 30));
        prompt_.render(fb, 2, bar_y + 2, platform::kScreenW - 4, font);
        return;
    }

    const char* const keys[6] = {cfg_.has_run_key ? "RUN" : "", "SAVE", "LOAD", "NEW", "", ""};
    draw_softkeys(fb, keys);
    if (status_[0] != 0) {
        const int w = font.text_width(status_);
        font.draw_string(fb, platform::kScreenW - w - 2, bar_y + 2, status_,
                         exit_armed_ ? kRed : kGreen);
    }
}

TextEditorWidget& text_editor() {
    static TextEditorWidget instance;
    return instance;
}

}  // namespace ui
