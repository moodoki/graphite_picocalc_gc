#pragma once

#include <cstddef>
#include <cstdint>

#include "math/array.hpp"
#include "math/engine.hpp"

// Home-screen list expressions (task 3A.5, spec §3.2; syntax D22).
// Lists cannot flow through tinyexpr (scalar values only), so this
// layer sits above the engine:
//
//   {1, 2, 3}            literal (elements are full expressions)
//   l1                   list reference (also `l1 -> l2` to copy)
//   <expr> -> l1         store a list result
//   sort_asc(l1)         in place when the arg is a bare list; value
//   sort_desc(...)       semantics on compound args
//   cumsum(X) delta_list(X) seq(expr, var, lo, hi, step)
//   sum(l1) prod(l1) length(l1)   scalar reductions (bare list arg),
//                        usable inside scalar expressions
//   sin(l1)+2*l2 ...     element-wise: any engine expression over
//                        l1..l6 is evaluated per element (vector lift)
//
// Anything that doesn't look like list syntax returns Kind::kNone and
// the caller falls through to the normal engine path.
namespace math::listexpr {

enum class Kind : uint8_t { kNone, kScalar, kList, kError };

struct Result {
    Kind kind = Kind::kNone;
    // kScalar: the full engine result (Ans/scalar store applied).
    EvalResult scalar;
    // kList: the value to display — an internal result array, or the
    // stored/sorted list slot.
    const Array* list = nullptr;
    int stored_list = -1;         // ->lk target, else -1
    bool lists_modified = false;  // Caller should persist this list (stored_list)
    const char* error = nullptr;  // kError (static string)
};

Result evaluate(const char* input);

// "{1,2,3}" (format_number per element); ",...}" when truncated.
void format_list(const Array& a, char* buf, size_t buf_len);

}  // namespace math::listexpr
