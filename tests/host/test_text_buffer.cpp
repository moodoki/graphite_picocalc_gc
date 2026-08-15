// Host-side tests for ui::TextBuffer (Phase 6A.5) — the multi-line
// edit core behind TextEditorWidget. Split out from the widget
// precisely so this logic is testable without a framebuffer.

#include <cstdio>
#include <cstring>

#include "ui/text_buffer.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void check_text(const ui::TextBuffer& b, const char* expected, const char* what) {
    ++g_checks;
    if (std::strcmp(b.text(), expected) != 0) {
        std::printf("FAIL: %s -> \"%s\" (expected \"%s\")\n", what, b.text(), expected);
        ++g_failures;
    }
}

void load(ui::TextBuffer& b, const char* s) {
    b.set_text(s, std::strlen(s));
}

void type(ui::TextBuffer& b, const char* s) {
    for (const char* p = s; *p != 0; ++p) {
        b.insert_char(*p);
    }
}

void test_empty() {
    ui::TextBuffer b;
    check(b.empty(), "fresh buffer empty");
    check(b.length() == 0, "fresh length 0");
    check(b.line_count() == 1, "fresh buffer has one line");
    check(b.line_length(0) == 0, "fresh line 0 empty");
    check(b.cursor_row() == 0 && b.cursor_col() == 0, "fresh cursor at origin");
    check(!b.dirty(), "fresh buffer not dirty");
    check(!b.backspace(), "backspace at start is a no-op");
    check(!b.del(), "delete at end is a no-op");
    check(!b.move_left(), "left at start is a no-op");
    check(!b.move_up(), "up on first line is a no-op");
}

void test_line_indexing() {
    ui::TextBuffer b;
    load(b, "alpha\nbeta\n\ngamma");
    check(b.line_count() == 4, "four lines incl. the blank one");
    check(b.line_length(0) == 5, "line 0 length");
    check(b.line_length(1) == 4, "line 1 length");
    check(b.line_length(2) == 0, "blank line length 0");
    check(b.line_length(3) == 5, "last line length (no trailing newline)");
    check(std::strncmp(b.line(1), "beta", 4) == 0, "line 1 contents");
    check(std::strncmp(b.line(3), "gamma", 5) == 0, "line 3 contents");
    check(b.line_length(-1) == 0 && b.line_length(99) == 0, "out-of-range line length is 0");

    // A trailing newline creates a real empty final line.
    load(b, "one\n");
    check(b.line_count() == 2, "trailing newline yields a final empty line");
    check(b.line_length(1) == 0, "final empty line has length 0");
}

void test_crlf_is_stripped() {
    ui::TextBuffer b;
    load(b, "a\r\nb\r\n");
    check_text(b, "a\nb\n", "CRLF stripped on load");
    check(b.line_count() == 3, "CRLF file line count");
}

void test_insert_and_cursor() {
    ui::TextBuffer b;
    type(b, "hello");
    check_text(b, "hello", "typed text");
    check(b.cursor() == 5, "cursor after typing");
    check(b.dirty(), "typing marks dirty");
    b.mark_clean();
    check(!b.dirty(), "mark_clean clears dirty");

    b.move_line_start();
    check(b.cursor_col() == 0, "home moves to column 0");
    b.insert_char('X');
    check_text(b, "Xhello", "insert at line start");
    check(b.cursor_col() == 1, "cursor advanced past insert");

    b.move_line_end();
    check(b.cursor_col() == 6, "end moves past last char");
}

void test_newline_splits() {
    ui::TextBuffer b;
    type(b, "abcd");
    b.move_line_start();
    b.move_right();
    b.move_right();
    b.insert_newline(0);
    check_text(b, "ab\ncd", "newline splits the line at the cursor");
    check(b.line_count() == 2, "split produced two lines");
    check(b.cursor_row() == 1 && b.cursor_col() == 0, "cursor lands on the new line");
}

void test_auto_indent() {
    ui::TextBuffer b;

    // Disabled: a plain newline carries no indent at all.
    load(b, "    x");
    b.set_cursor(b.length());
    b.insert_newline(0);
    check_text(b, "    x\n", "auto-indent disabled adds no indent");

    // Enabled but not triggered: the leading blanks are still carried,
    // because that is ordinary indent-preservation, not the ':' rule.
    load(b, "    x");
    b.set_cursor(b.length());
    b.insert_newline(':');
    check_text(b, "    x\n    ", "indent preserved without the trigger char");

    // Triggered: one extra level on top of the current indent.
    load(b, "  if x:");
    b.set_cursor(b.length());
    b.insert_newline(':');
    check_text(b, "  if x:\n    ", "trigger char adds one indent level");

    // Trailing blanks after the trigger char still count.
    load(b, "if x:   ");
    b.set_cursor(b.length());
    b.insert_newline(':');
    check_text(b, "if x:   \n  ", "trailing blanks don't hide the trigger char");

    // Splitting mid-line only measures the indent left of the cursor.
    load(b, "        tail");
    b.set_cursor(4);
    b.insert_newline(':');
    check_text(b, "    \n        tail", "indent measured left of the cursor only");
}

