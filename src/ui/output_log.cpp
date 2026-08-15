#include "ui/output_log.hpp"

#include <cstring>

namespace ui {

void OutputLog::clear() {
    len_ = 0;
    line_count_ = 1;
    line_start_[0] = 0;
    truncated_ = false;
    buf_[0] = 0;
}

void OutputLog::drop_oldest_line() {
    // Everything up to and including the first newline. With no newline
    // at all the whole buffer is one unterminated line, and dropping it
    // is the only way to make room.
    std::size_t cut = 0;
    while (cut < len_ && buf_[cut] != '\n') {
        ++cut;
    }
    if (cut < len_) {
        ++cut;  // take the newline too
    }
    std::memmove(buf_, buf_ + cut, len_ - cut);
    len_ -= cut;
    truncated_ = true;
}

void OutputLog::reindex() {
    // Count first. The index can overflow independently of the byte
    // buffer — 2048 bytes of one-character lines is 1024 of them — so
    // the excess is dropped in a single move rather than a line at a
    // time, which would be quadratic on exactly the pathological input
    // that provokes it.
    int lines = 1;
    for (std::size_t i = 0; i < len_; ++i) {
        if (buf_[i] == '\n') {
            ++lines;
        }
    }
    if (lines > kMaxLines) {
        int to_drop = lines - kMaxLines;
        std::size_t cut = 0;
        while (cut < len_ && to_drop > 0) {
            if (buf_[cut] == '\n') {
                --to_drop;
            }
            ++cut;
        }
        std::memmove(buf_, buf_ + cut, len_ - cut);
        len_ -= cut;
        truncated_ = true;
    }

    line_count_ = 1;
    line_start_[0] = 0;
    for (std::size_t i = 0; i < len_; ++i) {
        if (buf_[i] == '\n' && line_count_ < kMaxLines) {
            line_start_[line_count_++] = static_cast<std::uint16_t>(i + 1);
        }
    }
}

void OutputLog::append(const char* s, std::size_t len) {
    if (s == nullptr || len == 0) {
        return;
    }
    // A single write bigger than the buffer keeps its tail, for the same
    // reason the buffer as a whole does.
    if (len >= static_cast<std::size_t>(kCapacity)) {
        s += len - (kCapacity - 1);
        len = kCapacity - 1;
        truncated_ = true;
    }
    // Reserve against the incoming length, not the post-strip length —
    // stripping only ever writes fewer bytes, so this cannot under-book.
    while (static_cast<std::size_t>(kCapacity) - len_ < len) {
        drop_oldest_line();
    }
    for (std::size_t i = 0; i < len; ++i) {
        if (s[i] != '\r') {
            buf_[len_++] = s[i];
        }
    }
    reindex();
}

const char* OutputLog::line(int i) const {
    if (i < 0 || i >= line_count_) {
        return buf_;
    }
    return buf_ + line_start_[i];
}

int OutputLog::line_length(int i) const {
    if (i < 0 || i >= line_count_) {
        return 0;
    }
    const std::size_t start = line_start_[i];
    // The next line starts one past its separator, so the separator sits
    // at (next - 1) and is not part of this line.
    const std::size_t end = (i + 1 < line_count_) ? line_start_[i + 1] - 1 : len_;
    return end > start ? static_cast<int>(end - start) : 0;
}

}  // namespace ui
