#pragma once

#include "math/types.hpp"

// Sequence-mode evaluator (4D.6, phase4-spec §7.2, D38/P4-12): the
// three TI-style sequences u/v/w defined by recurrences over n —
// `u(n) = u(n-1) + 1`, cross-references like `0.5*v(n-1)`, two-step
// lags like `u(n-1)+u(n-2)` (Fibonacci), or explicit formulas in n.
//
// Lag references can't ride tinyexpr as-is, so begin() rewrites them
// into placeholder variables (`u(n-1)` -> `u1`, bound to memo storage
// via Engine::compile_with) and compiles each sequence once. value()
// advances all three sequences in lockstep from n_min with a rolling
// two-deep memo — a forward sweep (graph, table scroll-down) is O(1)
// per step, and cross-references at n-1/n-2 are already computed. A
// backward jump restarts from n_min (cheap at calculator scales,
// bounded by kMaxN).
//
// Seeds: seed1 = value at n_min; seed2 = value at n_min+1, consumed
// only when the sequence references an (n-2) lag. A sequence with no
// lag references evaluates its expression directly at every n
// (explicit formula; seeds unused).
namespace math::seqexpr {

constexpr int kSeqCount = 3;  // u, v, w
// Iteration bound: value(s, n) beyond n_min + kMaxN returns NaN (§9:
// a sequence table must stay in the low-millisecond range).
constexpr long kMaxN = 10000;

struct SeqDef {
    const char* expr[kSeqCount] = {};  // nullptr/"" = undefined
    double seed1[kSeqCount] = {};      // value at n_min
    double seed2[kSeqCount] = {};      // value at n_min+1 (lag-2 sequences)
    long n_min = 1;
};

// (Re)compile the definitions. A textual/seed/n_min match against the
// previous call is a no-op, so per-row callers (the table) stay cheap.
// Returns true when at least one sequence compiled.
bool begin(const SeqDef& def);

// Force the lockstep iterator to restart from n_min on the next
// value() call (graph recompute calls this so edits to referenced
// home-screen variables are always picked up for a full replot).
void refresh();

// True if `expr` is a syntactically valid sequence definition — the
// same lag-rewrite + engine compile begin() performs, but stateless (no
// iterator/compile-state side effects). Empty text is "valid". Lets the
// editor color an invalid row red without a graph recompute first;
// recursive refs like u(n-1) don't compile in the plain engine, so the
// editor's generic compile check would wrongly flag every recurrence.
bool compiles(const char* expr);

bool defined(int s);    // Compiled successfully (expr present + valid)
bool uses_lag2(int s);  // References any (n-2) lag (seed2 is consumed)
// Web-plot eligibility (4D.7): references ONLY its own (n-1) lag —
// no bare n, no cross-references, no (n-2).
bool lag1_only(int s);

// Sequence s at n. NaN when undefined, n < n_min, or past kMaxN.
// Binds the engine's 'n' variable slot during evaluation — callers
// save/restore vars['n'] around a sweep (same contract as x/t/theta).
double value(int s, long n);

// The web-plot map f(x): sequence s's expression with its own (n-1)
// lag bound to x. Only meaningful when lag1_only(s).
double map_value(int s, double x);

}  // namespace math::seqexpr
