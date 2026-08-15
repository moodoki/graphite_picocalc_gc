#include "ui/text_buffer.hpp"

#include <cstring>

namespace ui {

namespace {

bool is_blank(char c) {
    return c == ' ' || c == '\t';
}

}  // namespace

void TextBuffer::reindex() {
    line_count_ = 1;
    line_start_[0] = 0;
    for (std::size_t i = 0; i < len_; ++i) {
        if (buf_[i] == '\n' && line_count_ < kMaxLines) {
            line_start_[line_count_++] = static_cast<std::uint16_t>(i + 1);
        }
    }
}

void TextBuffer::clear() {
    len_ = 0;
    cursor_ = 0;
    desired_col_ = 0;
    buf_[0] = 0;
    dirty_ = false;
    reindex();
}

bool TextBuffer::set_text(const char* s, std::size_t len) {
    clear();
    return append_text(s, len);
}

bool TextBuffer::append_text(const char* s, std::size_t len) {
    if (s == nullptr) {
        return true;
    }
    bool complete = true;
    for (std::size_t i = 0; i < len; ++i) {
        const char c = s[i];
        if (c == '\r') {
            continue;  // CRLF from a desktop editor
        }
        if (len_ >= static_cast<std::size_t>(kCapacity)) {
            complete = false;
            break;
        }
        buf_[len_++] = c;
    }
    buf_[len_] = 0;
    reindex();
    // Loading is not editing — a freshly loaded file has nothing
    // unsaved, so the caller isn't warned about discarding it.
    dirty_ = false;
    return complete;
}

// Every read of line_start_ outside reindex() goes through here, so the
// bound is stated once and is visible to the static analyzer rather
// than inferred from each caller's guard.
std::size_t TextBuffer::line_offset(int row) const {
    if (row <= 0) {
        return 0;
    }
    const int last = (line_count_ < kMaxLines ? line_count_ : kMaxLines) - 1;
    return line_start_[row > last ? last : row];
}

const char* TextBuffer::line(int i) const {
    return buf_ + line_offset(i);
}

int TextBuffer::line_length(int i) const {
    if (i < 0 || i >= line_count_) {
        return 0;
    }
    const std::size_t start = line_offset(i);
    // The next line's start is one past this line's '\n', so the
    // terminator is not counted.
    const std::size_t end = (i + 1 < line_count_) ? (line_offset(i + 1) - 1) : len_;
    return static_cast<int>(end - start);
}

int TextBuffer::cursor_row() const {
    int row = 0;
    for (int i = 1; i < line_count_; ++i) {
        if (line_start_[i] <= cursor_) {
            row = i;
        } else {
            break;
        }
    }
    return row;
}

int TextBuffer::cursor_col() const {
    return static_cast<int>(cursor_ - line_offset(cursor_row()));
}

void TextBuffer::set_cursor(std::size_t offset) {
    cursor_ = offset > len_ ? len_ : offset;
    desired_col_ = cursor_col();
}

bool TextBuffer::insert_char(char c) {
    if (c == '\n') {
        return insert_newline(0);
    }
    if (len_ >= static_cast<std::size_t>(kCapacity)) {
        return false;
    }
    std::memmove(buf_ + cursor_ + 1, buf_ + cursor_, len_ - cursor_ + 1);
    buf_[cursor_] = c;
    ++len_;
    ++cursor_;
    reindex();
    desired_col_ = cursor_col();
    dirty_ = true;
    return true;
}

bool TextBuffer::insert_newline(char auto_indent_after) {
    if (line_count_ >= kMaxLines) {
        return false;
    }

    // Work out the indent before touching the buffer — the source line
    // moves once the newline goes in.
    //
    // auto_indent_after == 0 disables the feature outright, which means
    // no indent carrying either, not just no extra level: Notepad
    // (§3.6) configures none and wants a plain newline.
    int indent = 0;
    if (auto_indent_after != 0) {
        const int row = cursor_row();
        const std::size_t start = line_offset(row);
        while (start + static_cast<std::size_t>(indent) < cursor_ &&
               is_blank(buf_[start + indent])) {
            ++indent;
        }
        // One extra level when the last non-blank before the cursor is
        // the trigger char.
        std::size_t scan = cursor_;
        while (scan > start && is_blank(buf_[scan - 1])) {
            --scan;
        }
        if (scan > start && buf_[scan - 1] == auto_indent_after) {
            indent += kIndentWidth;
        }
    }

    if (len_ + 1 + static_cast<std::size_t>(indent) > static_cast<std::size_t>(kCapacity)) {
        return false;
    }

    const std::size_t added = 1 + static_cast<std::size_t>(indent);
    std::memmove(buf_ + cursor_ + added, buf_ + cursor_, len_ - cursor_ + 1);
    buf_[cursor_] = '\n';
    for (int i = 0; i < indent; ++i) {
        buf_[cursor_ + 1 + static_cast<std::size_t>(i)] = ' ';
    }
    len_ += added;
    cursor_ += added;
    reindex();
    desired_col_ = cursor_col();
    dirty_ = true;
    return true;
}

bool TextBuffer::backspace() {
    if (cursor_ == 0) {
        return false;
    }
    std::memmove(buf_ + cursor_ - 1, buf_ + cursor_, len_ - cursor_ + 1);
    --len_;
    --cursor_;
    reindex();
    desired_col_ = cursor_col();
    dirty_ = true;
    return true;
}

bool TextBuffer::del() {
    if (cursor_ >= len_) {
        return false;
    }
    std::memmove(buf_ + cursor_, buf_ + cursor_ + 1, len_ - cursor_);
    --len_;
    reindex();
    desired_col_ = cursor_col();
    dirty_ = true;
    return true;
}

bool TextBuffer::move_left() {
    if (cursor_ == 0) {
        return false;
    }
    --cursor_;
    desired_col_ = cursor_col();
    return true;
}

bool TextBuffer::move_right() {
    if (cursor_ >= len_) {
        return false;
    }
    ++cursor_;
    desired_col_ = cursor_col();
    return true;
}

bool TextBuffer::move_up() {
    const int row = cursor_row();
    if (row == 0) {
        return false;
    }
    const int target = row - 1;
    const int len = line_length(target);
    const int col = desired_col_ < len ? desired_col_ : len;
    cursor_ = line_offset(target) + static_cast<std::size_t>(col);
    return true;
}

bool TextBuffer::move_down() {
    const int row = cursor_row();
    if (row + 1 >= line_count_) {
        return false;
    }
    const int target = row + 1;
    const int len = line_length(target);
    const int col = desired_col_ < len ? desired_col_ : len;
    cursor_ = line_offset(target) + static_cast<std::size_t>(col);
    return true;
}

bool TextBuffer::move_line_start() {
    const std::size_t start = line_offset(cursor_row());
    if (cursor_ == start) {
        return false;
    }
    cursor_ = start;
    desired_col_ = 0;
    return true;
}

bool TextBuffer::move_line_end() {
    const int row = cursor_row();
    const std::size_t end = line_offset(row) + static_cast<std::size_t>(line_length(row));
    if (cursor_ == end) {
        return false;
    }
    cursor_ = end;
    desired_col_ = cursor_col();
    return true;
}

}  // namespace ui