void test_backspace_joins_lines() {
    ui::TextBuffer b;
    load(b, "ab\ncd");
    b.set_cursor(3);  // start of "cd"
    check(b.cursor_row() == 1 && b.cursor_col() == 0, "cursor at start of line 1");
    b.backspace();
    check_text(b, "abcd", "backspace at line start joins with the previous line");
    check(b.line_count() == 1, "join reduced the line count");
    check(b.cursor() == 2, "cursor sits at the join point");
}

void test_delete_forward() {
    ui::TextBuffer b;
    load(b, "abc");
    b.set_cursor(1);
    b.del();
    check_text(b, "ac", "delete removes the char under the cursor");
    check(b.cursor() == 1, "delete leaves the cursor put");
    b.set_cursor(b.length());
    check(!b.del(), "delete at end of buffer is a no-op");
}

void test_vertical_motion_keeps_column() {
    ui::TextBuffer b;
    // A long line, a short one, then long again — the classic case
    // where a naive implementation loses the column.
    load(b, "abcdefgh\nxy\nabcdefgh");
    b.set_cursor(6);  // row 0, col 6
    check(b.cursor_col() == 6, "start column");

    b.move_down();
    check(b.cursor_row() == 1, "moved to the short line");
    check(b.cursor_col() == 2, "column clamped to the short line's end");

    b.move_down();
    check(b.cursor_row() == 2, "moved to the third line");
    check(b.cursor_col() == 6, "desired column restored past the short line");

    // A horizontal move resets the remembered column.
    b.move_left();
    b.move_up();
    check(b.cursor_col() == 2, "left-arrow reset the desired column to 5, clamped to 2");
}

void test_capacity_bounds() {
    ui::TextBuffer b;
    // Fill to capacity one char at a time; the first refusal must come
    // exactly at kCapacity, with the buffer intact.
    int written = 0;
    while (b.insert_char('x')) {
        ++written;
        if (written > ui::TextBuffer::kCapacity + 8) {
            break;  // never refused — the check below will fail
        }
    }
    check(written == ui::TextBuffer::kCapacity, "insert fills exactly to kCapacity");
    check(b.length() == static_cast<std::size_t>(ui::TextBuffer::kCapacity), "length at cap");
    check(!b.insert_char('y'), "insert past capacity refused");
    check(!b.insert_newline(0), "newline past capacity refused");
    check(b.text()[b.length()] == 0, "buffer still NUL-terminated at the cap");

    // Overlong loads report the truncation rather than failing silently.
    static char big[ui::TextBuffer::kCapacity + 64];
    std::memset(big, 'z', sizeof(big));
    ui::TextBuffer c;
    check(!c.set_text(big, sizeof(big)), "oversized set_text reports incomplete");
    check(c.length() == static_cast<std::size_t>(ui::TextBuffer::kCapacity),
          "oversized set_text keeps what fits");

    ui::TextBuffer d;
    check(d.set_text("small", 5), "in-range set_text reports complete");
}

void test_line_count_bound() {
    ui::TextBuffer b;
    // insert_newline must refuse rather than overrun line_start_.
    int lines = 1;
    while (b.insert_newline(0)) {
        ++lines;
        if (lines > ui::TextBuffer::kMaxLines + 4) {
            break;
        }
    }
    check(lines == ui::TextBuffer::kMaxLines, "newline fills exactly to kMaxLines");
    check(b.line_count() == ui::TextBuffer::kMaxLines, "line_count at the cap");
    check(!b.insert_newline(0), "newline past kMaxLines refused");
    // Ordinary characters must still work at the line cap.
    check(b.insert_char('a'), "typing still works once the line cap is hit");
}

void test_set_cursor_clamps() {
    ui::TextBuffer b;
    load(b, "abc");
    b.set_cursor(999);
    check(b.cursor() == 3, "set_cursor clamps to length");
    check(b.cursor_col() == 3, "clamped cursor column");
}

}  // namespace

int main() {
    test_empty();
    test_line_indexing();
    test_crlf_is_stripped();
    test_insert_and_cursor();
    test_newline_splits();
    test_auto_indent();
    test_backspace_joins_lines();
    test_delete_forward();
    test_vertical_motion_keeps_column();
    test_capacity_bounds();
    test_line_count_bound();
    test_set_cursor_clamps();

    std::printf("test_text_buffer: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
