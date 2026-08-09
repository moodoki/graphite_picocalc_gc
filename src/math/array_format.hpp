#pragma once

#include <cstddef>

#include "math/array.hpp"

// Display formatting for array-valued results (Phase 5.2, task 5.2.11).
//
// These three functions were `listexpr::format_list` and
// `matexpr::format_matrix`/`format_matrix_frac`. They are display code, not
// evaluator code, and they outlive the evaluators they happened to live in —
// 5.2.11 rehomes them rather than deleting them, exactly as MatAns moves to the
// matrix store. The bodies are unchanged; only the namespace is.
//
// Scalars are formatted by format.hpp. These are here rather than there so that
// format.hpp, which half the UI includes, does not start pulling in the array
// stores.
namespace math {

// "{1,2,3}" (compact form per element); ",<ellipsis>}" when truncated.
void format_list(const Array& a, char* buf, size_t buf_len);

// "[[1,2][3,4]]" (compact form per cell); "...]]" when truncated. Near-zero
// cells snap to "0" relative to the matrix's own largest magnitude, so
// [A]^-1*[A] prints as a clean identity instead of 2.22e-16 off-diagonals.
void format_matrix(const Array& m, char* buf, size_t buf_len);

// Same layout, but real cells render as fractions p/q (>Frac, 4D.2) when a
// tight fraction exists. Complex cells keep the compact rectangular/polar form.
void format_matrix_frac(const Array& m, char* buf, size_t buf_len);

}  // namespace math
