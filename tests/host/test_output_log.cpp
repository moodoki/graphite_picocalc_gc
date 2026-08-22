// Host-side tests for ui::OutputLog (Phase 6B.12) — the script output
// pane's buffer. Pure logic: no rendering, no interpreter, no device.
//
// The behaviour worth pinning is what happens at the edges — dropping
// the oldest lines to keep the tail, the index overflowing independently
// of the byte buffer, and a single write larger than the whole log.
// Those are exactly the cases an on-device check is worst at showing,
// because nothing visibly breaks; the log just quietly holds the wrong
// text.

#include <cstdio>
#include <cstring>

#include "ui/output_log.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool cond, const char* what) {
    ++g_checks;
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void check_line(const ui::OutputLog& log, int i, const char* expected, const char* what) {
    ++g_checks;
    const int n = log.line_length(i);
    const int e = static_cast<int>(std::strlen(expected));
    if (n != e || std::strncmp(log.line(i), expected, static_cast<std::size_t>(n)) != 0) {
        std::printf("FAIL: %s — line %d is \"%.*s\" (%d), expected \"%s\"\n", what, i, n,
                    log.line(i), n, expected);
        ++g_failures;
    }
}

void put(ui::OutputLog& log, const char* s) {
    log.append(s, std::strlen(s));
}

void test_empty() {
    ui::OutputLog log;
    check(log.empty(), "fresh log is empty");
    check(log.line_count() == 1, "fresh log has one (empty) line");
    check(log.line_length(0) == 0, "that line is zero length");
    check(!log.truncated(), "nothing dropped yet");
    // Out-of-range access must be safe, not UB — render() indexes this
    // from a scroll offset that can outrun a log cleared underneath it.
    check(log.line_length(-1) == 0, "negative index is empty");
    check(log.line_length(999) == 0, "past-the-end index is empty");
    check(log.line(999) != nullptr, "past-the-end line is still a valid pointer");
}

void test_basic_lines() {
    ui::OutputLog log;
    put(log, "hello\n");
    check(log.line_count() == 2, "one newline makes two lines");
    check_line(log, 0, "hello", "first line");
    check_line(log, 1, "", "trailing empty line after a newline");

    put(log, "world\n");
    check(log.line_count() == 3, "second line appended");
    check_line(log, 1, "world", "second line");
}

void test_partial_writes_join() {
    // MicroPython writes through mp_hal_stdout_tx_strn_cooked in
    // whatever chunks it feels like; a line is not a write.
    ui::OutputLog log;
    put(log, "ab");
    put(log, "cd");
    put(log, "\nef");
    check(log.line_count() == 2, "three writes, two lines");
    check_line(log, 0, "abcd", "chunks joined into one line");
    check_line(log, 1, "ef", "remainder starts the next line");
}

void test_carriage_returns_are_stripped() {
    // stdout is *cooked*: '\n' arrives as CRLF. Keeping the '\r' would
    // put a stray glyph at the end of every single line on screen.
    ui::OutputLog log;
    put(log, "one\r\ntwo\r\n");
    check(log.line_count() == 3, "CRLF counts as one separator");
    check_line(log, 0, "one", "no trailing CR on the first line");
    check_line(log, 1, "two", "no trailing CR on the second");
}

void test_clear() {
    ui::OutputLog log;
    put(log, "something\n");
    log.clear();
    check(log.empty(), "cleared log is empty");
    check(log.line_count() == 1, "cleared log has one line");
    check(!log.truncated(), "clear resets the truncation flag");
}

void test_drops_oldest_to_keep_tail() {
    ui::OutputLog log;
    // 8-byte lines ("L0000\n" style) until well past capacity.
    char line[16];
    const int total = (ui::OutputLog::kCapacity / 6) + 50;
    for (int i = 0; i < total; ++i) {
        std::snprintf(line, sizeof(line), "%04d\n", i % 10000);
        put(log, line);
    }
    check(log.truncated(), "overflowing the buffer reports truncation");
    check(log.line_count() <= ui::OutputLog::kMaxLines, "line index never overflows");

    // The LAST line written must still be there — that is the whole
    // point of keeping the tail rather than the head.
    std::snprintf(line, sizeof(line), "%04d", (total - 1) % 10000);
    check_line(log, log.line_count() - 2, line, "newest line survives");

    // And the first one must not be.
    check(std::strncmp(log.line(0), "0000", 4) != 0, "oldest line was dropped");
}

void test_dropping_is_line_aligned() {
    ui::OutputLog log;
    // Fill with a known, distinctive line, then check every surviving
    // line is a whole one rather than a fragment.
    for (int i = 0; i < 400; ++i) {
        put(log, "abcdefghij\n");
    }
    check(log.truncated(), "filled past capacity");
    for (int i = 0; i < log.line_count() - 1; ++i) {
        check_line(log, i, "abcdefghij", "every surviving line is whole");
    }
}

void test_index_overflows_before_bytes() {
    // 1024 two-byte lines fit in 2048 bytes but need 1024 index slots,
    // and there are only kMaxLines. The index is the binding constraint
    // here, not the buffer, and it must not be the one that silently
    // gives way.
    ui::OutputLog log;
    for (int i = 0; i < 600; ++i) {
        put(log, "x\n");
    }
    check(log.line_count() <= ui::OutputLog::kMaxLines, "index capped");
    check(log.truncated(), "capping the index reports truncation");
    check_line(log, 0, "x", "surviving lines are still intact");
}

void test_write_larger_than_the_whole_log() {
    ui::OutputLog log;
    // One write of 3x capacity. Keeping the tail is consistent with
    // what the log does everywhere else.
    static char big[ui::OutputLog::kCapacity * 3];
    std::memset(big, 'a', sizeof(big));
    big[sizeof(big) - 5] = 'Z';
    big[sizeof(big) - 4] = 'Z';
    big[sizeof(big) - 3] = 'Z';
    big[sizeof(big) - 2] = 'Z';
    big[sizeof(big) - 1] = '\n';
    log.append(big, sizeof(big));

    check(log.truncated(), "oversized write reports truncation");
    check(log.line_count() == 2, "it is still one line plus the empty tail");
    const int n = log.line_length(0);
    check(n > 0 && n < ui::OutputLog::kCapacity, "kept line fits the buffer");
    check(std::strncmp(log.line(0) + n - 4, "ZZZZ", 4) == 0, "the TAIL of the write is kept");
}

void test_no_trailing_newline() {
    // A script whose last print has no newline, or a traceback cut off
    // mid-line, must not lose its final line.
    ui::OutputLog log;
    put(log, "first\nsecond");
    check(log.line_count() == 2, "unterminated last line still counts");
    check_line(log, 1, "second", "unterminated last line is readable");
}

void test_zero_and_null_writes() {
    ui::OutputLog log;
    put(log, "x\n");
    log.append(nullptr, 10);
    log.append("y", 0);
    check(log.line_count() == 2, "null and empty writes change nothing");
    check_line(log, 0, "x", "content untouched");
}

}  // namespace

int main() {
    test_empty();
    test_basic_lines();
    test_partial_writes_join();
    test_carriage_returns_are_stripped();
    test_clear();
    test_drops_oldest_to_keep_tail();
    test_dropping_is_line_aligned();
    test_index_overflows_before_bytes();
    test_write_larger_than_the_whole_log();
    test_no_trailing_newline();
    test_zero_and_null_writes();

    std::printf("test_output_log: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
