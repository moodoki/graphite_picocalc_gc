#pragma once

#include <cstddef>
#include <cstdint>

namespace ui {

// Captured output from a script run (Phase 6B.12).
//
// Deliberately free of rendering and platform types, the same way
// TextBuffer is, so the wrap and line-indexing behaviour can be tested
// on the host — the parts most likely to be wrong are exactly the parts
// a device test is worst at showing.
//
// KEEPS THE TAIL, not the head. When a run outruns the buffer the
// OLDEST whole lines are dropped. Both ends have a claim — a script
// that prints a table wants its first rows, a script that fails wants
// its traceback — and the traceback wins, because it is the case where
// the user cannot get the information any other way. `truncated()` says
// when something was lost so the screen can show it rather than
// silently presenting a partial log as complete.
//
// Dropping is line-aligned, so the surviving text never starts
// mid-word. '\r' is stripped on the way in: MicroPython writes through
// mp_hal_stdout_tx_strn_cooked, which emits CRLF.
class OutputLog {
public:
    // ~2.3 KB with the index. Sized against the Pico 1's 20 KB of spare
    // SRAM after the Python heap, not against what a chatty script
    // might want.
    static constexpr int kCapacity = 2048;
    static constexpr int kMaxLines = 128;

    void clear();

    // Appends, dropping oldest lines to make room. A single write larger
    // than the whole buffer keeps only its tail.
    void append(const char* s, std::size_t len);

    int line_count() const { return line_count_; }
    // Pointer into the flat buffer. NOT NUL-terminated — bound it with
    // line_length().
    const char* line(int i) const;
    int line_length(int i) const;

    bool empty() const { return len_ == 0; }
    // True once anything has been dropped to make room.
    bool truncated() const { return truncated_; }

private:
    char buf_[kCapacity] = {};
    std::size_t len_ = 0;
    std::uint16_t line_start_[kMaxLines] = {};
    int line_count_ = 1;
    bool truncated_ = false;

    void drop_oldest_line();
    void reindex();
};

}  // namespace ui
