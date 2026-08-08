#pragma once

#include <cstddef>

// Exact-form (surd) display for home-screen results — Phase 5 Stage 4,
// phase5-spec.md §10.1, tasks 4D.23/4D.24.
//
// This is a display-path feature, not an always-on symbolic evaluator: it
// never touches evaluate_real() or any graphing/table/stats path (spec §13
// Risk 3). The home screen computes its ordinary numeric result first, then
// calls exact_form() as a second, cheap, Enter-rate-only probe that can only
// ever change what is *shown*. Ans, stores and variables all continue to come
// from the numeric result. This mirrors the pattern Phase 4C's complexexpr
// REAL-mode probe established (D30 §4).
namespace math::cas {

// Try to express `input` as an exact closed form: rational coefficients plus
// sqrt/pi, e.g. sqrt(8) -> "2*sqrt(2)", pi/2 -> "pi / 2", 2/6 -> "1/3".
//
// `numeric` is the value the ordinary numeric path already produced. The
// exact form is accepted only if it agrees with `numeric` to 1e-9 relative,
// which makes any CAS-vs-tinyexpr parser divergence (implicit multiplication,
// DEG-mode trig, ...) unable to change a displayed answer.
//
// Returns false — leaving `out` untouched — whenever the decimal should
// stand, which is the overwhelmingly common case. Callers must treat false as
// "display unchanged", never as an error.
//
// Pool discipline (D41): resets g_cas_pool, does all its work, and serializes
// into `out` before returning, so no Expr pointer outlives the call and the
// shared scratch kCompute region is free again on return.
bool exact_form(const char* input, double numeric, char* out, std::size_t out_len);

}  // namespace math::cas
