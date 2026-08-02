#pragma once

#include <cstdint>

#include "math/cas/expr.hpp"

// Inline CAS calls on the home screen (Phase 5, phase5-spec.md §10, task
// 4D.21). Recognizes a single top-level CAS call and runs the matching engine
// pass, returning a symbolic result. All parsing/simplification stays
// home-screen-only, Enter-rate — it never touches the numeric hot path.
//
// Recognized forms (var defaults to 'x' when omitted):
//   simplify(e)               canonical form
//   expand(e)                 distribute + binomial
//   factor(e [,x])            polynomial factoring
//   diff(e [,x [,n]])         nth derivative (n>=1, default 1)
//   integ(e [,x])             indefinite integral (no +C)
//   integ(e ,x ,lo ,hi)       definite integral (numeric value)
//   solve(e [,x])             symbolic solve; e may be an equation (a=b)
//
// The shape-based solve() split (P5-4): solve() with a numeric guess or bounds
// (>= 3 args) is the numeric solver's job (math::solveexpr, D28) — this returns
// kNone for it so the caller falls through to that path.
namespace math::cas {

enum class HomeKind : std::uint8_t {
    kNone,       // not a recognized CAS call — fall through to numeric paths
    kError,      // recognized, but failed (message in .error)
    kExpr,       // single-expression result (in .result)
    kSolutions,  // solve(): a solution set (in .solutions[0..count))
};

struct HomeResult {
    HomeKind kind = HomeKind::kNone;
    const char* error = nullptr;  // static message when kind == kError
    Expr* result = nullptr;       // kExpr
    Expr* solutions[8] = {};      // kSolutions
    int count = 0;                // kSolutions: number of solutions
    bool complex = false;         // kSolutions: any solution is non-real
    char var = 'x';               // the operation variable
    char op[10] = {};             // recognized op name ("diff", "solve", ...)
};

// Recognize + evaluate a single inline CAS call. Resets g_cas_pool first, so
// the returned Expr nodes are valid until the next top-level CAS operation.
// allow_complex gates complex roots for solve() (from the number mode).
HomeResult evaluate_home(const char* input, bool allow_complex);

}  // namespace math::cas
