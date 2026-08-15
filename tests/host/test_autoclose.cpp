// Host-side tests for math::close_open_parens (issue #35) — TI-style
// auto-completion of trailing parentheses at entry time.

#include <cstdio>
#include <cstring>

#include "math/autoclose.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

// Runs the fixup on a copy of `in` and checks both the resulting text
// and how many parens were reported as added.
void check_close(const char* in, const char* expected, int expected_added) {
    ++g_checks;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", in);
    const int added = math::close_open_parens(buf, sizeof(buf));
    if (std::strcmp(buf, expected) != 0 || added != expected_added) {
        std::printf("FAIL: \"%s\" -> \"%s\" (+%d), expected \"%s\" (+%d)\n", in, buf, added,
                    expected, expected_added);
        ++g_failures;
    }
}

// Unchanged input, nothing reported.
void check_untouched(const char* in) {
    check_close(in, in, 0);
}

void test_the_reported_case() {
    // Bryn's report, forum post #5: sin(90 should run as sin(90).
    check_close("sin(90", "sin(90)", 1);
}

void test_balanced_is_left_alone() {
    check_untouched("sin(90)");
    check_untouched("1+2");
    check_untouched("");
    check_untouched("(1+2)*(3+4)");
    check_untouched("sin(cos(0))");
}

void test_multiple_and_nested() {
    check_close("sin(cos(0", "sin(cos(0))", 2);
    check_close("(((1", "(((1)))", 3);
    check_close("2*(3+4", "2*(3+4)", 1);
    check_close("sqrt(16", "sqrt(16)", 1);
    check_close("(1+2", "(1+2)", 1);
    // Only the still-open ones are completed.
    check_close("(1+2)*(3+4", "(1+2)*(3+4)", 1);
}

void test_over_closed_is_still_an_error() {
    // Closing more than were opened is a genuine syntax error. Masking
    // it would be worse than reporting it, so the fixup declines.
    check_untouched("sin(90))");
    check_untouched(")");
    check_untouched("1+2)");
    // Over-closing early is not cancelled out by opening again later.
    check_untouched(")(");
}

void test_store_target_keeps_its_place() {
    // The parens belong to the EXPRESSION, not the end of the line:
    // sin(90->a must become sin(90)->a, never sin(90->a).
    check_close("sin(90->a", "sin(90)->a", 1);
    check_close("(1+2->b", "(1+2)->b", 1);
    check_close("sin(cos(0->c", "sin(cos(0))->c", 2);
    check_untouched("sin(90)->a");
    // A bare arrow with nothing open stays untouched.
    check_untouched("5->a");
}

void test_display_suffix_keeps_its_place() {
    check_close("1/(2+3>frac", "1/(2+3)>frac", 1);
    check_close("1/(2+3>dec", "1/(2+3)>dec", 1);
    check_untouched("1/(2+3)>frac");
    // The suffix alone is not a paren problem.
    check_untouched("0.5>frac");
}

void test_store_wins_over_suffix() {
    // Both present: the store arrow comes first, so that is the
    // boundary. Anything else would put the paren on the wrong side.
    check_close("sin(90->a>frac", "sin(90)->a>frac", 1);
}

void test_capacity_is_respected() {
    ++g_checks;
    // A buffer with exactly no room for the closing paren must be left
    // alone rather than truncated — a truncated expression is worse
    // than a syntax error.
    char tight[8];
    std::snprintf(tight, sizeof(tight), "sin(90");  // 6 chars + NUL = 7 of 8
    const int added = math::close_open_parens(tight, sizeof(tight));
    if (added != 1 || std::strcmp(tight, "sin(90)") != 0) {
        std::printf("FAIL: exact-fit case -> \"%s\" (+%d)\n", tight, added);
        ++g_failures;
    }

    ++g_checks;
    char full[7];
    std::snprintf(full, sizeof(full), "sin(90");  // 7 of 7 — no room
    const int added2 = math::close_open_parens(full, sizeof(full));
    if (added2 != 0 || std::strcmp(full, "sin(90") != 0) {
        std::printf("FAIL: no-room case -> \"%s\" (+%d)\n", full, added2);
        ++g_failures;
    }
}

void test_brackets_and_braces_are_not_touched() {
    // TI closes parens only. Matrix/list literals have stricter syntax
    // where guessing is more likely to be wrong than helpful.
    check_untouched("[[1,2][3,4");
    check_untouched("{1,2,3");
    // A paren inside an unclosed list still gets its own treatment,
    // since only ')' is ever inserted.
    check_close("{1,sin(2", "{1,sin(2)", 1);
}

void test_null_and_empty() {
    ++g_checks;
    if (math::close_open_parens(nullptr, 16) != 0) {
        std::printf("FAIL: null buffer\n");
        ++g_failures;
    }
    ++g_checks;
    char buf[4] = "";
    if (math::close_open_parens(buf, 0) != 0) {
        std::printf("FAIL: zero capacity\n");
        ++g_failures;
    }
}

}  // namespace

int main() {
    test_the_reported_case();
    test_balanced_is_left_alone();
    test_multiple_and_nested();
    test_over_closed_is_still_an_error();
    test_store_target_keeps_its_place();
    test_display_suffix_keeps_its_place();
    test_store_wins_over_suffix();
    test_capacity_is_respected();
    test_brackets_and_braces_are_not_touched();
    test_null_and_empty();

    std::printf("test_autoclose: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
