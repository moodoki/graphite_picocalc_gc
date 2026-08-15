#pragma once

#include <cstddef>

namespace math {

// Completes the closing parentheses an entry left off, TI-style
// (issue #35). `sin(90` becomes `sin(90)`.
//
// Edits `buf` in place and returns how many ')' were appended, or 0 if
// nothing changed. Deliberately conservative — it declines rather than
// guesses in three cases:
//
//   * The entry is already balanced.
//   * The entry closes MORE parens than it opens (`sin(90))`). That is
//     a genuine syntax error and masking it would be worse than
//     reporting it.
//   * The result would not fit in `cap`. Better a syntax error than a
//     silently truncated expression.
//
// The parens are inserted at the end of the EXPRESSION, which is not
// always the end of the string: a store target or a display suffix
// trails it. `sin(90->a` must become `sin(90)->a`, not `sin(90->a)`,
// and `1/(2+3>frac` must become `1/(2+3)>frac`. `>` is unambiguous
// here — the expression language uses it only for the `->` store arrow
// and the `>frac`/`>dec` suffixes, never as a comparison operator.
//
// Brackets and braces are NOT auto-closed. TI closes parens only, and
// matrix/list literals have stricter syntax where a guess is more
// likely to be wrong than helpful.
int close_open_parens(char* buf, std::size_t cap);

}  // namespace math
