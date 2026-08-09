// The differential allow-list (5.2.9). Hand-maintained, one row per expected
// divergence, each naming its row in docs/notes/unified-evaluator-changes.md.
//
// This is not a suppression list. The harness checks it in both directions: a
// divergence with no row here fails, and a row here that stops diverging fails
// too. Adding a row is therefore a decision to record in the register, not a
// way to quiet a test.

// W6 — nesting past listexpr's kMaxRec of 3. The cap was a call-frame budget
// (D47); depth is operand-stack slots now.
{"sort_asc(sort_asc(sort_asc(sort_asc({3,1,2}))))", "W6"},
{"cumsum(sort_asc(sort_asc({3,1,2})))", "W6"},

// W7 — dim/eigenvals compose. matexpr::Value could not hold a list.
{"2*dim([A])", "W7"},
{"2*eig([A])", "W7"},

// W9 — matexpr's parse-depth cap of 3, the one that landed with 84 B of margin.
// The first of these is the input that hard-faulted the Pico 1 (D48).
{"det(([A]*([A]+[A]))+[A])", "W9"},
{"det(inverse(([A])))", "W9"},
{"((([A])))", "W9"},

// W15 — complexexpr's complex-only functions (conj/real/imag/arg) are reachable
// from the real path now that the builtin table is shared.
{"conj(a)", "W15"},

// W16 — listexpr's kMaxOperands term cap.
{"{1}+{2}+{3}+{4}+{5}", "W16"},

// E6/E7 — a rejected store target reports itself instead of falling through to
// tinyexpr as a bare syntax error.
{"2->Theta", "E6"},
{"{1}->ans", "E7"},

// E8 — an unknown identifier inside a list literal.
{"{1,foo}", "E8"},

// F1 — tinyexpr computes (-2)^2 as -4. The two shipped evaluators disagree on
// this input TODAY; the unified one agrees with complexexpr, which is right.
{"(-2)^2", "F1"},
