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

Result evaluate(const char* input);

// True if `s` contains the standalone identifier `i` (imaginary unit)
// or a numeric-literal-adjacent form like `2i` — the trigger for
// routing an expression through this evaluator instead of the plain
// real engine (spec §5.2).
bool mentions_i(const char* s);

}  // namespace math::complexexpr
