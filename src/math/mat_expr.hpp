#pragma once

#include <cstddef>
#include <cstdint>

#include "math/array.hpp"
#include "math/engine.hpp"

// Home-screen matrix expressions (task 4A.7, phase4-spec §3.3; TI-style
// [A]-[J] syntax by decision 2026-07-20). Matrices cannot flow through
// tinyexpr, and unlike lists their operators are not element-wise
// ([A]*[B] is matrix multiplication), so this layer is a small
// recursive-descent evaluator over tagged scalar/matrix values instead
// of a token-rewriting lift:
//
//   [A]                       matrix reference ([A] -> [B] to copy)
//   [A]+[B]  [A]-[B]  [A]*[B] matrix arithmetic (mul = matrix product)
//   2*[A]  [A]/3  -[A]        scalar scaling
//   [A]^-1  [A]^T  [A]^5      inverse, transpose, integer powers
//   [A](2,3)                  element (1-based row, col) — a scalar
//   det([A]) rank([A])        scalar results, usable in expressions
//   inverse/transpose/rref/ref/augment/identity(...)   matrix results
//   dim([A]) eigenvals([A])   list results (whole-expression forms)
//   <expr> -> [C] / lk / a    store (matrix / list / scalar targets)
//
// Scalar subterms (2*pi, sin(3), stored vars) go through eval_field.
// Anything without matrix syntax returns Kind::kNone and the caller
// falls through to the list/scalar paths.
namespace math::matexpr {

enum class Kind : uint8_t { kNone, kScalar, kMatrix, kList, kText, kError };

struct Result {
    Kind kind = Kind::kNone;
    // kScalar: ok/value/stored_var filled (Ans updated, like engine).
    EvalResult scalar;
    // kMatrix: the value to display — the result buffer (also exposed
    // as mat_ans()), or the stored [X] slot.
    const Array* matrix = nullptr;
    // kList: dim()/eigenvals() output, or the stored list slot.
    const Array* list = nullptr;
    // kText: a pre-formatted, unstorable display string — currently
    // only eigenvals() when the spectrum has a complex-conjugate pair
    // (Phase 4C, D30/P4-7): lists are real-only, so it can't become a
    // kList the way an all-real spectrum does.
    const char* text = nullptr;
    int stored_matrix = -1;          // ->[X] target index, else -1
    int stored_list = -1;            // ->lk target, else -1
    bool matrices_modified = false;  // Caller should persist matrices.dat
    bool lists_modified = false;     // Caller should persist lists.dat
    const char* error = nullptr;     // kError (static string)
};

Result evaluate(const char* input);

// The last matrix result ("MatAns") — empty until a matrix expression
// evaluates. The matrix editor shows it as a read-only slot.
const Array& mat_ans();

// "[[1,2][3,4]]" (format_number per element); "...]]" when truncated.
void format_matrix(const Array& m, char* buf, size_t buf_len);

}  // namespace math::matexpr
