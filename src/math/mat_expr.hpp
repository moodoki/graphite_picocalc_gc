#pragma once

#include <cstddef>
#include <cstdint>

#include "math/array.hpp"
#include "math/engine.hpp"

namespace platform {
class Storage;
}

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
    // kScalar with a non-real value (4D.25: det/element access of a
    // complex matrix, complex scalar subterms): cvalue holds it and
    // scalar.value only carries the real part. Ans/store are committed
    // here (set_complex); the caller just formats cvalue.
    bool scalar_complex = false;
    Complex cvalue;
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
    bool matrices_modified = false;  // Caller should persist this matrix (stored_matrix)
    bool lists_modified = false;     // Caller should persist this list (stored_list)
    // mat2list (4D.12) writes several lists at once: bit k set = lk
    // was written and needs persisting (independent of stored_list).
    uint8_t lists_mask = 0;
    const char* error = nullptr;  // kError (static string)
};

// Parse-nesting cap (D48). Like complexexpr's kMaxParseDepth this counts
// recursion levels rather than parens — a string pre-scan cannot see the
// function-argument re-entry (`det(<expr>)`) that costs a level here.
//
// The cycle is parse_expr -> parse_term -> parse_unary -> parse_power, and
// parse_primary/parse_matrix_fn/parse_matrix_literal/parse_scalar_span are
// all inlined into it. Depth 1 is the top-level call; each nested paren,
// function argument (parse_matrix_fn) or matrix-literal element
// (parse_matrix_literal) costs one more, since all three re-enter parse_expr.
//
// High-water marks against core 0's 4 KB. "leaf fix" = parse_scalar_span's
// strtod fast path + static span buffer, in the same commit as this cap.
//
//   Pico 1, uncapped            det([[1,2][3,4]])  d3  3,940  (156 margin)
//                               det(([a]*([c]+[d]))+[d]) d4  HARD FAULT
//   Pico 1, capped              det([[1,2][3,4]])  d3  4,012  ( 84 margin)
//                               d4  "Too deeply nested", no fault
//   Pico 2, capped, no leaf fix det([[1,2][3,4]])  d3  HARD FAULT
//                               (parse_power prologue, sp = __StackBottom+160)
//   Pico 2, capped + leaf fix   worst of five      3,860  (236 margin), correct
//
// The 3,940 -> 4,012 pair is the same input before and after the cap, so the
// +72 B is the guard's own cost (P gained `depth`, each level carries a
// DepthGuard: cycle 808 -> 832 B/level).
//
// **The Pico 2 crash is why the leaf fix exists.** det([[1,2][3,4]]) and
// det(identity(2)) died at depth 3 while det([a]*[c]+[d]) was fine — the
// difference is a numeric literal *at maximum depth*, which parse_scalar_span
// was handing to eval_field, i.e. the whole tinyexpr engine at the deepest
// point of the recursion. D47's a0939bf fixed exactly this in complexexpr;
// matexpr has its own copy of that function and never got it. Removing it
// took the cycle to 600 B/level (Pico 1) and 536 (Pico 2), -232 either way,
// for +256 B of .bss. See the comment at parse_scalar_span.
//
// 3 is forced from both sides. Below it, shipped behaviour breaks —
// det(identity(2)) and matrix literals inside a function argument are both
// depth 3 and are pinned by test_matrix. Above it, depth 4 faulted.
//
// !! Margins are thin on both boards and the numbers above are not
// predictions — every attempt this session to derive a peak from frame sizes
// was wrong (by 360 B, then 560 B, always optimistic). Static frame sums are
// an upper bound on a *single* frame, not a usable model of a peak. Measure.
//
// The Pico 1 has not been re-measured since the leaf fix (board swaps are
// batched to stage closures). It does not need to be for safety: the leaf fix
// only ever *removes* stack from this path — literals bypass eval_field, every
// other span takes the same route minus a 256 B buffer — and the Pico 1 was
// already passing at 4,012 without faulting, so it can only improve. Only the
// 4,012 figure is stale, not the safety case.
//
// Still containment, not headroom. Real fixes: idea F, the unified evaluator,
// which retires this parser outright and should be built on an explicit
// evaluation stack; or moving core 0's stack out of its 4 KB scratch bank.
//
// One production entry point (home_screen.cpp) means one constant, unlike
// complexexpr's split kMaxParseDepth/kMaxParseDepthNested.
constexpr int kMaxParseDepth = 3;

Result evaluate(const char* input);

// The last matrix result ("MatAns") — empty until a matrix expression
// evaluates. The matrix editor shows it as a read-only slot.
const Array& mat_ans();
// Non-const MatAns access — for the persistence TU (matrices_persist.cpp)
// to restore it at boot. Application code uses mat_ans().
Array& mat_ans_mutable();

// MatAns persistence (/picocalc/matans.dat), so it survives a power
// cycle like the named matrices [A]..[J] do. Defined in the firmware-only
// matrices_persist.cpp (like the [A]..[J] persistence) so the host build
// stays storage-free. save_ans writes the current MatAns (call after a
// matrix result commits); load_ans restores it at boot. load_ans has the
// same all-or-nothing contract as MatrixStore::load — false while it
// still needs PSRAM (cold boot, D14), so the late-init loop retries — and
// never clobbers an in-session result once one exists.
bool save_ans(platform::Storage& storage);
bool load_ans(platform::Storage& storage);

// "[[1,2][3,4]]" (compact form per element); "...]]" when truncated.
void format_matrix(const Array& m, char* buf, size_t buf_len);

// Same layout as format_matrix, but real cells render as fractions p/q
// (>Frac, 4D.2) when a tight fraction exists, else the compact decimal.
// Complex cells keep the compact rectangular/polar form.
void format_matrix_frac(const Array& m, char* buf, size_t buf_len);

}  // namespace math::matexpr
