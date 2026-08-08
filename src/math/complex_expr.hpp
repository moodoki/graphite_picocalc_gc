#pragma once

#include "math/complex.hpp"
#include "math/types.hpp"

// Home-screen complex expressions (Phase 4C, phase4-spec.md §5.2-5.4;
// decision D30). Complex values cannot flow through tinyexpr (double
// only), so — mirroring math::matexpr's precedent for matrices — this
// is a small recursive-descent evaluator, but over Complex instead of
// tagged scalar/matrix values:
//
//   i                          imaginary unit (reserved, D30)
//   3+2i  (1+i)^2  sqrt(-4)    arithmetic + the complex elementary set
//   sqrt/exp/ln/sin/cos/tan/asin/acos/atan/abs/arg/conj/real/imag(z)
//   <expr> -> a                store (real results only — Variables are
//                              calc_t; storing a non-real value errors)
//
// Anything else (pi, e, theta, ans, bare variables, and every other
// catalog function — ncr, factorial, round, the distributions, ...)
// is not special-cased here: unrecognized primaries are handed to
// math::eval_field as an opaque real span, exactly like matexpr's
// scalar subterms. That span can't itself go complex (eval_field is
// real-only), so e.g. `normal_cdf(...)` works fine inside a complex
// expression as long as its own arguments don't need to be complex.
//
// evaluate() is deliberately side-effect-free (no Ans/store writes) —
// unlike matexpr/listexpr, it doubles as a cheap probe: the home
// screen calls it speculatively on ordinary REAL-mode expressions to
// upgrade a NaN result (e.g. sqrt(-4)) into a proper "Non-real result"
// domain error, without committing anything until the caller decides
// which path's result to keep.
namespace math::complexexpr {

struct Result {
    bool ok = false;
    Complex value;
    // ->a store target requested (0-25), else -1. Only meaningful when
    // ok && value.is_real() — the caller applies the store itself.
    int stored_var = -1;
    const char* error = nullptr;  // Static string when ok == false
};

// Parse-nesting caps (D47). This parser recurses ~368 B per level and core 0
// has 4 KB before core 1's stack, so the affordable depth depends on how deep
// the *caller* already is — hence two values rather than one constant buried
// in the parser.
//
//   kMaxParseDepth        home screen / editor entry, ~1,200 B in.
//                         7 keeps the D45 nesting ladder working to rung 6
//                         (rung N needs N+1) and `2^2^2^2^2`; margin ~200 B.
//   kMaxParseDepthNested  reached from inside list or matrix evaluation
//                         (list_expr's complex literal and clift paths,
//                         mat_expr's scalar spans), ~2,400 B in — only 4 fit.
//
// Over-cap input returns "Too deeply nested" instead of walking off the stack.
constexpr int kMaxParseDepth = 7;
constexpr int kMaxParseDepthNested = 4;

Result evaluate(const char* input, int max_depth = kMaxParseDepth);

// True if `s` contains the standalone identifier `i` (imaginary unit)
// or a numeric-literal-adjacent form like `2i` — the trigger for
// routing an expression through this evaluator instead of the plain
// real engine (spec §5.2).
bool mentions_i(const char* s);

}  // namespace math::complexexpr
