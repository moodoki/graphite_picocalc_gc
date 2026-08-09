#pragma once

#include <cstdint>

#include "math/complex.hpp"
#include "math/types.hpp"
#include "math/unified_eval.hpp"

// Home-screen entry point for the unified evaluator (Phase 5.2, task 5.2.10).
//
// This is the layer HomeScreen::evaluate_input's dispatch becomes: compile,
// run, format. It replaces lines 452-682 of home_screen.cpp — the matexpr /
// listexpr / scalar cascade, its REAL-mode probe, and the four result-rendering
// branches — with one call.
//
// It lives in math/ rather than in the screen for one reason: **display strings
// are the part 5.2.9's differential harness could not compare.** That harness
// compares values and committed state because formatting lived in a UI
// translation unit the host build cannot link. Here it can, so the strings get
// the same treatment as the values.
//
// The store glyph stays in the UI layer, which is why this returns a store
// *label* ("a", "theta", "l1", "costs", "[C]") rather than a finished echo.
// math must not include gfx (AGENTS.md's layering: apps -> ui -> math/render).
namespace math::unified {

enum class HomeKind : uint8_t { kError, kScalar, kList, kMatrix, kText };

struct HomeResult {
    HomeKind kind = HomeKind::kError;
    // kError: a static string. Everything else: `text` holds the formatted
    // value, with no store echo — the caller appends that from `store_label`.
    const char* error = nullptr;
    const char* text = nullptr;
    // "a" / "theta" / "l1" / "costs" / "[C]", or empty when nothing was stored.
    char store_label[8] = {};
    // What the run committed, so the caller can persist exactly what changed.
    Commit commit;
    // kScalar only: the real value, and whether an exact-form probe may run
    // over it (real, finite, and not a store — the same three conditions
    // home_screen.cpp:101 applies today).
    calc_t scalar_value = 0;
    bool exact_form_ok = false;
};

// `expr` has already been through the CAS probe, solve()/convert() substitution
// and >frac/>dec stripping — all input rewriting, all orthogonal to which
// evaluator runs, and all still owned by the screen.
//
// Commits on success (Mode::kCommit): Ans, the store target, MatAns, and any
// list a sort or mat2list wrote. The caller persists what `commit` names.
HomeResult evaluate_home(const char* expr, bool to_frac);

// Evaluate a scalar cell entry, committing nothing (5.2.11). The list and
// matrix editors used complexexpr::evaluate for this, relying on it never
// writing engine state — true of that evaluator by accident of design rather
// than by contract. Mode::kProbe makes it a contract.
//
// Returns false with *err on a parse failure or a non-scalar result. A complex
// value comes back whole; whether the caller may store it is the caller's rule
// (both editors reject non-real in REAL mode, D30).
bool evaluate_scalar(const char* expr, Complex* out, const char** err);

}  // namespace math::unified
