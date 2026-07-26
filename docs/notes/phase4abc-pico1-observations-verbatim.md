# Phase 4A-4C on-device observations — Pico 1 (2026-07-22)

Board: **Pico 1 (RP2040)**, same binary as the 3D.14 pass earlier this
session (`build/pico/picocalc_graphcalc.uf2`, HEAD `f9dbfb6`, Session 19
font/glyph build) — no reflash needed, since Phase 4A-4C (Sessions 16-18)
and Session 19 fonts were already part of that build. This is the first
time Phase 4A (matrices + solver), 4B (CALC menu / graph analysis), and 4C
(complex numbers) have been exercised on Pico 1 hardware at all — previously
verified on Pico 2 only.

Logged for a future session — **no fixes applied yet**.

## 4A — Matrix editor + basic operations

- Matrix editor (TAB cycle [A]-[J]+Ans, F7 DIM, F8 clear, cell edit/advance,
  bracket typing on the physical keyboard): works as expected.
- Matrix operations ([A]*[B], [A]^-1, [A]^T, [A](2,3), det/rank,
  inverse/rref/ref/augment/identity): works as expected.
- matrices.dat persistence across a power cycle: persists correctly.

**Findings raised, cross-checked against `decisions.md`:**

1. `[B]+2` and `[B]+ans` give a dim-mismatch error — user asked "is this
   correct?" **Confirmed intentional**: matrix arithmetic only defines
   scalar *multiplication* (`2*[A]`, `[A]/3`), not scalar addition;
   `[A]+[B]` (matrix+matrix) is the only `+` form. Matches TI-84 behavior
   (`[A]+2` errors there too). Not a bug.
2. Matrix names are case-insensitive (`[A]` same as `[a]`) — user noted
   this seems to contradict the general case-sensitive-input rule (D19).
   **Confirmed intentional**: decision D28 explicitly states "lowercase
   `[a]` accepted, uppercase displayed" as a deliberate, documented
   exception for matrix names specifically. Not a bug.
3. **`MatAns` typed on the home screen gives a syntax error.** Real
   finding, not yet investigated further than: there is no typeable
   `MatAns` home-screen token in the code at all — the last matrix result
   is only exposed as a read-only slot inside the matrix editor's TAB
   cycle (`[A]`-`[J]`+Ans). A TI user would likely expect to reference the
   last matrix result inline (the way scalar `ans` works on the home
   screen) — flagged as a UX gap/feature gap, not a regression of
   existing behavior.

## 4A — Solver + big-matrix performance

- Solver form screen (Lower/Upper bound, optional Guess, residual +
  iterations): works as expected.
- Inline `solve(f,x,lo,hi)` / `solve(f,x,guess)` / `solve(lhs=rhs,...)`:
  works as expected.
- Big-matrix (>16x16, PSRAM tier) edit/op timing feel on the Pico 1:
  **feels fine** — no sluggishness, unlike the list editor/scatter-plot
  perf findings from the earlier 3D.14 (Phase 3) pass today.
- Regression (lists/stats/distributions/inference/graph unaffected):
  unaffected.

## 4B — CALC menu / graph analysis

- F6 CALC softkey / typed `calc`/`analyze`, cursor-riding curve pick, TI
  step prompts ("Left Bound?"/"Right Bound?"/"Guess?",
  "First curve?"/"Second curve?"): works as expected.
- Value/Zero/Min/Max/dy-dx/fnInt on a function, including tangent-line draw
  and shaded fnInt region (strip-render risk on this board): works as
  expected.
- Same CALC ops on a parametric pair and a polar curve, both angle modes:
  works as expected.
- Intersect on two curves, and the same-curve-refusal case: works as
  expected.

**Feature request raised:** fnInt shading should follow the curve's own
color (currently presumably a fixed shading color); asked whether an
alpha-blended look is possible, or an alternative like hatching/shading
lines if true alpha isn't feasible on this display.

## 4C — Complex numbers

- MODE "Number" row cycling REAL/a+bi/`r∠θ` and persisting: works as
  expected.
- REAL mode "Non-real result" wording (`3+2i`, `sqrt(-4)`, `(1+i)^2`, etc.)
  and a+bi mode arithmetic (`sqrt(-4)`->2i, `(1+i)^2`->2i, `e^(i*pi)`->-1,
  `abs(3+4i)`->5, conj/real/imag): works as expected.
- Store rules (`5->a` works, `2i->a` errors) and `r∠θ` polar display with the
  real ∠ glyph: works as expected.
- `eigenvals([A])` on a rotation-like 2x2 (`[[0,-1][1,0]]`) showing
  `{i,-i}` as text: works, **and this happens even in REAL mode** — user
  flagged this as worth double-checking. **Confirmed intentional per
  decision D30** ("matrix results aren't tied to the home-screen a+bi/polar
  setting"): eigenvalue display deliberately ignores the global number
  mode. User's own note: "might be ok if we decide to make complex first
  class" — worth revisiting if complex numbers get promoted to
  first-class storage later.
- Non-REAL mode still reaching the rest of the real catalog (ncr, round,
  distributions): not separately re-confirmed this round (covered by the
  same test as the item above).

## Fonts/glyphs (informal, not a full sweep)

Not run as a dedicated pass this session (user's call) — but complex
display glyphs, MODE row glyphs, pretty-print math glyphs, and stats
glyphs were incidentally seen during the Phase 4 testing above and
reported as looking correct.

## Open items raised, not covered above

None further — developer confirmed nothing else to add.
