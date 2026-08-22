#pragma once

#include <cstddef>
#include <cstdint>

namespace ui {

// Multi-line text buffer (Phase 6A.5, spec §3.5). Deliberately free of
// rendering and platform types so it can be unit-tested on the host —
// TextEditorWidget is the thin layer that adds keys, drawing and file
// I/O on top.
//
// Storage is one flat array with '\n' separators, plus a line-offset
// index rebuilt on every mutation. Flat-with-index rather than an
// array of fixed-length lines because save/load is then a straight
// write_file(text(), length()) with no reassembly, and short lines
// cost nothing. Insert and delete are a memmove over at most kCapacity
// bytes, which is microseconds at 200 MHz.
//
// The index is rebuilt in the mutators, never in a const accessor, so
// a caller's render path stays pure — Pico 1 calls render() once per
// 16-px strip and must not do work there (D47).
class TextBuffer {
public:
    // Text bytes, excluding the NUL. Sized for notes and the ~20-line
    // scripts 6B.11 targets, not for arbitrary documents.
    static constexpr int kCapacity = 4096;
    static constexpr int kMaxLines = 256;

    void clear();

    // Replaces the contents. Bytes past kCapacity are dropped and false
    // is returned, so a caller can tell the user their file was
    // truncated rather than silently editing a partial document.
    // '\r' is stripped, so CRLF files from a desktop editor load clean.
    bool set_text(const char* s, std::size_t len);

    // Appends at the end, same truncation and '\r' rules. Lets a file
    // be streamed in through a small chunk buffer instead of a second
    // full-size staging array — 4 KB of bss that would otherwise
    // duplicate this one for the length of the program.
    bool append_text(const char* s, std::size_t len);

    const char* text() const { return buf_; }
    std::size_t length() const { return len_; }
    bool empty() const { return len_ == 0; }

    int line_count() const { return line_count_; }
    // Pointer into the flat buffer. NOT NUL-terminated at the line end
    // — use line_length() to bound it.
    const char* line(int i) const;
    int line_length(int i) const;

    std::size_t cursor() const { return cursor_; }
    int cursor_row() const;
    int cursor_col() const;

    // All return true when they changed something.
    bool insert_char(char c);

    // Auto-indent is always on: the new line inherits the leading
    // blanks of the one it was split from. That is useful in plain
    // text too, so it is not gated on a language trigger.
    //
    // `trigger` adds a further level on top of that when the last
    // non-blank before the cursor matches it (':' for Python). 0
    // disables the extra level, which with extra_indent = 0 leaves
    // plain indent-carrying — the Notepad default.
    bool insert_newline(char trigger, int extra_indent);
    bool backspace();
    bool del();

    bool move_left();
    bool move_right();
    bool move_up();
    bool move_down();
    bool move_line_start();
    bool move_line_end();
    // Clamps into range; used when loading a file or jumping.
    void set_cursor(std::size_t offset);

    bool dirty() const { return dirty_; }
    void mark_clean() { dirty_ = false; }

private:
    char buf_[kCapacity + 1] = {};
    std::size_t len_ = 0;
    std::size_t cursor_ = 0;

    // Byte offset of each line's first character. kCapacity fits in a
    // uint16, so this is 512 B rather than 1 KB.
    std::uint16_t line_start_[kMaxLines] = {};
    int line_count_ = 1;

    // Column that vertical motion tries to return to, so walking down
    // past a short line and back up lands where you started.
    int desired_col_ = 0;
    bool dirty_ = false;

    void reindex();
    std::size_t line_offset(int row) const;
};

}  // namespace ui
