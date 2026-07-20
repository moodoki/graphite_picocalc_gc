#pragma once

#include <cstddef>

// Inline solve() calls on the home screen (task 4A.9; decision
// 2026-07-20: solver is both a form screen and an expression form).
//
//   solve(x^3-2x-5, x, 2)          Newton from the guess
//   solve(x^2-2, x, 0, 2)          bracketed on [lo, hi]
//   solve(sin(x)=0.5, x, 0, pi/2)  equation form (top-level '=')
//
// Each call is replaced by a numeric literal (like the list
// reductions), so solve() composes inside larger scalar expressions.
namespace math::solveexpr {

bool contains_solve(const char* s);

// Replace every solve(...) call in buf with its numeric result.
// Returns false and sets *err (static string) on failure. A buf
// without solve() calls is left untouched (returns true).
bool substitute(char* buf, size_t cap, const char** err);

}  // namespace math::solveexpr
