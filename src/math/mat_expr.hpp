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
// Measured on the Pico 1, high-water marks against core 0's 4 KB:
//
//   BEFORE this cap
//     depth 2  det([A]*[B]+[C])              3,492 of 4,096   (604 margin)
//     depth 3  det([[1,2][3,4]])             3,940 of 4,096   (156 margin)
//     depth 4  det(([a]*([c]+[d]))+[d])      hard fault, sp below __StackBottom
//   AFTER
//     depth 3  det([[1,2][3,4]])             4,012 of 4,096   ( 84 margin)
//     depth 3  det(identity(2))              3,540 of 4,096
//     depth 4  det(([a]*([c]+[d]))+[d])      "Too deeply nested", no fault
//
// The 3,940 -> 4,012 pair is the same input before and after, so the +72 B
// is the guard's own cost, not a difference between expressions. The matrix
// literal is the heavier of the two depth-3 shapes.
//
// So ~448 B/level in practice — the static frame sum (808) overestimates,
// the four functions are not all live at once. These are measurements, not
// arithmetic: the depth-4 figure is the crash that prompted D48.
//
// 3 is forced from both sides. Below it, shipped behaviour breaks —
// det(identity(2)) and matrix literals inside a function argument are both
// depth 3 and are pinned by test_matrix. Above it, depth 4 faults.
//
// !! The depth-3 margin is 84 B, and the guard is *why* it is 84 and not
// 156: P gained `depth` and every level carries a DepthGuard, ~72 B across
// three levels. This cap converts a reachable hard fault into an error
// message, which is strictly better, but it is containment and it made the
// surviving margin worse. Depth 3 is not safe to build on. Frame reduction
// is now required, not optional — parse_power alone is 416 B holding matrix
// temporaries, cf. D47's eval_list_into (2,248 -> 32 B) — or idea F, the
// unified evaluator, which retires this parser outright.
// **Re-measure on hardware before growing anything on this path.**
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
