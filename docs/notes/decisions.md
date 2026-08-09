# Architecture & Design Decisions

A running log of decisions made during development. Each entry captures the question, the choice, the rationale, and what was traded off. New entries go at the top.

Format:

```
## DXX: <decision title>

**Date**: YYYY-MM-DD
**Status**: Accepted | Superseded by DYY | Deferred
**Context**: <what triggered the decision>
**Decision**: <what was chosen>
**Rationale**: <why this over alternatives>
**Tradeoffs**: <what we gave up>
**Revisit when**: <conditions that should trigger reconsidering>
```

---


## D51: tinyexpr's unary minus binds to its own operand — the `(-2)^2` fix ships ahead of the unified evaluator, not with it

**Date**: 2026-08-09
**Status**: Accepted (bugfix)
**Context**: Phase 5.2's differential harness found that the two shipped
home-screen evaluators disagree about `(-2)^2` — **-4** from tinyexpr, **4** from
`complexexpr` — so today's answer depends on the number mode. **D50** (on branch
`phase-5.2`, not yet merged) scoped the fix out of that phase deliberately,
because the defect is upstream in the vendored parser and patching it *there*
fixes graphing, tables, stats and the solver as well, where 5.2 alone fixes only
the home screen.

That is also why it ships first. The unified evaluator has no hardware
verification yet (5.2.12 is the only task left in that phase) and may not land on
schedule; a wrong arithmetic answer in `evaluate_real()` should not be gated on
it. This is the third instance of the same disagreement class after **D46** and
**D50**, and the first one fixed at the source.

**Two defects, one mistake.** Both are in `drivers/tinyexpr/tinyexpr.c`'s
`#ifdef TE_POW_FROM_RIGHT` build of `factor()` — the branch we compile
(`CMakeLists.txt:62`) — and both come from letting a negation and a `^` swap
places.

*B1 — a parenthesised negation was hoisted out of the power.* `factor()` called
`power()` (which scans leading signs), then stripped any `negate` node off the
top and re-applied it after the `^` chain. By then `-2` and `(-2)` are the **same
node**: the parentheses are gone, so nothing distinguishes the negation the user
grouped from the one they did not. Upstream reports the same defect as
[issue #52](https://github.com/codeplea/tinyexpr/issues/52), `(-1)^0 == -1`.

*B2 — a negated exponent was re-based.* The right-associative insertion loop took
`insertion->parameters[1]` as the base of the next `^`. When that slot held a
`negate` node it descended *through* the negation, so `2^-3^2` built
`2^((-3)^2)` = 512 instead of `2^(-(3^2))` = 0.001953125.

**Neither was pinned by a test, and the reason is worth keeping.** `(-2)^3` = -8
and `(0-2)^2` = 4 come out right *by accident* — an odd exponent absorbs the
misplaced sign, and the second spelling never reaches the hoist. The suite
happened to sample the accidents.

**Decision**: restate the grammar so the sign is scanned in `factor()` at each
level, and build the chain through an insertion point so a negation always stays
**outside** the sub-chain it introduced:

```
<factor> = {("-" | "+")} <base> {"^" <factor>}

-2^2    ->  -(2^2)      = -4    (TI convention, unchanged)
(-2)^2  ->  (-2)^2      =  4    (was -4)
2^-3^2  ->  2^(-(3^2))          (was 2^((-3)^2) = 512)
2^2^3   ->  2^(2^3)     = 256   (right-associative, unchanged)
```

**Rationale — why iterative and not recursive.** The short fix is to make
`factor()` recurse on `^`, which is four lines and obviously correct. It was
rejected: it costs a stack frame per caret, and a `^` chain carries **no
parentheses**, so `Engine`'s only depth guard — `too_deeply_nested()`
(`engine.cpp:262`), a paren-count pre-scan capped at 7 — cannot see it. `2^2^2^…`
would recurse unbounded against core 0's 4 KB. That is D45/D47/D48's failure mode
exactly, and this project has now paid for it four times; a bugfix is not the
place to reintroduce it. The insertion-point form keeps `factor()`'s frame flat,
and a 500-caret chain parses without incident.

**This is a local fix to a vendored driver**, the first one, so `drivers/README.md`
gains the "Local modifications" section its own policy has been reserving. A
re-vendor must re-apply it — or, per the correction below, adopt upstream's.

**Verified before/after on the same Pico 2**, by flashing each build and
replaying one 32-expression corpus through Phase 5.1's serial injection —
which is what that tooling was built for. In REAL mode (the tinyexpr path),
14 rows flip and every "must not move" row is byte-identical:

| Input | before | after |
|---|---|---|
| `(-2)^2` | -4 | **4** |
| `(-1)^0` | -1 | **1** |
| `sqrt((-2)^2)` | NaN | **2** |
| `(-3)^2+1` | -8 | **10** |
| `2^-3^2` | 512 | **1/512** |
| `-2^2`, `-3^2`, `2^3^2`, `2^2^3`, `(0-2)^2`, `-2^2^2`, `3--2` | | *unchanged* |

`seq((-2)^n,n,1,4,1)` covers the **compiled** path — the same
`Engine::compile`/`eval_compiled` API graphing and tables use — and goes
`{-2,-4,-8,-16}` → `{-2,4,-8,16}`, while `seq(-2^n,…)` stays `{-2,-4,-8,-16}`.
The two are now distinguishable, which they were not.

**Tradeoffs**: two inputs lose a plausible-looking answer, and both are the bug's
other half rather than a new regression.

- `(-2)^0.5` was `-1.4142` — that is `-(2^0.5)`, not a root of -2. It is NaN now,
  which is what a real evaluator has to say. On the home screen nothing changes
  (the REAL-mode gate already answered "Non-real result", because the mode probe
  runs `complexexpr`); on graphing the point simply goes undefined.
- `(-8)^(1/3)` was `-2`, the real cube root, arrived at by accident. It is NaN
  now. TI-84 agrees (`ERR:NONREAL ANS` in REAL mode) and `a+bi` mode still gives
  the principal complex root, but there is **no `cbrt` in the catalog**, so REAL
  mode has no spelling for a negative cube root. Recorded here so a bench session
  does not rediscover it as a regression.

A third effect is an improvement worth naming: `exact.cpp`'s gate 5 requires the
CAS result and the numeric result to agree to 1e-9. They disagreed on these
inputs, so exact forms were silently suppressed — `1/(-2)^2` displayed `-0.25`
where it now displays an amber `1/4`.

**Revisit when**: upstream tinyexpr fixes #52 and we re-vendor — port their fix
if it is equivalent, keep ours if it is not. The larger question, whether the
unified evaluator should replace tinyexpr on the numeric path outright, is
unaffected and stays where D50 left it: after 5.2 closes, with §9's M1 measured.

### Correction, 2026-08-09: upstream had already fixed it, four days earlier

**This entry twice said upstream #52 was open. It was not** — it was closed
**2026-08-05**, and the claim came from a web search taken at face value instead
of the two API calls that would have checked it. Only the narrow reading
survives: the bug is present in our vendored `4a7456e` (2025-12-12), which
predates the fix. Corrected here rather than quietly, because the wrong version
sends the next reader looking for work that no longer exists.

**What actually happened.** tinyexpr had been dormant for years; #52 was filed in
July 2019, diagnosed correctly by the maintainer within a day — *"it will
optimize `(-1)^0` to `-1^0` to `-(1^0)`, which is very wrong"* — and then sat.
The same 2019 comment predicted the second defect before anyone hit it: *"there
are probably other cases such as `-a^-b^-c` that need special consideration."*
On 2026-08-05 the project woke up and landed 22 commits, including
[`1e2ba48`](https://github.com/codeplea/tinyexpr/commit/1e2ba481) "Fix unary
operator handling with TE_POW_FROM_RIGHT, fixes #52".

**Their fix covers both defects and is equivalent to ours**, reached by a
different route. They keep the sign scan in `power()` and record it in a new
`state::applied_unary` flag, so `factor()` hoists only a unary operator actually
written to the left of a `^`; we moved the scan into `factor()` instead. For B2
they move the insertion slot inside the unary node
(`slot = p_unary ? &p->parameters[0] : &insert->parameters[1]`), which is the
same tree ours builds. Both are iterative — nobody reintroduced recursion per
caret. Verified rather than assumed: our 46-expression corpus runs against
upstream `master` with **0 failures**, and their `smoke.c` re-enables a
`/* TODO POW FROM RIGHT IS STILL BUGGY */` block that had been commented out
since 2020, containing `2^-3^4` and `-2^-3^-4`.

**So nothing goes upstream.** No issue to file, no fix to contribute; a duplicate
would be noise. The good-citizen move is the opposite one — stop carrying a
private fork of a fix that now exists upstream.

**The follow-up is a re-vendor, and `drivers/README.md`'s policy already decides
it**: "port their fix if it is equivalent" — it is. But `master` is **22 commits
ahead** and not a drop-in, so this is scoped work rather than a drive-by:

- It merged the **logic branch**, so `!` is now prefix logical-not with its own
  `logical_not`/`notnot` node types. We use `!` as *postfix factorial*.
  `Engine::preprocess` rewrites every `!` to `fac(...)` before tinyexpr sees one,
  so the collision is contained today — but that containment becomes
  load-bearing rather than incidental, and `!=` would now mean something.
- [`a851f2b`](https://github.com/codeplea/tinyexpr/commit/a851f2be) **"Limit
  parser recursion depth, fixes #136"** — that is **D47's bug**, fixed upstream.
  Our `kMaxParseDepth = 7` paren-count pre-scan in `engine.cpp` is a workaround
  outside the parser; theirs is inside it and counts what actually recurses.
  Adopting it could retire ours.
- Plus locale-independent number parsing, `unsigned char` ctype fixes, ARMCC
  support, and a CI workflow.

Until that happens our local fix stands and the "Local modifications" row is
still correct — it just has a shorter expected life than it looked like.

## D50: tinyexpr stays on the numeric path — its `(-2)^2` bug is a separate bugfix, and replacing it outright is a post-5.2 question

**Date**: 2026-08-09
**Status**: Accepted (scoping)
**Context**: Phase 5.2's differential harness (task 5.2.9) found that the two
shipped home-screen evaluators **disagree today**: `(-2)^2` is `-4` from
tinyexpr and `4` from `complexexpr`, so the answer depends on the number mode
or on whether the expression happens to mention `i`. The cause is upstream, in
`drivers/tinyexpr/tinyexpr.c`'s `TE_POW_FROM_RIGHT` build of `factor()`, which
hoists a negation out of a power without knowing whether parentheses closed it —
by then `(-2)` is just a `negate` node, indistinguishable from `-2`. `(0-2)^2`
gives `4` on both paths.

This is the second instance of the class that justified Phase 5.2 (D46 was the
first), and it raised the obvious follow-on: if the unified evaluator gets this
right, should it replace tinyexpr everywhere rather than only on the home
screen?

**Decision**: Three parts.

1. **The unified evaluator stays home-screen-only for 5.2**, as
   `phase4-spec.md` §5.2 requires. Graphing, tables, stats and the solver keep
   `evaluate_real()`.
2. **The `(-2)^2` fix is a separate bugfix**, taken outside 5.2 and not gated on
   it. It is ~5 lines in the vendored parser and parse-time only.
3. **"Replace tinyexpr entirely" is revisited after 5.2 closes**, with §9's
   measured per-sample numbers in hand (spec P5.2-7).

**Rationale**: The guardrail's stated reason no longer applies, and saying so
matters: §5.2 argued that "making the default numeric path complex would double
arithmetic cost", which describes the design 4C considered — a `Complex` value
type — not this evaluator, where real ⊕ real never touches complex arithmetic.
So the original argument does not transfer. Four other costs do, and they are
the real ones:

- **A `Program` is a fixed 2,064 B**; a tinyexpr tree is malloc'd and
  proportional to the expression (~120 B for `sin(x)+2*x`). Today's graph screen
  compiles → sweeps → frees one at a time and would survive, but caching Y1-Y7
  compiled would cost 7 x 2,064 = 14.4 KB against ~1 KB — more than the phase's
  whole bss win.
- **`compile()` and `run()` are non-reentrant singletons**, safe only because
  nothing on the home screen re-enters them. The numeric path does
  (`list_ops.cpp:290` compiles inside an evaluation; `eval_field` reaches the
  engine at the leaf of another parser's recursion).
- **The scratch-arena invariant**: the evaluator's chunk staging overlays
  `scratch::kCompute`, whose other owners are `stats`, `matrix` and `infer`.
  They cannot collide today only because the evaluator is home-screen-only.
- **No differential coverage off the home screen.** The 5.2.9 harness and the
  change register cover home-screen expressions; graphing, tables and stats have
  no corpus, so every plotted curve would become an unverified regression
  surface. 5.2.9 found three bugs inside *covered* territory.

Measured, for whoever picks this up:

| | tinyexpr | unified evaluator |
|---|---|---|
| text | 7,897 B | 30,734 B (both TUs) |
| bss | 0 (heap trees) | 2,442 B + 2,064 per live `Program` |

Splitting the bugfix out is what lets the correctness win land without any of
that: patched at the source it fixes the home screen *and* graphing, where 5.2
alone fixes only the home screen.

**Tradeoffs**: Until the bugfix lands, 5.2 trades a home-screen disagreement
(REAL vs RECT) for a home-vs-graph one — `(-2)^2` will read `4` on the home
screen while `Y1=(-2)^X` still plots the tinyexpr reading. That is a smaller and
more visible inconsistency than the current one, but it is a real regression in
kind and is recorded rather than glossed. Deferring the replacement question
also means the project keeps two evaluators, and with them the possibility of a
third D46.

**Revisit when**: 5.2 closes and §9's M1 has measured per-sample latency for the
stack machine against tinyexpr. That number is the one input the decision needs
and nobody has it yet — the honest answer to "faster or slower" today is that
it is unknown.

**Amendment, 2026-08-09 (same day): part 2 is discharged — see D51.** The bugfix
was taken immediately rather than left on the wishlist, on `main` and released as
**v0.3.2**, so it is in the shipping firmware whether or not 5.2 ever closes.
Two consequences for this entry:

- **The tradeoff above no longer applies.** `(-2)^2` reads 4 on the home screen
  *and* plots as 4; the home-vs-graph inconsistency this decision accepted as the
  price of splitting never actually shipped, because the split half landed first.
- **The scoping argument held up, and stronger than written.** "~5 lines,
  parse-time only" was wrong about the size — patching at the source turned up a
  *second* defect in the same function (`2^-3^2` = 512), which 5.2 would not have
  fixed anywhere, because the unified evaluator gets that case right and nobody
  was comparing on it. Splitting the fix out is what made the vendored parser get
  looked at at all.

Parts 1 and 3 are unchanged: the unified evaluator stays home-screen-only for
5.2, and replacing tinyexpr outright still waits on §9's M1.

## D49: Integer powers of a complex base are computed, not approximated — and why the display-tolerance alternative was rejected

**Date**: 2026-08-09
**Status**: Accepted (bugfix)
**Context**: Found by Phase 5.1's serial injection on its first real sweep, which
is the point of that tooling. `(1+i)^2` displayed `1.224646799e-16 + 2i` where it
should be `2i`. Not a display artifact: `real((1+i)^2)` returned the epsilon and
`(1+i)^2-2i` propagated it.

`c_pow` had exactly one exactness branch — real base, real exponent, `std::pow`,
added by **D46** for the same class of defect (`10202^2` a hair off 104080804).
A **complex** base fell through to `c_exp(c_ln(base) * exp)`, which cannot
produce an exact zero component: `1.2246e-16` is precisely `2*cos(pi/2)` in
double. Confirmed against CPython, which reproduces our wrong answer bit for bit
via `cmath.exp(2*cmath.log(1+1j))` while `(1+1j)**2` gives exactly `2j` — it
special-cases small integer exponents for this reason.

The test suite did not catch it, and *why* is worth keeping:
`test_complex.cpp:82` already asserted `(1+i)^2 == 2i`, but with `tol = 1e-9`.
The assertion was right; the tolerance could not distinguish an exact zero from a
1e-16 one. **A tolerance chosen for "close enough" cannot test exactness.**

**Decision**: binary exponentiation for a real, integer-valued exponent of
magnitude <= 100, mirroring D46's real-base branch. `(1+i)^2` is now exactly
`2i` as a *value*. New tests assert with `tol = 0`, because exactness is the
property under test.

**Rationale — two architectural camps, and why camp 1.**

*Camp 1, never generate the epsilon* (adopted): compute integer powers by
repeated multiplication. What CPython and most numeric libraries do. True CAS
systems (Mathematica, Maple, TI-Nspire CAS, HP Prime CAS) reach the same place by
a different route — they expand `(1+i)^2` **symbolically** to `1 + 2i + i^2`, so
no float is involved and exactness is structural.

*Camp 2, suppress at display* (rejected here, recorded below): leave the value
and widen the formatter's zero test.

Camp 2 was rejected because **it does not fix the value**. `real()` and ordinary
arithmetic both observe the component, so snapping only at format time would have
printed `2i` while `real((1+i)^2)` still returned `1.2e-16` — a worse
inconsistency than the original, because the display would then be lying about a
number the user can still extract.

**Camp 2, recorded for if this ever becomes an issue.** Camp 1 has a real limit:
it only helps where an exact algorithm exists. `(1+i)^2.5`, `(1+i)^i`, and
transcendental compositions that *should* yield an exact zero still go through
`exp(ln)` and can still show an epsilon. No camp-1 system avoids that. If those
paths start producing user-visible artifacts, camp 2 is the remaining lever, and
these are the notes for doing it properly:

- **The formatter is already asymmetric about zero, and that is the bug camp 2
  would fix.** `format.cpp:184` tests `z.is_real()`, which is *tolerant*
  (`eps = 1e-12`), so a negligible imaginary part is snapped away and `(1+i)^4`
  printed a clean `-4` even before this fix. `format.cpp:203` tests
  `z.re == 0.0`, which is *exact*, so a negligible real part survived. One axis
  forgiving, the other not.
- **Use a relative test, not an absolute one.** `is_real(1e-12)` is scale-blind:
  for a result of magnitude 1e13 an imaginary part of 1e-11 is relatively
  negligible but absolutely above eps, and at magnitude 1e-13 everything looks
  real. The principled form is `|re| <= eps * |z|`.
- **It changes behaviour, not just presentation.** A genuine `1e-15 + 2i` would
  display as `2i`. That is the same trade the imaginary axis already makes, newly
  applied to the real one — defensible, but it is a decision about lying to the
  user in a documented way, not a cleanup.
- Apply it at *display* only. Do not let a tolerance leak into `real()`,
  `imag()`, comparisons or arithmetic, or the inconsistency simply moves.

**Tradeoffs**: a cap at |n| <= 100 (CPython draws the same line); beyond it the
`exp(ln)` path takes over and the result is correct but not bit-exact — pinned by
a test so the fallback is not accidentally removed. Repeated squaring costs
O(log n) multiplications, negligible on this path.

**Revisit when**: an epsilon artifact appears from a path camp 1 cannot reach
(non-integer or complex exponents, transcendental compositions). Then implement
camp 2 per the notes above rather than re-deriving them.

**User-facing documentation**: this belongs in the user docs, not only here —
the distinction between results that are *exact by construction* and results that
are *numerically approximated* is something a calculator user should be told
plainly, along with the honest statement that `(1+i)^2` is exact while
`(1+i)^2.5` is not, and why. Flagged for the `docs/site` branch's CAS/complex
chapter.

---

## D48: `matexpr` gets the depth cap it never had — and the margin it leaves makes idea F worth doing

**Date**: 2026-08-08
**Status**: Accepted (bugfix; containment, with a named follow-up)
**Context**: The post-D47 group-5 bench sweep on the Pico 1 (stack guards live)
walked every heavy path with `stack: peak` on serial. Four of five were clean —
idle 1,540, graph redraw + zoom 2,360, and the list editor / 1-Var stats /
inference set never registered a new high-water mark at all (the first hardware
confirmation of D47's `eval_list_into` rework). The D45 ladder in both REAL and
a+bi reached 3,588, which turned out to be *by design*: D45's static prediction
for the home-screen entry at cap 7 was 3,728 with 368 margin, so the live mark
agreed with frame arithmetic derived by inspection to within ~140 B.

Matrix was not clean. `det(([a]*([c]+[d]))+[d])` hard-faulted, reproducibly.
The fault record's `pc`/`lr` were garbage (both resolve to `??`) because the
overflow corrupted exception stacking, but `sp=0x20040ff8` named it alone: 8
bytes below core 0's `__StackBottom` (`0x20041000`), i.e. SP had crossed its own
floor into core 1's stack. `matexpr` was the last parser with no depth cap —
D45 capped the CAS parser, D47 capped tinyexpr and complexexpr, and three of
those four were found by something crashing.

**Decision**: `kMaxParseDepth = 3` in `mat_expr.hpp`, enforced by an RAII
`DepthGuard` in `parse_unary` — the single point every level of the
`parse_expr -> parse_term -> parse_unary -> parse_power` cycle passes through
exactly once. RAII because `parse_term`/`parse_expr` call `parse_unary` in a
loop and siblings must not accumulate; three host tests pin that specifically,
since a naive increment-without-unwind passes every depth test and breaks flat
expressions. One production entry point (`home_screen.cpp`) means one constant,
unlike complexexpr's split cap.

**Rationale**: 3 is forced from both sides, and only measurement found the
boundary. A first attempt at 2 — derived from frame arithmetic (1,028 prefix +
848 entry + 808/level, predicting depth 3 at 4,300 and therefore unreachable) —
broke two shipped behaviours that `test_matrix` already pinned:
`det(identity(2))` and matrix literals inside a function argument are both
depth 3, because `parse_matrix_fn` and `parse_matrix_literal` each re-enter
`parse_expr`. On hardware depth 3 turned out to *fit*: 3,940 of 4,096. The
arithmetic was 360 B pessimistic — the four frames are not all live at once —
and had it been trusted, the cap would have shipped one level too tight.
Repeating D47's lesson exactly: measure, don't reason about frame sizes.

Measured on the Pico 1 (`stack: peak` high-water marks, 4,096 total):

| | before | after |
|---|---|---|
| `det([A]*[B]+[C])` (depth 2) | 3,492 | — |
| `det([[1,2][3,4]])` (depth 3) | 3,940 | **4,012** |
| `det(identity(2))` (depth 3) | — | 3,540 |
| `det(([a]*([c]+[d]))+[d])` (depth 4) | **hard fault** | "Too deeply nested" |

**Tradeoffs**: The guard costs stack. `P` gained `depth` and every level carries
a `DepthGuard`: the Pico 1 cycle went 808 -> 832 B/level, +24 B/level, which
over three levels is the +72 B seen live (3,940 -> 4,012). The static tooling
and the hardware mark agree exactly here, which is worth noting after a session
in which frame arithmetic was wrong twice. **So the fix converted a reachable
hard fault into an error message while shrinking the surviving margin from 156 B
to 84 B.** That is strictly better than faulting and it is not comfortable:
depth 3 is now something to defend, not build on. Expressions a user could
reasonably write (`det(([A]+[B])*[C])` is depth 3 and fine; anything one level
deeper) are now rejected.

The Pico 2 is not better off. Same 4 KB core-0 stack (`0x20081000`-`0x20082000`)
with core 1 immediately below, identical layout to the Pico 1 — the RP2350's
extra SRAM does not reach the stack, which lives in a 4 KB scratch bank on both
chips. Its frames are ~8% cheaper (cycle 768 vs 832), worth ~290 B of headroom
at depth 3 against the ~768 B a whole extra level costs. So one constant serves
both boards; no board-conditional cap.

**PSRAM cannot help.** It is 8 MB of PIO-driven SPI and *not memory mapped*
(`psram.hpp`: `alloc()` returns offsets, access goes through `read()`/`write()`),
so there is no address a stack could target — on either board, since both use
the PIO driver rather than the RP2350's native QMI mapping. At ~200 us/KB it
would be unusable for a call stack regardless.

**Amendment, same day — the cap alone was not enough, and the Pico 2 proved
it.** Flashed to the Pico 2 (first time on this branch) and `det([[1,2][3,4]])`
and `det(identity(2))` — both depth 3, both *allowed* by the cap — hard-faulted,
while `det([a]*[c]+[d])` was fine. This board's fault reporter gave a real PC
where the Pico 1's had given garbage: `parse_power`'s prologue
(`mat_expr.cpp:625`) called from `parse_unary`, `sp = __StackBottom + 160`.

The discriminator is a **numeric literal at maximum depth**. `parse_scalar_span`
put a `char span[256]` on the stack and handed it to `eval_field` -> the whole
tinyexpr engine (`Engine::compile`/`compile_with`, 280/288 B each) — at the
*leaf* of the recursion, i.e. the deepest point on the stack. **This is D47's
bug verbatim**: `a0939bf` fixed exactly it in `complexexpr`, but `matexpr` has
its own copy of that function and never received the fix.

Fixed the same way: `strtod` for a plain literal (what tinyexpr would have used
anyway), and `span` becomes `static` — non-reentrant by the same argument, since
`parse_scalar_span` consumes a terminal span and neither `eval_field` nor
`complexexpr::evaluate` re-enters this parser. **Cycle 832 -> 600 B/level
(Pico 1) and 768 -> 536 (Pico 2), -232 on both, for +256 B of `.bss`** (211,100
-> 211,356). Pico 2 re-verified: all five expressions correct, no fault, worst
case **3,860 of 4,096 (236 margin)**.

Two corrections to the entry above. First, the Pico 2 is *not* simply better
off — it faulted where the Pico 1 survived, despite every statically-reported
frame being smaller and its idle baseline 304 B lower. Second, part of why:
**`size-report.sh` does not count FP register saves.** The Pico 1 image has zero
`vpush` instructions; the Pico 2 has 19, including inside `math::eval_field` on
the crash path. Every Pico 2 frame figure in this entry is therefore low by an
unquantified amount, and the tool should be taught to count `vpush`.

The Pico 1 has not been re-measured since the leaf fix — board swaps are batched
to stage closures — and does not need to be for safety: **the leaf fix only ever
removes stack from this path**, and the Pico 1 was already passing at 4,012
without faulting, so it can only improve. The 4,012 figure is stale; the safety
argument is not.

**Method note worth keeping.** Three separate attempts this session to derive a
peak from frame sizes were wrong, always optimistic: depth 3 predicted
unreachable (it fit, 360 B error), the Pico 2 predicted ~3,500 (it crashed), and
the post-fix Pico 2 predicted ~3,300 (measured 3,860, 560 B error). Static frame
sums bound a *single* frame; they are not a model of a peak. Prefer measurement,
and where a board isn't available prefer **monotonic arguments** ("this can only
remove stack") over predictions.

**Revisit when**: depth 3 proves too restrictive in real use, or anything on
this path grows a frame. Note the cap was sized against the *old* 832 B/level
frames and may now be conservative — but raising it needs a measurement pass on
both boards, not arithmetic. Three levers, cheapest first: (a) frame reduction —
`parse_power` alone is 388-416 B holding matrix temporaries, cf. D47's
`eval_list_into` at 2,248 -> 32 B, worth maybe 2-3x the depth; (b) move core 0's
stack out of the scratch bank into main SRAM via the linker script — raises the
ceiling without touching frames, comfortable on the Pico 2 and tight on the
Pico 1's ~52 KB headroom, a whole-firmware change of the same class as
`PICO_USE_STACK_GUARDS`; (c) an explicit-stack iterative parser, whose depth is
bounded by an array rather than by frames — and that array *is* PSRAM-friendly,
being accessed sequentially rather than as a call stack.

**(c) belongs to idea F, not to `matexpr`** (decision 2026-08-08): the unified
evaluator retires this parser outright, so building an explicit-stack rewrite
here would be thrown away. F's case is now two independent arguments, not one —
D46's correctness argument (the real and complex evaluators silently disagreed
about DEGREE-mode trig since Session 18) and this session's structural one (four
parsers, four separately-discovered depth budgets, three of them found by a
crash). F should be designed with an explicit, PSRAM-capable evaluation stack
from the start rather than inheriting a fourth frame budget.

---

## D47: Stack frames are a budget — no compiling inside render(), no kMaxLen arrays on a recursive path, and a trap where core 0 meets core 1

**Date**: 2026-08-08
**Status**: Accepted (bugfix; the Y= lockup reported 2026-08-05, plus a second
instance of the same class found while fixing it)
**Context**: From the bench: "the Y= function editor freezes the calculator,
every time, on Pico 1 — only the first few pixel rows of the header render,
then keys are dead and it needs a physical power cycle." Testing stopped there,
so the whole Phase 5 CAS on-device checklist sat behind it.

The cause is the bug class **D45** named and then explicitly warned about:
"graph rendering, matrix ops and Phase 6's MicroPython heap may currently work
*because* nothing traps." Core 0's stack is the 4 KB `SCRATCH_Y` bank, sitting
directly on top of core 1's `SCRATCH_X`. Overrun does not fault; it walks
straight into core 1's live frames, starting at its outermost one.

`SlotEditorScreen::render()` coloured each row red-or-white by calling
`field_valid()` → `math::engine().compile()` — **inside the renderer**. Frames
measured on the linked Pico 1 ELF:

| frame | bytes |
|---|---|
| `main` | 216 |
| `ScreenManager::render_frame` + `Framebuffer::render_frame` | 80 |
| `SlotEditorScreen::render` | 120 |
| **`Engine::compile`** | **2,232** |
| `preprocess` (peer of the parse) | 560 |
| `te_compile` + one parser nesting level | 56 + 160 |

`Engine::compile`'s 2,232 B is almost all `te_variable lookup[kLookupCount]` —
122 entries x 16 B = 1,952 B, rebuilt on the stack on *every* compile.
`eval_internal` and `compile_with` each carried their own copy.

Every reported symptom follows. Strip mode renders in 16-px bands and the
header bar is exactly `kStatusH` = 16, so strip 0 pushes fine; core 0 then
renders strip 1 **while core 1 is mid-DMA on strip 0**, overruns into its
stack, and core 1 dies. Core 0 blocks forever in `wait_one_ack()`, and since
keys are polled on core 0, input dies with it. Every time, because the stored
expressions are the same each boot. The graph screen survives the same
expressions because `recompute_function` sits ~200 B shallower *and* runs
outside `render_frame`, with core 1 parked.

**Decision**: four changes, in the order they matter.

1. **`render()` only draws.** `SlotEditorScreen` caches field validity in a
   `valid_mask_` bitfield, refreshed from `on_activate()` and after each
   mutating key. This is the contract `list_editor.hpp` has documented since
   Phase 3A ("refresh_cells() from on_key/on_activate; render() only draws");
   the slot editors never got it. Secondary win: ~140 `te_compile` calls
   (each with its own mallocs) per frame, gone.

2. **The tinyexpr binding table moves to bss.** It is stable after startup —
   `a`-`z` bind to the singleton's `Variables`, and `constants()`/`catalog()`
   return constexpr tables — so it is built once. Safe to share because
   `te_compile` reads it only while parsing and engine compiles never nest.
   `Engine::compile` **2,232 → 280 B**, `eval_internal` likewise,
   `compile_with` 2,368 → 288 B, firmware-wide.

3. **The same disease, worse, on the list path.** The new frame report (below)
   immediately found it: `HomeScreen::evaluate_input` (872) →
   `listexpr::evaluate` (1,192) → `eval_list_into` (**2,248, and recursive**)
   = 4,312 B at recursion depth 1. A plain `{1,2,3}` on the home screen was
   already overrunning, silently, on a path HW-verified since Phase 3A. Fixed
   by keeping the leaf evaluators out of line (`noinline` on `eval_literal`,
   `eval_seq`, `eval_range`, `eval_clift` — inlined, their `kMaxLen` locals,
   `eval_seq`'s `arg[5][256]` worst of all, were charged once per level
   instead of once), moving every non-reentrant `kMaxLen` buffer to bss, and
   making the buffers that genuinely are per-level **depth-indexed** —
   the pattern `g_temp[kMaxDepth]` already used in that file.

4. **A hard recursion cap, and a trap.** `eval_list_into`'s recursion is now
   bounded in the function itself (`RecGuard`, `kMaxRec = 3`) rather than at
   individual call sites: the old `ctx.depth` only ever covered the
   cumsum/delta path, so `sort_asc(sort_asc(sort_asc(...)))` and the lift path
   through `extract_operands` both recursed uncounted. And
   `PICO_USE_STACK_GUARDS=1` + `PICO_STACK_SIZE=4096` put an MPU trap exactly
   at `__StackBottom == __StackOneTop`, with `isr_hardfault` recording the
   faulting PC in a watchdog scratch register and rebooting, so the next boot
   prints `fault: previous boot hard-faulted at pc=0x...`. Without that
   handler the guard would convert silent corruption into an indistinguishable
   lockup — the SDK's default handler is an infinite loop.

**Results** (Pico 1, measured):

| path | before | after |
|---|---|---|
| Y= editor render | ~3,200+, overrunning | **424** |
| home-screen list expr, worst case at the cap | ~4,300+, overrunning | **3,152** of 4,096 (944 margin) |
| `Engine::compile` frame | 2,232 | 280 |
| `eval_list_into` frame (per level) | 2,248 | 32 |

**Rationale**: shrinking `Engine::compile` alone would have fixed the Y=
lockup, but it would have left the actual defect — a 2 KB frame under a
renderer that runs ~20x a frame — in place for the next thing to trip over.
Caching validity fixes the design error; hoisting the table fixes the
firmware-wide exposure. Both were needed before the guard could be switched
on, and the guard is what stops the next instance being another silent
lockup.

**Tradeoffs**:

- **bss +10,284 B on the Pico 1** (198,836 → 209,120), trading SRAM for stack.
  Headroom 61.8 → 51.8 KB. This eats into the **Phase 6 margin specifically**:
  the MicroPython heap is 48 KB, so the spare drops from ~14 KB to ~4 KB. The
  three documented levers (heap 48→40 KB, ArrayStore slab cut, `g_chunk` fold,
  `pre-phase5-review.md`) are worth far more than this and are now more likely
  to be needed. A reclaim pass on the new statics is possible too — the four
  leaf scratch blocks are provably never simultaneously live and could be
  unioned for ~2.3 KB, deliberately not done here because a wrong aliasing
  union is the same class of bug this entry is about.
- The reported `size` figure jumps a further **4,096 B** that is *not* real:
  `PICO_STACK_SIZE` 2048 → 4096 doubles both `.stack_dummy` sections. Those
  live in the dedicated `SCRATCH_X`/`SCRATCH_Y` banks and were never
  allocatable. Compare `.bss` alone across this change, not `size`'s total.
- `kMaxRec = 3` makes deeply nested list expressions an error. The boundary
  case the code documents, `cumsum(sort_asc({...}))`, is exactly at the cap
  and still works; a fourth level now says "Too deeply nested". It used to
  corrupt core 1's stack instead.
- **The stack guard is a tripwire, not a barrier.** On RP2040 it is a single
  32-byte MPU subregion at `__StackBottom`
  (`runtime_init_stack_guard.c:14-32`), so it reliably catches gradual or
  recursive growth — the D45 and item-3 cases — but one oversized `sub sp, #N`
  can step straight over it. `scripts/size-report.sh`'s new frame listing is
  the deterministic half of the check: it walks prologues out of the ELF and
  reports anything over a threshold, which is exactly the measurement that
  root-caused this bug and found the list-path instance.

### What the hardware pass added: tinyexpr had no depth cap either

Flashed to the Pico 1 the same day. The boot was clean and the guard did not
trip through init — but **F1/F4/F5 still failed**, now as "black screen, then
back to the home screen" rather than the original dead-keys lockup. That is
the fault handler working: serial on the next boot read

```
fault: previous boot hard-faulted at pc=0x100551da
```

which resolves to `factor+0xa` in `tinyexpr.c` — the `push {r5, r6, r7, lr}`
**prologue** instruction. A fault on the prologue push is unambiguous stack
overflow, and it exposed the gap this entry had missed: **D45 gave the CAS
parser stated depth caps; tinyexpr's parser never got one.** Its recursive
descent costs **200 B per nesting level** (measured
`list`+`expr`+`term`+`factor`+`power`+`base`), with no limit of its own, so
depth is whatever the input says.

The input, in this case, was the user's own: `testdrive-2026-08-02` recorded
perf stress probes of "up to 20 nested trig calls", and one was still sitting
in Y1. The Y= path allows 16 levels. That is why the editor failed *every
time*, and why F4/F5 failed too — `recompute_function` compiles the same slot.

**`kMaxParseDepth = 8`**, checked in `eval_internal`/`compile`/`compile_with`
after `preprocess`. Sized to the tightest caller, not to taste: the list-lift
path (`evaluate_input` → `listexpr::evaluate` → 3x `eval_list_into` →
`eval_lift` → `compile_with`) leaves ~1,696 B for the parser, so eight levels.
The Y= path alone would allow sixteen, but one cap has to hold everywhere.
Over-deep input is now a parse error — the row draws red and stays editable.
`test_math` 235 → 242 covers depths 9/20/40 through both `compile` and
`evaluate`.

**HW-verified on the Pico 1**: Y= opens and renders, Y1 draws red (the stress
probe, correctly rejected), the graph works, and three consecutive
`graph recompute:` lines came in at 103,163 / 103,107 / 103,019 us with no
fault — the same path that had been faulting deterministically.

The sequencing is the lesson worth keeping: the guard plus the fault reporter
turned a second, still-unknown instance of this bug class into a PC and a
one-line diagnosis, on the first flash. Shipping the guard without the handler
would have produced another indistinguishable lockup.

### Third parser, and why this one needed two caps

Same session, immediately after: `math::complexexpr` was flagged above as the
obvious next uncapped parser, and it is — but it does not take the same
treatment. It has **two** recursion cycles, both routed through `parse_unary`,
which is where the guard therefore lives:

1. paren/function nesting — `parse_primary` -> `parse_expr` -> `parse_term`
   -> `parse_unary`;
2. right-associative `^` — `parse_power` -> `parse_unary`, **with no
   parentheses at all**, so `2^2^2^2^...` nests once per caret.

Cycle 2 is why the paren-depth pre-scan used for tinyexpr would not do here: a
string scan cannot see it. (tinyexpr has no such cycle — its `factor` builds
right-associativity iteratively via an insertion pointer, verified in source,
so `kMaxParseDepth`'s proxy is sound.)

The cap could not be a single number. At **360 B/level**, the affordable depth
depends on how deep the caller already is, and the two entry points differ by
1.2 KB: the home screen enters ~1,208 B in, while list and matrix evaluation
reach the same parser ~2,400 B in (`eval_literal` for `{1+i,...}` elements,
`eval_clift` for `2i*l1`, `eval_matrix_body` for scalar spans). One
conservative value would have had to be 4 — and rung N of the D45 ladder needs
depth N+1, so 4 would have broken `test_real_pow_exact`'s rung-4 case, which is
a deliberate D45/D46 regression test. So the cap belongs to the *entry point*:
`kMaxParseDepth = 7` (default) and `kMaxParseDepthNested = 4`, passed as a
defaulted argument to `evaluate()`.

Measured, all three paths:

| path | prefix | cap | total | margin |
|---|---|---|---|---|
| home screen | 1,208 | 7 | 3,728 | 368 |
| via `list_expr` | 2,400 | 4 | 3,840 | 256 |
| via `mat_expr` | 2,056 | 4 | 3,496 | 600 |

Getting the list prefix down to 2,400 took the same bss treatment on
`listexpr::evaluate` (720 -> 280 B) and `eval_clift` (360 -> 176 B). **One of
those was wrong and `test_lists` caught it**: making `eval_clift`'s
`CTerm terms[kMaxCTerms]` static broke `2i*l1` with a segfault, because only
`.sign` is assigned per use — `.scalar{1.0, 0.0}` and `.list = nullptr` come
from default member initializers that run per call for a stack local but
*once* for a static, so the second call saw the previous call's accumulated
scalar and a stale `Array*`. Reverted to the stack; 192 B is worth paying, and
a static silently carrying state between calls is the same class of defect this
entry exists to remove. Worth remembering when the leaf-scratch union below is
picked up.

`test_complex_expr` 113 -> 122. Pico 1 `.bss` 209,120 -> 209,888.

**Still uncapped: `math::matexpr`'s parser**, and it is the worst of the three
at **808 B/level** (`parse_power` alone is 416 B, holding matrix temporaries)
with only ~2 levels of headroom from the home screen. Not touched here — it
needs its own measurement pass and probably frame reduction before a cap can be
set that does not break ordinary matrix expressions.

### The actual root cause of the 4-paren crash, and three wrong answers first

Everything above fixes real defects, but **none of it fixed the crash the
bench kept reporting**: four nested parens hard-faulted the board, in every
number mode. Recording the sequence because the *method* was the problem.

Three wrong attributions, each from reasoning about static frame sizes
instead of measuring:

1. **`matexpr`** — ruled out by reading: it early-outs at `mat_expr.cpp:918`
   when the input has no matrix token, so it never sees `((((1+1))))`.
2. **`build_layout` inside `HomeScreen::render()`** — real, and the
   measured +376 B steps seemed to confirm it. But the step was
   complexexpr's 368 B/level, not the layout parser's 476. Cutting the
   layout path from 376 to 96 B/level moved the observed peak *not at all*,
   which is what exposed the mistake.
3. **complexexpr's recursion depth** — the cap was too generous, but depth
   was not what overflowed.

What settled it was building the right instrument, several rounds later than
it should have been: a **crash record in `.uninitialized_data`** holding the
whole exception frame. It survives the warm reset the handler triggers, where
two packed watchdog scratch words could only report "implausible" — and
implausible was exactly what these faults were. With the full frame:

```
fault: core 0  pc=0x10004f6e lr=0x20041490 sp=0x20040fe0
```

`sp` is 32 bytes below `__StackBottom` — one exception frame — so the guard
trapped cleanly, and `pc` resolves to `preprocess+0x12`, `engine.cpp:28`: the
prologue allocating `char tmp[256]` in a 560 B frame.

**The cause was the leaf, not the recursion.** `HomeScreen::evaluate_input`
runs `complexexpr::evaluate` as a probe on *every* input (to turn a NaN into
"Non-real result"), which is why this reproduced in REAL mode too. And
`parse_scalar_span` handled **every numeric literal** by copying it into a
buffer and calling `eval_field` — which runs the entire tinyexpr engine:
`Engine::evaluate` -> `eval_internal` -> `preprocess`, ~1,220 B. That sat at
the *leaf* of a 360 B/level recursion, i.e. at maximum stack depth.
`((((1+1))))` came to ~1,200 prefix + 4x360 + ~1,500 leaf, over 4,096. Three
parens fit; four did not.

Fixed by parsing plain numeric literals with `strtod` (which is what tinyexpr
would have used anyway) and moving the fat leaf buffers to statics — all
non-reentrant by inspection: `preprocess`'s `tmp`/`rebuilt`,
`Engine::evaluate`'s `body`, `eval_internal`'s `processed`,
`parse_scalar_span`'s `span`.

| frame | before | after |
|---|---|---|
| `preprocess` | 560 | 48 |
| `Engine::evaluate` | 316 | 64 |
| `eval_internal` | 280 | 32 |
| complexexpr per level | 360 | 240 |
| `eval_field` leaf | ~1,220 | 208 |

Worst case at the cap is now **3,128 of 4,096**, 968 B margin. HW-verified.

**Two things worth keeping from this.** First, the earlier garbage fault PCs
(`0xe6fd6f2e`, `0x998c9015`) were not noise: at 376 B/level a single
`sub sp, #N` leaps clean over the 32-byte guard window, so core 0 silently
killed core 1 instead of trapping, and the "PC" was core 1's corrupted return
address. The layout fix shrinking frames to 96 B/level is *why* the crash
finally trapped cleanly and named itself — an accidental but real benefit.
Second, the guard's own failure mode bit hard: a first cut of `paint_stack()`
wrote into the guarded 32 bytes, faulted on its first store, and boot-looped
the board past USB enumeration — unrecoverable without the physical BOOTSEL
button. Hence `kFaultsBeforeBootsel`: three consecutive faults now call
`reset_usb_boot()` so a boot-looping image parks itself in BOOTSEL.

Re-deriving both caps against the new frames also caught `kMaxParseDepth = 8`
overshooting its tightest caller (the list-lift path affords 7). Both parsers
now stop at **7 parens** — Engine counts parens, complexexpr counts recursion
levels where the top-level call is level 1, so its constant is 8 for the same
user-visible limit.

**Revisit when**: the frame report shows a new function over ~1 KB, or the
Phase 6 budget gets tight enough that the leaf-scratch union is worth taking.
Also revisit the parse caps if 7 parens ever rejects an expression someone
actually wanted — the home path alone affords 13 (Engine) and 10
(complexexpr); the binding constraint is the list-lift chain, which enters
~1,100 B deeper — it is bounded by the list-lift prefix, so shrinking
`listexpr::evaluate` (720 B) or `HomeScreen::evaluate_input` (568 B) buys
levels back. **`math::complexexpr` has its own recursive-descent parser
(`parse_unary` 232 B/level) and is still uncapped** — same class, reachable in
non-REAL number mode; it did not surface here but it is the obvious next one.
If core 0's 4 KB ever becomes the real constraint rather than individual
frames, the structural move is to relocate core 1's stack out of `SCRATCH_X`
(`multicore_launch_core1_with_stack`) and give core 0 a custom linker region —
deliberately not attempted here.

---

## D46: The complex evaluator must not disagree with the real one — DEGREE-mode trig, and exact real powers

**Date**: 2026-08-05
**Status**: Accepted (bugfix; Phase 4C defect found during the Phase 5 Stage 5
hardware pass)
**Context**: Reported from the bench: "trig functions in deg mode give results
identical to radian mode." Root cause is `src/math/complex.cpp` — `c_sin(z)`
is `{std::sin(z.re) * cosh(z.im), ...}`, calling `std::sin` on the raw value.
The real evaluator's trig goes through `functions.cpp`'s `rad()`, which
applies the degree scaling; the complex evaluator never did. So whenever
Number mode is RECT or POLAR (which routes home-screen evaluation through
`complexexpr`), **every trig call answered in radians and the MODE row's
DEGREE setting was silently ignored**. Confirmed on the host:
`sin(30)` in DEGREE gave `0.5` from the real path and `-0.9880316241` from the
complex path.

This is **not** a Phase 5 regression — `complex.cpp` predates it and has
shipped since Session 18 (Phase 4C). It surfaced now only because the Stage 4
verification script sends the tester into RECT/POLAR mode.

**Decision**: add angle-mode wrappers (`m_sin`/`m_cos`/`m_tan` scaling in,
`m_asin`/`m_acos`/`m_atan` scaling out) in `complex_expr.cpp`'s `kFns` table.
`complex.cpp`'s `c_*` functions stay pure math. The whole complex argument is
scaled, not just its real part (TI-89's behavior).

**Rationale**: `complex_expr.cpp`'s table is the only caller of the `c_*`
trig functions, so the wrapper layer is complete by construction. Putting the
scaling at the evaluator boundary mirrors where the real path already does
it, and keeps a `Complex` sine from depending on global UI state — which
matters because `Complex` is also used by matrices, lists and stats, none of
which should acquire an angle mode by accident. Scaling the whole argument is
what makes the decisive property hold: **for a real-valued argument the two
evaluators agree in either mode**, which is exactly what "DEG mode does
nothing" was violating.

**Tradeoffs**: `sin(2+3i)` in DEGREE now means `sin((2+3i) x pi/180)` rather
than radians-regardless. That is a genuine semantic choice for an input no
calculator agrees on, taken because it is the one that degrades correctly to
the real case. Nothing else in the codebase consumes complex trig, so the
blast radius is the home screen in non-REAL Number mode.

**Test gap this exposed**: `test_complex`/`test_complex_expr` never varied
angle mode, and `test_exact_trig` varies it but goes through the real
evaluator — so nothing crossed the two. `test_angle_mode()` now asserts both
evaluators agree on a corpus of real-valued trig in both modes
(`test_complex_expr` 75 → 99 checks).

### Second defect, same principle: `c_pow` was not exact for real powers

Found in the same bench pass. `10202^2` and the nesting rung
`((((2+1)^2+1)^2+1)^2+1)^2+1` displayed **white in REAL mode but amber in a+bi
mode**. Cause: `c_pow(base, exp)` was unconditionally `c_exp(c_ln(base) *
exp)`, so a real power came back a hair off the integer. That is enough to
fail `format_number`'s `x == std::floor(x)` test (`format.cpp:128`), dropping
the value out of the `"%.0f"` integer branch into `"%.10g"`, which prints a
fractional digit. The exact-form probe then compared `"104080805.x"` against
its own `"104080805"`, found them different, declared the result
"interesting", and rendered it amber.

Only that one rung showed it, and the reason is worth recording: rungs 1-3
(10, 101, 10202) are too small for the drift to reach ten significant
digits, and rungs 5-6 (>= 1e10) take `format_number`'s scientific branch
where it is invisible. **104080805 is nine digits — the only rung whose tenth
significant digit lands in the fraction.**

Fixed by taking `std::pow` directly when both operands are real and either
the base is positive or the exponent is an integer; genuinely complex powers
still route through `exp(ln)`. The real fast path also handles a negative
base with an integer exponent (`(-2)^3`), which `exp(ln)` cannot do on the
real line at all. This one lives in `complex.cpp` rather than the wrapper
layer, because unlike the angle scaling it is pure math with no dependence on
UI state.

**Note on what was *not* a bug**: the amber itself was the exact-form feature
behaving exactly as designed — the decimal really had drifted, and the exact
value really was the better answer. The defect was upstream, in the
arithmetic.

**Revisit when**: a second consumer of `c_sin`/`c_asin` appears — the scaling
is per-caller by design, so a new caller must decide for itself rather than
inherit it. `mat_expr.cpp:682` calls `c_pow` and now inherits the exact real
path, which is what matrix element arithmetic wants.

---

## D45: Phase 5 Stage 5 hardening — CAS pass scratch moves off the stack into a two-ended pool, plus stated depth caps and a real Risk-2 abort

**Date**: 2026-08-05
**Status**: Accepted (Phase 5 Stage 5, task 4D.22)
**Context**: Stage 5's brief was "stress testing + edge cases". An audit of
`src/math/cas/` against spec §13 first found something worse than a missing
test. `simplify_sum` and `simplify_product` each held four `kMaxOperands = 64`
arrays on the stack — **1,144 B and ~1,140 B frames, measured on the linked
Pico 1 object** — and they nest through `simplify_rec` once per level of
ADD-inside-POW-inside-ADD. Core 0's stack is `__StackBottom 0x20041800` →
`__StackTop 0x20042000`: 2 KB declared, and only 4 KB before `__StackOneTop`
(0x20041000), which is core 1's stack — running the display service on both
boards since D10 leg A. `PICO_USE_STACK_GUARDS` is not defined, so nothing
traps.

The blast radius was wider than CAS calls: `exact_form()` runs `parse_expr` +
two `simplify()` passes on *every* home-screen input whose literals are all
integers, so plain arithmetic reaches it.

**Reproduced on the Pico 2 (fd61849), 2026-08-05.** The ladder
`(2+1)^2+1` → `((2+1)^2+1)^2+1` → ... was typed out to rung 6, which is six
nested levels ≈ 6.9 KB of stack. It **returned the correct answer**
(1.173e32) and serial showed 46 `temp:` and 46 `psram-bulk:` heartbeats with
no gap, no fault, no reboot. The overrun ran past core 1's stack top *and*
past its declared bottom into the unused gap above the heap — core 1's
display loop only occupies the top few hundred bytes of its region, so
nothing live was hit. **The failure mode is therefore silent memory
corruption whose blast radius depends on what core 1 is doing at that
instant, not a deterministic fault** — which is worse to diagnose, and
invisible to the host suite (x86, 8 MB stack).

**Decision**: four changes, all in `src/math/cas/`.

1. **The `ExprPool` arena becomes two-ended.** Nodes bump up from the bottom
   as before; the passes' per-invocation scratch arrays bump *down* from the
   top under LIFO `scratch_mark()`/`scratch_release()` (RAII `ScratchScope`).
   `simplify_product`, `simplify_sum`, `split_term` and `deriv_product` take
   their arrays from there instead of the stack. Scratch cannot share the
   node end: `simplify()` runs its fixed-point loop up to 50 times without
   resetting, so a bump-only scratch would multiply by passes x depth and
   exhaust the arena; LIFO release makes every pass reuse the same space.
2. **Stated depth caps** replace bounds that happened to fall out of input
   length. Parser `kMaxDepth = 12` (parens, function arguments, unary sign
   chains); simplifier `kMaxDepth = 8` on the two n-ary passes only.
3. **Risk 2 is actually implemented.** `ExprPool` gained a sticky
   `overflowed()` flag and `near_capacity()` (the spec's 80%). `evaluate_home`
   now reports `"Too complex"` when the flag is set, and `exact_form` leaves
   the decimal standing — previously `simplify()`'s "last good form" fallback
   returned a tree indistinguishable from a converged one.
4. **`expand()` no longer simplifies twice.** The binomial path returned
   `simplify(sum)` and `expand()` then simplified that again — ~5.4 KB of a
   22.5 KB arena spent re-canonicalising an already-canonical tree.

**Rationale**: the arrays were the whole problem, and the pool is what the
arena exists for. Moving them there fixes the hazard at its source rather
than capping depth until the symptom goes away, keeps `kMaxOperands` at 64 so
no expression that worked before stops working, and converts an
unbounded-and-silent overrun into a bounded failure that returns `nullptr`.
The caps are then sized to the *measured* worst case rather than a round
number: the deepest-recursing CAS frame is now `integrate_rec` at 172 B, so
12 x 172 B + `evaluate_home`'s 580 B base keeps an operation near 2.6 KB,
inside the 4 KB with margin. Rejected: shrinking `kMaxOperands` to 24 (silently
degrades 25-64-term expressions and still leaves 2 KB at four levels), and a
depth cap alone (rejects working input, leaves the frames as a trap for the
next caller).

**Tradeoffs**: scratch costs ~1 KB of the 22.5 KB arena per nesting level,
bounded by the depth cap. Expressions nested past 12 levels are now refused
rather than attempted — no hand-entered expression comes close, and
`kMaxInputLen` (48) admits at most 7 of the worst-case shape anyway. The
`near_capacity()` abort is deliberately **not** applied to expand's binomial
path: that path is linear in n (capped at 20) with a known cost, and
`(x+1)^10` legitimately reaches ~76% of the arena, so an 80% abort there
would reject working input.

**Results**: `test_cas` 272 → **368 checks**, 0 failures; full host suite
green. Pico 1 bss 201,096 → **198,836** (−2,260). Largest recursive CAS frame
1,144 B → **172 B**. lint/format clean. The new `test_stress_edge_cases()`
immediately earned itself by catching a defect in the first cut of this very
change: `alloc_raw` bounded the node end against the arena end rather than
the scratch end, so nodes bumped through live scratch arrays and the pass
read back overwritten `Expr` pointers.

**Revisit when**: (a) a CAS pass is added that recurses deeper than
`integrate_rec` or carries a bigger frame — re-measure and re-derive the
parser cap, the arithmetic is in `parser.cpp`'s comment; (b) the arena moves
off `scratch::kCompute` or changes size, which shifts both the scratch budget
and the ~76% that `(x+1)^10` occupies; (c) **`PICO_USE_STACK_GUARDS` is
considered** — see the follow-up note below, deliberately left out of this
change.

**Follow-up left open (not done here)**: enabling `PICO_USE_STACK_GUARDS=1`
with `PICO_STACK_SIZE=4096` would put a trap exactly where core 0 starts
eating core 1's stack, turning this whole class of bug from silent corruption
into a hard fault. It is not bundled into Stage 5 because it is a
whole-firmware change, not a CAS one: this session proved at least one path
overran silently, and there may be others (graph rendering, matrix ops, and
Phase 6's MicroPython heap) that currently work *because* nothing traps.
It deserves its own soak on both boards rather than riding along with a
phase close.

---

## D44: Exact-form follow-ups — Alt+Enter decimal escape, exact trig at special angles, and non-REAL number modes

**Date**: 2026-08-03
**Status**: Accepted (Phase 5 Stage 4 follow-up, same day as D43)
**Context**: The first Pico 2 flash of D43's exact-form display raised three gaps
in immediate use. (1) `>dec` was the only way to see a decimal, which means
retyping the whole expression with a suffix — clumsy for the common "yes I know
it's $\sqrt{2}$, what's the number" case. (2) `sin(pi/3)` showed `0.8660254038`, not
`√3/2` — exact trig at special angles is the single most expected case of this
feature and it was missing. (3) Exact forms were REAL-mode only, a scoping
decision D43 recorded as a v1 limitation with no technical reason behind it.
**Decision**:

1. **Alt+Enter is the decimal escape.** With an expression entered it
   evaluates with the exact-form probe suppressed, identically to a trailing
   `>dec`. With the input line empty and the newest history result being an
   exact form, it re-runs *that* expression as a decimal, so an amber
   $\sqrt{2}$ can be turned into `1.414213562` without retyping. Commands
   (`cls`, `help`, ...) are unaffected. The `#HOME` and a new `#EXACT FORMS`
   block in the on-device HELP KEYS/SYNTAX tabs document it.
2. **Exact trig at special angles.** `sin`/`cos`/`tan` of a rational multiple of
   $\pi$ with denominator in $\{1,2,3,4,6\}$ fold to their exact values through a
   24-entry table indexed in twelfths of $\pi$ (one table covers both the $\pi/6$ and
   $\pi/4$ families; `cos(x) = sin(x + pi/2)` is an index shift, so only sine and
   tangent tables are stored). **Angle-mode aware**: in DEGREE mode the argument
   is read as degrees, so `sin(60)` folds exactly as `sin(pi/3)` does in RADIAN.
   `math::frac::pi_multiple` does the recognition.
3. **Non-REAL number modes get exact forms** for real-valued results. The probe
   moved into a shared `apply_exact_form` helper called from both the REAL and
   the complex dispatch branches. Genuinely complex values stay decimal.
4. **"Interesting" now compares formatted strings, not doubles.** A bare integer
   is still normally not upgraded, but it *is* when the numeric path would
   display something else.
**Rationale**: The decimal escape was first bound to Shift+Enter, on the
reasoning that `shift_held` rides on every `kEnter` event. **On hardware it does
not fire**: the diag screen showed the chord arriving as key code 59 (`kInsert`)
rather than 52 (`kEnter`). The STM32 *translates* Shift chords into their own
scan codes — Shift+Enter to 0xD1, the same family as Shift+F1..F4 to F6..F9 —
instead of reporting base-key plus modifier, which is the same underlying
behavior as the D12 arrow-swallowing seen from a different angle. Binding
`kInsert` would have worked but squats on a real key with its own meaning, so
the shortcut moved to **Alt+Enter**: Alt passes its flag through intact, and
Alt+UP/DOWN already scrolls the history view, so the modifier is established on
this screen. Reusing the existing `to_dec` plumbing made it a one-parameter
change rather than a new display path. For trig, the twelfths
indexing is what keeps this a table lookup instead of a case analysis: every
exact angle in both families is an integer number of $\pi/12$, so reduction is one
modulo. Handling DEGREE mode by reinterpreting the argument (rather than
declining to fold) matters because a calculator in DEGREE mode is where a user
most naturally types `sin(60)`. Point 4 is what lets `sin(pi)` show `0` instead
of `1.224646799e-16` and `cos(pi/2)` show `0` instead of `6.123233996e-17` —
float noise the numeric path cannot avoid and the exact path knows the answer to
— while keeping `tan(pi/4)` out of the amber path, since its
`0.9999999999999999` already formats as `1`. Comparing doubles would have caught
`tan(pi/4)` too; comparing `format_number` output is the precise test because
the display is what the gate is actually about.
**Tradeoffs**: The trig table is +5.3 KB flash (bss unchanged at 201,096). Only
sin/cos/tan — the inverse functions (`asin(1/2)` → `π/6`) are not folded, and
neither are hyperbolic or `atan2`. In DEGREE mode the argument must be an
integer number of degrees, so `sin(22.5)` does not fold even though `sin(pi/8)`
is expressible. Alt+Enter on an empty line only acts when the newest result is
an exact form; on a plain decimal result it does nothing (deliberate — silently
re-evaluating an arbitrary past entry would be surprising). The complex-mode
extension means RECT/POLAR now shows amber results where it showed decimals,
a visible change in those modes.
**Revisit when**: Inverse-trig exact values are wanted (the table is already the
right shape to invert); or DEGREE-mode half-degree angles turn up in real use;
or Insert acquires a real binding and the two want reconciling. The
Shift-chord translation behavior is now recorded in `platform/keyboard.hpp`
next to the D12 arrow note, so the next binding does not repeat this.

## D43: Phase 5 Stage 4 exact-form display — always-on, gated by an integers-only input rule plus a numeric-agreement check

**Date**: 2026-08-03
**Status**: Accepted (Phase 5 Stage 4, tasks 4D.23 / 4D.24). Resolves open questions P5-5 and P5-6.
**Context**: `phase5-spec.md` §10.1 wanted home-screen results with a clean closed
form to display that form instead of a truncated decimal (`sqrt(2)` → `√2`,
`sqrt(8)` → `2√2`, `pi*2` → `2π`, `1/3` → `1/3`). Two questions were left open:
whether recognition is always-on or a MODE toggle (P5-5), and whether `pi` gets
the treatment alongside `sqrt` (P5-6). Both were answered "always-on, yes to pi",
which makes the feature touch *every* plain scalar result on the home screen —
so the real design work is bounding what it may change.
**Decision**: Always-on, no MODE row entry; `pi` included. `>dec` is the per-result
opt-out (it already existed and previously did nothing beyond stripping itself).
Recognition lives in a new `src/math/cas/exact.cpp`, **not** in `simplify()`, and
runs as a side-effect-free probe after the numeric result is committed, mirroring
the D30 `complexexpr` pattern. Five gates decide whether the decimal is replaced:

1. The numeric result must exist, be finite, and not be a store (`5->a`).
2. Every numeric literal in the *parsed* input must be an integer.
3. No variables may appear anywhere in the parsed tree.
4. The simplified tree must match a whitelist grammar — rational coefficients,
   `sqrt` of a square-free integer, `pi` — and be "interesting" (a bare integer
   is not upgraded).
5. The recognized form must agree with the numeric result to $10^{-9}$ relative.

Display goes through the existing D42 path (serialize → `render::build_layout`,
amber `kSymbolic`). Two layout-builder changes give the handwritten look the
spec's acceptance text asks for: a single-atom radicand drops its parens (`√2`,
not `√(2)`), and a coefficient before a radical or a symbol glyph multiplies
implicitly (`2√2`, `2π`, not `2*√(2)` / `2*π`).
**Rationale**: Gate 2 is the one that makes "always-on" safe. Without it, `2.5`
would display as `5/2` and `0.1+0.2` as `3/10` — technically the nearest small
rational, but a change to results the user never asked to see reinterpreted.
"The user typed only integers" is a clean, explainable proxy for "an exact answer
was meant", and it also rejects the decimals that `convert()`/`solve()`
substitution splices into the expression buffer. Gate 3 is not optional: the CAS
parser has no `ans` or `e`, so `ans` parses as `a*n*s` and `e` as a variable while
the numeric engine gives both real values — a silent-wrong-answer generator, not
just a missed opportunity. Gate 5 makes the whole class of CAS-vs-`tinyexpr`
parser divergences (implicit multiplication, unary-minus binding, DEG-mode trig)
unable to alter a displayed answer, by construction. Keeping surd extraction out
of `simplify()` matters because `simplify()` runs inside `integrate()`, `solve()`,
`factor()` and the derivative fixed-point loops; rewriting `POW(NUM, 1/2)` there
would change node shape mid-loop for passes that pattern-match on `POW` (§13
Risk 1) for zero Stage-4 benefit. The implementation works in `POW(u, 1/2)` space
rather than `FUNC sqrt(u)` space so the existing simplifier does the factor
collection: `sqrt(2)*sqrt(2)` collapses to `2` and `1/sqrt(2)` becomes
`POW(2,-1/2)`, making denominator- and radicand-rationalization one code path.
**Tradeoffs**: `1/3` now displays as an amber stacked fraction where it used to
show `0.3333333333`, and `pi` displays as `π` — a visible change to
long-standing behavior, deliberate but worth watching on device. `>frac` results
stay white flat text, so `1/3` and `1/3>frac` look different despite meaning the
same thing. Expressions naming a variable or `Ans` never get an exact form
(gate 3), so `5->a` then `a/3` stays decimal. Non-REAL number modes get no exact
forms at all in v1 — the `force_complex` branch is not wired, a ~6-line
follow-up. The grammar is a whitelist, so `pi^2`, `sqrt(pi)`, `2^(1/3)` and
`sqrt(sqrt(2))` all fall back to decimal; each extension is a separate decision.
Radicands are capped at $10^6$ (the trial-division factorizer runs to $d = 1000$,
~0.5 ms worst case on the M0+) and denominators at 10000, matching
`serialize.cpp`. The layout change also alters how a *typed* `2*pi` renders on
the input line, which required updating an existing `test_layout` assertion.
**Revisit when**: The on-device pass judges the `1/3`-as-fraction and `pi`-as-`π`
changes too intrusive (the escape hatch is to require a `sqrt`/`pi` flag rather
than any flag, dropping bare rationals back to decimal); or a MODE toggle is
wanted after all; or non-REAL modes need exact forms; or the whitelist grammar
starts rejecting forms users actually hit.

## D42: Phase 5 CAS result rendering — reuse `render::build_layout` on the serialized string instead of a dedicated `expr_to_layout` tree-walker

**Date**: 2026-08-02
**Status**: Accepted (Phase 5 Stage 3, tasks 4D.4 / 4D.21)
**Context**: Spec task 4D.4 called for `expr_to_layout` — a function that walks a
CAS `Expr` tree directly into `render::LayoutNode` for 2D typeset display of
symbolic results. The plan already hedged this ("or fold `expr_to_layout` into
serialize"). The existing numeric layout builder, `render::build_layout`, is a
recursive-descent typesetter over calculator-expression *strings* that already
produces `kFraction`/`kSuperscript`/`kParen`/`kText` nodes with `√`/`π` glyph
substitution and function-call shaping — everything a CAS-result renderer needs.
**Decision**: CAS results are serialized to an infix string with
`math::cas::expr_to_string` and rendered through `render::build_layout`; no
separate `Expr`-tree layout walker is written. The home screen stores the
serialized result in the history `Entry` (kind `kSymbolic`) and typesets it via
`build_layout` in the accent color, reusing the same path already used for the
input-expression line.
**Rationale**: A tree-walker would duplicate all of build_layout's fraction/
superscript/paren/precedence logic for *identical* visual output — the one case
where a tree-walker could win (a big radical spanning its argument) is
explicitly KIV (`√` stays the inline `kSqrtGlyph`). Serialize→build_layout reuses
a tested, in-firmware path, adds no duplicate layout code or test surface, and
sidesteps the render pool's single-tree-at-a-time constraint (results are held
as strings, not live trees, so they survive `pool_reset` between redraws). The
Expr→string→Node round-trip cost is trivial at Enter-rate.
**Tradeoffs**: Reloaded history results (from `history.txt`) come back as plain
text (kind `kPlain`), not re-typeset in the accent color — acceptable, and it
mirrors how complex/MatAns results already reload as plain strings. Results
longer than the 48-char `Entry.result` truncate on the scrollback (the newest
result keeps its full 128-char form for typesetting).
**Revisit when**: A big-radical (`√` spanning the radicand) or subscript node type
is added to the layout back end, or expression-valued variables (idea F/H) need
a live-tree renderer.

## D41: Phase 5 CAS ExprPool placement — SRAM raw-pointer arena over the shared scratch kCompute region (rejects the spec's PSRAM plan)

**Date**: 2026-08-02
**Status**: Accepted (Phase 5 Stage 0, tasks 4D.1–4D.3)
**Context**: `phase5-spec.md` §3 sketched the CAS node pool ("ExprPool") living in
PSRAM on Pico 1 (64 KB) / SRAM on Pico 2 (128 KB), via `config::kCasPoolSize`,
but §3 itself flagged "re-verify this pool's SRAM-vs-PSRAM placement against
actual headroom before implementation starts." Stage 0 needed the concrete
representation decided before building the tree/parser on top of it.
**Decision**:
1. **The pool is SRAM with raw `Expr*` child/next pointers** (`src/math/cas/expr.{hpp,cpp}`),
   a bump allocator modeled on `render::pool.hpp` (`alloc()`/`reset()`, no
   per-node free, bulk-reclaim per top-level op). `alloc()` uses `std::align`
   over the region and returns `nullptr` when full (the spec §13 Risk-2
   abort-above-capacity path).
2. **PSRAM-backing is rejected.** `platform::Psram` is offset-addressed, not
   memory-mapped (`src/platform/psram.hpp`) — access is `read()/write()` by
   offset. A CAS engine traverses and rewrites nodes constantly; raw-pointer
   trees can't live in PSRAM, and a read/write per node access would be
   pathologically slow. The spec's PSRAM idea does not survive that constraint.
3. **The pool overlays the shared math scratch kCompute region**
   (`math::scratch::compute_region()`, `kComputeBytes = 22528`) rather than
   allocating a dedicated buffer — **zero new bss**. kCompute's existing owners
   (list_expr | stats | infer | matops) are never active during a home-screen
   CAS op, so CAS borrows the whole region; `cas` is added to the owner
   contract in `scratch.hpp`. A CAS op must not re-enter those owners while it
   holds the arena (it doesn't in v1).
4. **`config::kCasPoolSize` is retired** (was unused; superseded by the
   kComputeBytes bound). ~22.5 KB / ~32 B per node ≈ ~700 nodes on device —
   ample for the spec's acceptance sizes; pathological inputs hit the Risk-2
   abort.
**Rationale**: keeps Pico 1's ~68 KB SRAM headroom fully intact (the standing
watch item) while giving the CAS a fast, pointer-native working set; reuses an
already-proven arena rather than inventing a handle/index indirection just to
reach PSRAM that would be far slower and more complex.
**Tradeoffs**: the CAS working set is capped at ~22.5 KB (no PSRAM spill) — a
very large symbolic expansion aborts rather than growing; the kCompute
owner-guard remains convention-only (a debug guard is still the recommended
follow-up from the pre-Phase-5 review). Deviates from the letter of
phase5-spec §3, which is left as-is (per AGENTS.md: specs aren't edited casually;
the as-built reconciliation lives here).
**Revisit when**: a real CAS input overruns ~700 nodes on device (add PSRAM
spill via a handle-based overflow tier, or grow the region); or the shared-arena
coupling causes a re-entrancy bug (move CAS to its own region / add the debug
owner-guard).

## D40: Phase 4D close — F sequenced after Phase 5 CAS, idea H deferred again

**Date**: 2026-08-02
**Status**: Accepted
**Context**: Phase 4D's three-item close checklist (`next-session.md`) needed
resolving: the F-evaluator follow-on check from D37, a revisit of idea H
(polymorphic variables), and the ti-parity.md/README status flip. D37's F
trigger ("2+ of B-E ship and duplication becomes visible") has now fired —
idea B (complex variables/Ans, 4D.15), C (complex lists, 4D.24), D (complex
matrices, 4D.25), E (vector ops, 4D.22 plus the list↔matrix bridge half
4D.12), and G (eigenvectors, 4D.23) have all shipped and are HW-verified
within Phase 4D (worklog's 2026-07-26/27 batch table).
**Decision**:
1. **F (unified evaluator): still committed, now explicitly sequenced after
   Phase 5 (CAS).** The forward order is: pre-Phase-5 code-review + size-
   optimization pass → Phase 5 (CAS, D32/D33) → F.
2. **Idea H (polymorphic variables): deferred again, stays unscheduled.**
   TI's three separate namespaces (`A`-`Z` scalars, `[A]`-`[J]` matrices,
   `l1`-`l6`/named lists) remain as-is. H is revisited only if real usage
   demands it, re-checkpointing after F rather than on any fixed date.
**Rationale**: Rewriting the evaluator (F) immediately before CAS risks
churning code that Phase 5 is about to build on top of; CAS may itself add
a fourth evaluator (symbolic expressions) that F would then need to unify
alongside the existing three, so doing F once CAS's shape is known avoids a
second unification pass later. H is a ~100+ hr effort needing its own design
pass (full scope: `design-departures-matrix-complex.md` §H) and Phase 5 CAS
is the actual planned next milestone — better to spend the design-thinking
budget there first.
**Tradeoffs**: The evaluator duplication F would resolve (engine/
complexexpr/matexpr/listexpr near-parallel logic) persists through all of
Phase 5. Idea H's convenience (one polymorphic variable slot instead of
three namespaces) stays unavailable; users must keep track of which
namespace a value lives in.
**Revisit when**: Phase 5 (CAS) ships — that's F's actual scheduling
trigger now, and the point to check whether CAS added a fourth evaluator
worth folding into the same F pass; H's own revisit trigger is unchanged
from its original framing — real usage demand, checked again after F lands.
**This closes Phase 4D's three-item checklist. Phase 4D is now declared
CLOSED** — see worklog's 2026-08-02 "Phase 4D CLOSED" entry and the same-day
`ti-parity.md`/README updates. Cross-references: D37 (F trigger + scoping),
`design-departures-matrix-complex.md` §H (idea H's full scope).

## D39: MatAns persists across a power cycle — reverses the D38/"by-design" stance

**Date**: 2026-08-02
**Status**: Accepted; implemented and HW-verified same day (Pico 2, `e5f2a10-dev`)
**Context**: Earlier the same day (`c158139`), MatAns not surviving a power
cycle was reclassified from "bug" to "by-design" — `mat_ans()` was a
transient global (`g_mresult`, `mat_expr.cpp:27`) never written to SD, only
the named `[A]..[J]` matrices persisted via `matrices_persist.cpp`. The
developer then decided that stance was wrong: MatAns should persist like the
named matrices do, for the same reason a scalar `ans` and the named
matrices already do — the surprise is in it *not* surviving, not in it
surviving.
**Decision**: MatAns now persists to its own file, `/picocalc/matans.dat`,
using the same PCM2 header/element format as `[A]..[J]`. The single-matrix
save/load logic in `matrices_persist.cpp` was refactored from
index-specific `save_matrix`/`load_matrix` into path-based
`save_matrix_file`/`load_matrix_file` (declared in `matrix.hpp`), so
`MatrixStore` and MatAns share one implementation instead of duplicating
the file format. `math::matexpr::save_ans`/`load_ans` (declared in
`mat_expr.hpp`, defined in the firmware-only `matrices_persist.cpp` so the
host build stays storage-free) wrap that shared code: `save_ans` is called
from `HomeScreen::evaluate_input` after any matrix-result commit;
`load_ans` runs at boot in `main.cpp` alongside the named-matrix load, with
the same late-init retry for the D14 cold-boot PSRAM/SD rail-settle window.
A `g_ans_loaded` latch (mirroring `MatrixStore::loaded_`) stops a delayed
cold-boot retry from clobbering an in-session result.
**Rationale**: Parity with `[A]..[J]` and scalar `ans` — a calculator user
has no reason to expect the *last* matrix result to behave differently from
every other stored value across a reboot. Reusing the existing single-file
PCM2 save/load code (rather than writing a bespoke MatAns format) keeps the
persistence surface uniform and the diff small.
**Tradeoffs**: One more file on the SD card
(`/picocalc/matans.dat`); negligible SD/PSRAM cost, same shape as the
existing `matrixN.dat` files. No format/magic bump — old cards simply have
no `matans.dat` until the first matrix-result commit under this firmware,
so there's no reset transition to call out.
**Revisit when**: never expected — this closes the question raised by the
2026-07-27/2026-08-02 MatAns observations for good, absent a future
persistence-format redesign.

## D38: 4D implementation decisions — open questions resolved, batching fixed, two tasks closed as already-shipped

**Date**: 2026-07-26
**Status**: Accepted (planning decision; implementation starting)
**Context**: Session start for Phase 4D implementation. A planning pass over
`phase4-spec.md` §7/§8 plus a code exploration sweep (engine/storage, graph
subsystem, render/platform HAL) surfaced that two task rows were already
shipped, and the remaining open questions (P4-10, P4-12, P4-13, units UX,
APD depth, two scope adds from the 2026-07-22 device pass) needed answers
before work began. All were put to the developer with implications;
decisions below.
**Decision**:
1. **4D.16 and 4D.21 closed with zero work.** 4D.16 (xyLine + normal
   probability plots) already shipped in Phase 3D (Session 15, D27) — all
   five `StatPlotType`s exist with renderers (`graph/graph_state.hpp`,
   `graph/stat_plot.cpp`); the spec row was written against an outdated
   assumption. 4D.21 (build-id diag label) shipped 2026-07-25 (`f444db9`).
2. **P4-12 resolved: full u/v/w + cross-reference in v1.** The seq
   evaluator iterates forward from `nMin` with memoization (recursion
   can't ride tinyexpr anyway), so all three sequences advance in lockstep
   and `v(n-1)`/`u(n-2)` cross-references are nearly free.
3. **P4-10 resolved: named user lists, full integration** (the bigger
   option). l1–l6 stay as fixed slots; named lists (letter-first, `≤5`
   chars, cap ~20) work everywhere a list token works — listexpr, stats/
   regressions, stat-plot configs, list editor — persisted one file per
   list plus a name directory. 4D.13 re-estimated 4 → ~15 hrs.
4. **P4-13 resolved: rref-nullspace of `(A−λI)`** per real eigenvalue,
   reusing shipped `matops::rref`; repeated/defective eigenvalues return
   an explicit "no unique eigenvector" error rather than guessing a basis.
5. **Units UX: typed `convert(value,"from","to")` only** — a pre-engine
   interceptor (tinyexpr has no string args), matching the project's
   typed-command precedent. A picker screen stays deferred.
6. **APD (4D.19) = soft-sleep v1**: inactivity timer in the main loop →
   backlight 0 (`±` DISPOFF), keep polling the STM32 for wake on any key. No
   deep sleep (core-1 display service + tinyusb + XIP-residency risk, per
   D10). Brightness/kbd-backlight + APD timeout persist in a new
   `/picocalc/settings.dat` with its own magic (`PCS1`) — device settings
   deliberately decoupled from GraphState format bumps.
7. **Two scope adds accepted** (both nest inside existing 4D tasks, so
   Risk 8's scope line holds): home-screen `MatAns` token (into 4D.14) and
   fnInt shading in the curve's palette color, darkened or hatched — no
   true alpha on RGB565 (into 4D.11).
8. **Sequencing: risk-first batches**, one per session: complex
   Variables/Ans + complex lists (4D.15+24) → complex matrices (4D.25) →
   sequence graphing (4D.6-8, carries the single PCG5→PCG6 bump including
   Batch-4's shade-config fields) → zoom/shading (4D.9-11) → data/catalog
   glue (4D.12/14/17/18/22) → named lists (4D.13) → display/formatting
   (4D.1-5) → eigenvectors (4D.23) → device polish (4D.19-20, needs board).
**Rationale**: Risk-first puts the cross-cutting storage work (complex
variables/lists/matrices, ~46 hrs) at the start of the phase where
re-planning room exists; small contained items (ENG, ▶Frac, glyphs) make
good session-filler later. Full u/v/w and full named-list integration were
chosen over thinner v1s because both thin variants ship visibly
half-integrated features (coupled sequences are the textbook use;
compute-but-not-edit named lists feels broken on device).
**Tradeoffs**: 4D subtotal ≈ 165 hrs (4D.13 grew ~11 hrs; 4D.16/21 removed
~7; two scope adds ~3). Complex lists/matrices still always pay the PSRAM
tier (D37, unchanged). Deep sleep and a units picker screen stay unshipped.
**Revisit when**: any batch's implementation contradicts a resolution
(e.g. rref tolerance proves inadequate for clustered eigenvalues —
fall back to inverse iteration per P4-13's alternative); APD soft-sleep
power draw disappoints and deep sleep gets its own scoping.

## D37: Close out the matrix/complex design departures (C, D, F) — all folded into 4D

**Date**: 2026-07-24
**Status**: Accepted (scoping decision; not yet implemented)
**Context**: Immediately after D36 pulled E (vector ops) and G (eigenvectors)
forward into 4D, asked to work through the rest of
`design-departures-matrix-complex.md` (C: complex lists, D: complex
matrices, F: unified evaluator) and close those decisions entirely rather
than leave them gated on a post-4D scoping pass. Concrete numbers were
pulled from `src/math/array.hpp` first: `ArrayStore`'s SRAM slab pool is a
fixed `kSlabCount(28) * kSlabBytes(2048)` = 56 KB of bss already committed
to the real-only small-array tier; Pico 1 bss is ~197 KB of 264 KB as of
D35 (~67 KB headroom, shrinking as 4D lands). PSRAM regions, by contrast,
are bump-allocated on first use — zero bss cost.
**Decision**:
1. **P4-11 resolved: error, not silent truncation.** Any real-only
   consumer of a complex value — matexpr scalar subterms, listexpr
   reductions, the graphing/table hot path — errors rather than silently
   drops the imaginary part. Generalizes the rule from "complex
   variables" (the original P4-11 scope) to complex list/matrix elements
   too, now that C/D are in scope.
2. **C (complex lists): go**, as **4D.24**. Complex-valued lists route
   **exclusively through the PSRAM region tier**, never the 28-slab SRAM
   pool — zero bss growth, even for a 3-element complex list, at the
   cost of losing the SRAM fast path small real lists get. **v1 scope**:
   storage, display, elementwise ops (add/sub/scalar-mul), `sum`/`mean`
   (well-defined componentwise). `stdev`, regression, `sort` — anything
   depending on an ordering or a variance definition — error on complex
   input in v1, not silently promoted.
3. **D (complex matrices): go**, as **4D.25**, reusing 4D.24's
   PSRAM-only storage answer (C and D share a storage model, as the
   design-departures doc anticipated). **Full complex linear algebra in
   v1** (the more ambitious of two scoping options offered — chosen over
   "storage + elementwise only"): det (complex LU), inverse (complex
   Gauss-Jordan, magnitude-based pivoting), rref/ref/rank,
   augment/reshape/identity/power/transpose, and the solver's
   `solve_linear` path all generalize to `Complex`. **Explicitly out of
   scope**: eigenvalues/eigenvectors *of* a complex-valued matrix — a
   complex Hessenberg+QR core is a materially bigger algorithmic lift
   than generalizing arithmetic to `Complex`, and distinct from 4C's
   existing feature (complex eigenvalues *from a real* matrix, D30,
   untouched here). `eigen_core` keeps its real-input assumption;
   4D.23's eigenvectors (idea G) stay real-input-only for the same
   reason.
4. **F (unified evaluator): committed as a real follow-on after 4D
   ships**, not indefinite parking. With B/C/D/E/G all landing in 4D,
   F's own trigger ("2+ of B-E ship and duplication becomes visible")
   is expected to fire once 4D closes — treated as the de facto next
   architecture pass, though not yet given its own phase/week slot.
**Rationale**: PSRAM-only routing sidesteps the SRAM feasibility risk the
design-departures doc flagged entirely, rather than requiring a
speculative bss study before committing — the numbers make the tradeoff
(always pay PSRAM access for complex arrays) knowable up front. Splitting
C/D's v1 scope down to "well-defined operations only" (elementwise,
sum/mean; full linear algebra for matrices but not complex-matrix
eigendecomposition) keeps each task honestly estimated instead of
open-ended. F stays sequenced after 4D because unifying three evaluators
needs real shipped code with real duplication to refactor against, not a
parallel speculative rewrite.
**Tradeoffs**: 4D's subtotal grows ~124 → ~160 hrs (4D.24 ~14 hrs, 4D.25
~22 hrs); Phase 4 total ~259 → ~295 hrs. Complex lists/matrices always
pay the PSRAM tier cost, never the SRAM fast path. Complex-matrix
eigendecomposition remains an open gap after 4D ships — a future item if
ever wanted, not covered by 4D.23 (real-only) or 4D.25 (explicitly
excludes it).
**Revisit when**: 4D.24/4D.25 are implemented and the actual PSRAM-tier
access cost is measured on hardware (informs whether "always PSRAM, even
for 3 elements" needs a small-size exception later); complex-matrix
eigendecomposition gets requested (separate scoping, bigger than 4D.25);
F's trigger condition is checked once 4D actually closes.

## D36: Pull vector ops + eigenvectors into 4D from soak-feedback session

**Date**: 2026-07-24
**Status**: Accepted (scoping decision; not yet implemented)
**Context**: A soak/discussion session after hands-on use of the current
build (both boards) surfaced feedback that lists and matrices feel "walled
off" from each other, wanting vector ops (`dot`/`cross`/norms) usable on
lists/narrow matrices, and asked whether an eigenvector function exists (it
doesn't — 4A/4C only ever shipped eigen*values*). This overlaps directly
with `design-departures-matrix-complex.md` idea E (vector ops — the
list↔matrix bridge half of E already shipped as 4D.12; the vector-ops half
never made it into 4D's task list) and raises a new idea, G (eigenvectors),
not in the original A-F list at all. `next-session.md`'s existing plan was
to scope E's leftover half (and C/D) in a dedicated pass *after* 4D ships.
**Decision**: Pull E's vector-ops half and the new eigenvectors idea (G)
forward into 4D now, rather than waiting for the post-4D scoping pass — as
**4D.22** (`dot`/`cross`/`norm` on vectors, plus matrix Frobenius `norm`)
and **4D.23** (matrix eigenvectors, real-only for v1, mirroring D28's
real-only eigenvalues precedent before D30 added the complex spectrum).
Added open question **P4-13** (`phase4-spec.md` §11) for 4D.23's algorithm
choice (nullspace of `A - λI` via existing `rref` vs. inverse iteration)
and repeated/defective-eigenvalue handling. C/D (complex lists/matrices)
and F (unified evaluator) are unaffected — still deferred to the post-4D
pass per their own gating reasons (Pico 1 memory feasibility; "wait for
duplication pain").
**Rationale**: E was already assessed cheap and storage-model-free in the
design-departures doc ("could land whenever, including opportunistically
inside another matrix-touching session") — no reason to hold it for a
separate pass. G is bigger (a real numeric-methods addition, not a thin
wrapper) but is scoped now, with its algorithmic uncertainty captured as
P4-13 rather than guessed at, so 4D's estimate stays honest.
**Tradeoffs**: 4D's subtotal grows ~109 → ~124 hrs (Phase 4 total ~244 →
~259 hrs). 4D.23 is real-only in v1 — complex eigenvectors (matching
`eigenvalues_complex()`'s existing complex spectrum) are not covered and
would need their own follow-up.
**Revisit when**: 4D.23 is implemented and P4-13 gets resolved against
real code, not guessed; or if complex eigenvectors get requested (separate
scoping, likely bigger than 4D.23 itself).

## D35: Pico 1 perf fixes — bucketed stat-plot point cache, list-editor dirty-band narrowing, one-file-per-list/matrix persistence

**Date**: 2026-07-22
**Status**: Accepted
**Context**: 3D.14 and the Phase 4A-4C Pico 1 pass (same day) found two non-blocking perf findings: a 5000-point scatter plot and the list editor both felt sluggish on the Pico 1. Root-cause investigation (no hardware needed, code-read only) found three distinct causes, all stemming from code that was correct on the Pico 2 full-framebuffer path but paid a hidden multiplier on Pico 1's strip renderer or on every single-value commit: (1) `draw_stat_plots()` streamed a plot's *entire* list from its Array (PSRAM-tier above ~256 elements, D21) on every `render()` call, and `render()` runs once per `config::kStripHeight`-line strip (~20x/frame) on Pico 1 vs. once for the whole screen on Pico 2 (§8); (2) the list editor's `invalidate_grid()` marked all 13 visible rows dirty on every cursor move or single-cell commit, even though only 1-2 rows' highlight actually changed; (3) `ListStore::save()`/`MatrixStore::save()` re-serialized *all six lists* / *all ten matrices'* full contents to SD on every single commit, regardless of which one was actually edited — an I/O cost proportional to total stored data, not the edit size, which turned out to be the dominant cost behind "large lists feel sluggish to enter values into" (the user's own diagnosis, confirmed by code read).
**Decision**:
1. **Bucketed pixel-space point cache for stat plots** (`src/graph/stat_plot.cpp`): `recompute_stat_plots(vp)` now takes the viewport (safe — `GraphScreen::recompute()` already holds one, and `dirty_` guarantees a single recompute per actual data/window change even though `render()`/strip calls happen many times after it) and streams each scatter/xy-line/normprob list *once* into a capped, decimated `PointCache` (`kMaxCachedPoints = 800` int16 px/py pairs — the screen is only `platform::kScreenW` px wide, so a few hundred points already exceeds what's visually distinguishable; decimation takes every stride-th point, preserving order). Scatter and normprob (dots only, order-independent) additionally get counting-sorted into `kStripBuckets` (`= screen_height / config::kStripHeight`, 20) bands, so `render()` only ever visits the bucket(s) overlapping `fb.clip_y0()/clip_y1()` — on Pico 2 that's one range spanning the whole screen (trivially all buckets, same total cost as before); on Pico 1 it's the 1-2 buckets under the current strip. xy-line keeps insertion order (segments depend on it) and just iterates the small capped cache every strip — cheaper than the old PSRAM re-stream even without bucketing. Box-plot outliers got a simpler version of the same idea: since all of a slot's outlier marks share one fixed `cy` row, the streamed fence-scan is skipped outright unless `cy` falls in the current strip's clip range (~19/20 strips skip it entirely now). Pico 1 bss cost: ~8.4 KB (800 x 2 x 2 bytes x 3 slots + shared sort scratch), comfortably inside the ~76 KB headroom watched since D28.
2. **List-editor dirty-band narrowing** (`src/apps/list_editor.cpp`): the screen already used the D13 dirty-band mechanism (`track_dirty()`/`invalidate()`) but only ever invalidated the *whole* 13-row grid. Added `invalidate_row(int)` (one row's band) and `invalidate_header()` (the "l1:N" count band); `kUp`/`kDown` and `commit_edit()` now invalidate just the old+new selected row (plus the header band on a commit, since appending changes the count) when the visible window didn't scroll, falling back to the full `invalidate_grid()` when it did (or for delete/sort/clear, which genuinely touch every row). No render() logic changed — the framebuffer's existing clip machinery already only touches pixels inside whatever band gets invalidated.
3. **One file per list / matrix** (`src/math/lists_persist.cpp`, `src/math/matrices_persist.cpp`): replaced the single concatenated `lists.dat`/`matrices.dat` (header + all six/ten bodies back to back) with `/picocalc/list1.dat`..`list6.dat` and `/picocalc/matrix1.dat`..`matrix10.dat`, magic bumped (PCL1→PCL2, PCM1→PCM2 — old images simply aren't read under the new paths, same "old files ignored, not deleted" precedent as prior format bumps). `ListStore::save`/`MatrixStore::save` now take the index that changed; every call site (`list_editor.cpp`, `matrix_editor.cpp`, `home_screen.cpp`'s matexpr/listexpr store paths) already only ever mutates one list or matrix per operation (a single `->lk`/`->[X]` store target, or one editor slot — verified by reading every call site, not assumed), so this is always the right granularity — there is no "save everything" entry point because nothing needs one. `load()` still reads all six/ten and keeps the old all-or-nothing-per-item contract (false while any item still needs PSRAM, D14), but now tracks a per-item `loaded_[]` latch so a pending retry never re-reads (and clobbers an in-session edit to) an item that already succeeded — a real behavior improvement the old single-file design couldn't express, not just a side effect.
**Rationale**: all three fixes target "the same code path pays a hidden multiplier under strip rendering or full-store persistence" rather than reducing algorithmic work in the abstract; each was verified against the actual code (not guessed) before being written, and each was flashed to the Pico 1 and confirmed by the developer before moving to the next. The persistence fix in particular was *found*, not assigned — the original 3D.14 report was rendering-shaped ("scatter plot slow"), but the developer's own retest of "list editor feels slow" during the dirty-band fix's interview pointed at data entry specifically, which led to reading `ListStore::save()` and finding the real bottleneck was I/O, not rendering, and by a wide margin.
**Tradeoffs**: `kMaxCachedPoints = 800` means very dense scatters (checked with 5000 points) are visually decimated rather than literally rendering every point — accepted as a non-issue at 320px screen width, confirmed by the developer ("looks right") rather than assumed. The per-file persistence change is a real format break (old lists.dat/matrices.dat images go dark) — acceptable and precedented, but means any lists/matrices saved before this session were lost on first boot under the new firmware (confirmed expected by the developer during the interview, not a surprise).
**Revisit when**: `kMaxCachedPoints` feels visually sparse on real dense data (raise the cap, watch Pico 1 bss); a future feature needs to mutate more than one list/matrix in a single operation (would need a real "save several indices" path, not assumed to exist today); Pico 2's own perf for these same paths gets profiled (still outstanding, see `next-session.md`) — the fixes here are Pico-1-motivated but board-generic, so Pico 2 should already benefit, just unverified.

## D34: SD-loadable apps — Python apps and `uf2loader`-based compiled apps both accepted into Phase 6; only in-process dynamic loading deferred

**Date**: 2026-07-21
**Status**: Accepted (same stocktaking session as D32/D33; a follow-on to
phase6-spec.md §9's original "SD-card app loading" candidate, refined
twice more within the same session before landing here)
**Context**: Asked to consider the complexity of letting 6A's app
framework load apps from the SD card and spec it as a Phase 6+ stretch
goal. Three passes within one session:
1. First pass split this into "SD-discovered MicroPython apps"
   (tractable) and "dynamically loaded native apps" (a real
   embedded-systems undertaking — relocator, flash-write safety, ABI
   versioning, fault recovery; ~150–250+ hrs, not recommended).
2. A follow-up question — would scoping compiled apps to Pico 2 only
   reduce that — surfaced that RP2350's MPU genuinely improves the risk
   picture (catchable faults instead of silent corruption) but doesn't
   remove the relocator/ABI/flash-safety work, which is the bulk of the
   estimate. In discussing alternatives, the developer raised `uf2loader`
   and a "reboot into app" model, which turned out to already be scoped
   in this project's own pre-Phase-1 `feasibility.md` (§4.4) — missed on
   the first pass.
3. A further question — can the `uf2loader` reboot path present as a
   launcher menu item indistinguishable from a Python app, rather than a
   separate reboot-to-a-different-menu experience — worked out to be
   yes, via the firmware performing its own UF2-format flash write into
   a reserved app-boot region (reusing the well-specified UF2 block
   format's own robustness, not inventing a new one) rather than relying
   on any programmatic hook into `uf2loader`'s own UI. That's enough of a
   complexity/UX win to promote it out of "deferred future phase" and
   into Phase 6 as a stretch item.
**Decision**:
1. **SD-discovered Python apps are accepted into Phase 6 core scope**, as
   new sub-phase 6B section §4.5 (see
   [phase6-spec.md](../phases/phase6-spec.md) §4.5, tasks 6B.15/6B.16,
   ~12 hrs added to 6B's estimate, 61→73 hrs). A second, SD-scanned tier
   of `AppRegistry` entries, each launching a declared MicroPython script
   — no new execution model beyond what 6B already builds.
2. **Reboot-based compiled apps (`uf2loader`-based) are accepted into
   Phase 6 as a stretch item** — new §3.4 (under 6A, since it generalizes
   the launcher/registry rather than being MicroPython-specific), ~25–35
   hrs, explicitly gated on a feasibility spike (parse one real `.uf2`,
   flash it to a scratch region, reboot into it, confirm the
   untouched-bootloader recovery path actually recovers) before
   committing to the rest. Mechanism: the calculator firmware itself
   parses a selected app's `.uf2` off SD and writes it into a reserved
   app-boot flash region (reusing UF2's own well-specified, robust block
   format rather than a bespoke protocol), then resets — no dependency
   on `uf2loader` exposing a "boot straight into app X" API it doesn't
   document having. `uf2loader` (or a minimal in-firmware bootstrap)
   remains the always-untouched safety net if the self-flash step ever
   has a bug.
3. **True in-process dynamic loading — code running concurrently with
   the calculator firmware, no reboot — stays deferred to a genuinely
   separate future phase**, not Phase 6 in any form, stretch or
   otherwise. This is the one approach where the hardest problem (a
   homegrown relocator) is unavoidable regardless of any of the above.
   Estimate unchanged: 120–200 hrs if scoped Pico-2-only (RP2350's MPU
   turns silent corruption into a catchable fault, and removes the
   "no cheap dual-board answer" objection since Pico 1 was never going
   to be protected either way), 150–250+ hrs for both boards.
**Rationale**: the SD card was never going to be execute-in-place memory
under any approach — something always has to stage code into RAM or
on-chip flash before it runs. Once that's accepted, the real choice is
between inventing a bespoke write-safety-and-relocation stack (the
in-process approach) versus implementing a well-specified, already
field-tested transport format (UF2) for a *whole replacement firmware*
that never needs relocating because it's linked at a fixed address like
every other build in this project already is. The reboot-based approach
being expressible as an ordinary launcher menu item — once it became
clear the firmware could do its own UF2 write rather than needing
`uf2loader` to expose an API — removed the last reason to treat it as
"a separate, worse-UX thing" rather than a first-class stretch feature.
Reboot-based handoff also sidesteps the GPL "combined work" question
in-process linking would raise, the same way this project's own
NOTICE.md already reasons about separately-distributed vs. linked GPL
code.
**Tradeoffs**: §3.4 still needs a real, carefully tested flash-write
step — smaller and better-specified than the in-process approach's, but
not zero risk; the "safety net stays untouched" constraint is
non-negotiable, not an optimization. No state handoff (Ans/variables/
graph state) between the calculator and a launched app either way —
solvable later via the existing SD-persistence pattern
(`lists.dat`-style) if ever needed, not built now. Two real open
questions remain and are tracked as P6-5/P6-6 in phase6-spec.md §8
rather than assumed away: whether §3.4 depends on `uf2loader` being
installed or becomes self-sufficient, and where the calculator's own
`.uf2` comes from at "return" time.
**Revisit when**: 6B is actually implemented (verify §4.5's manifest
scan against real SD hardware timing); before implementing §3.4, run its
~4–8 hr feasibility spike first — parse+write+reboot one real `.uf2` and
confirm the recovery path actually recovers — rather than building on
top of an unverified assumption; if true in-process dynamic loading is
ever seriously considered, it still needs its own dedicated phase and a
standalone relocator feasibility spike before anything else.

---

## D33: Phase re-scoping — Phase 4 = pre-release GC milestone, Phase 6 = non-calculator functions with MicroPython as its first app

**Date**: 2026-07-21
**Status**: Accepted (same stocktaking session as D32; refines it)
**Context**: Immediately after D32 split CAS into Phase 5, the developer
gave a clearer target shape for the remaining phases: Phase 4 should
"roughly provide full GC functionality" as a pre-release milestone, Phase
5 is CAS, and Phase 6 should be "non-calculator functions... ideally with
sub phases that can be completed in any order," with MicroPython as a
candidate first sub-phase ("first base app"). This directly resolves
D32's own open item — MicroPython's phase slot — which D32 had explicitly
left undecided.
**Decision**:
1. **Phase 4 gains a new closing sub-phase, 4D: GC completeness**,
   reusing the `4D` label CAS vacated. Scope is every TI-83/84+ parity
   gap the [parity stocktake](../notes/ti-parity.md) found
   (excluding CAS-tier items) plus the two lowest-risk
   [design-departures](../notes/design-departures-matrix-complex.md)
   ideas (home-screen matrix literals, complex-valued storage). Full
   item list, task table, and rationale in
   [phase4-spec.md](../phases/phase4-spec.md) §7–§8. **Phase 4's
   completion is now the project's pre-release milestone** — the point
   at which the calculator is a complete, TI-83/84+-class graphing
   calculator independent of CAS or programmability.
2. **MicroPython moves out of Phase 4 entirely, into Phase 6** as
   sub-phase 6B, no longer an open question. Phase 6 gains a new
   sub-phase 6A (app framework: a launcher screen + static app-registry
   table) that 6B rides on as its first consumer, so a second
   non-calculator feature later has a landing spot instead of a bespoke
   integration. New file [phase6-spec.md](../phases/phase6-spec.md)
   replaces the MicroPython content that used to be phase4-spec.md §7
   (4E) verbatim, adapted to launch through 6A.
3. **Phase 6's sub-phases are explicitly designed to be order-independent**
   once 6A exists (6B, future 6C+ apps, and release engineering/docs-site
   work all become parallel-safe) — a structural difference from Phases
   1–5's strict week-by-week sequencing, matching the developer's
   "ideally... any order" framing.
4. **Phase ordering is now 4 → 5 → 6** (GC completeness → CAS → apps),
   confirmed explicitly rather than left implicit.
**Rationale**: "full GC functionality" as an explicit pre-release
milestone gives Phase 4 completion real meaning — closing 4A–4C without
closing the parity doc's remaining gaps would leave "Phase 4 done" quietly
meaning "mostly done." Splitting MicroPython into its own app-framework
sub-phase (rather than bolting it directly onto the screen manager, the
original 4E plan) costs a small amount of upfront scaffolding (6A, ~14
hrs) in exchange for not re-solving "how does a non-calculator feature
plug in" from scratch the next time one is wanted.
**Tradeoffs**: 4D is a "grab-bag" sub-phase with ~20 mostly-independent
small items rather than one coherent subsystem (flagged as its own risk —
phase4-spec.md Risk 8, scope creep) — it's a different shape of risk than
4A–4C's deep-but-narrow subsystems. 6A adds a small amount of
indirection (an app registry + launcher) that a single-app MicroPython
plan wouldn't have needed on its own; justified only if a second app
actually shows up later.
**Revisit when**: 4D's scope is being implemented and any single item
(especially APD/brightness persistence, flagged feasibility-unknown) turns
out infeasible — drop it back to the wishlist rather than forcing it.
6A's design should be revisited once a real second app (Phase 6 §9
candidates) is actually picked up, to check the launcher/registry still
fits rather than needing to grow.

---

## D32: CAS split into its own Phase 5; docs-site + design-departures plans (stocktaking session)

**Date**: 2026-07-21
**Status**: Accepted (documentation/reflection session — no code touched)
**Context**: A stock-taking session to assess feature parity against the
TI-83/84+ (and, for the unbuilt CAS, TI-Nspire CX II CAS), plan a public
docs site with TI-guidebook-style workbooks, and consider departures from
TI's design for matrix/vector/complex handling. Along the way, CAS's size
and risk relative to the rest of what was Phase 4 (originally sub-phase
4D, ~124 of Phase 4's ~320 estimated hours — already the largest and
riskiest sub-phase per phase4-spec.md's own §1) made a clean phase split
worth doing now rather than leaving it bundled.
**Decision**:
1. **CAS is now Phase 5**, split out of Phase 4 sub-phase 4D. New file
   [phase5-spec.md](../phases/phase5-spec.md) carries the content
   verbatim (former §6.1–6.9 → top-level §2–§10), plus a new intro tying
   it to the [TI parity stocktake](ti-parity.md) §8
   (TI-Nspire CAS comparison) and the [design-departures doc](design-departures-matrix-complex.md).
   `phase4-spec.md` §6 is now a one-paragraph pointer; its task
   breakdown, performance benchmarks, risks (1, 2, 5), and open
   questions (P4-1/2/3) moved to phase5-spec.md's own §11–§14 (task IDs
   and risk/question numbers kept as originally assigned — `4D.n`,
   `Risk 5`, `P4-n` → `P5-n` — rather than renumbered, to keep any
   existing cross-references from other notes still resolvable by search).
2. **The old "Phase 5" (app framework, polish, release) becomes Phase 6.**
   Still spec-pending, no content change — just the number.
3. ~~**MicroPython (sub-phase 4E) stays put, unrenumbered, for now.**~~
   **Superseded by D33** (same session): MicroPython moves to Phase 6 as
   sub-phase 6B. What follows here is kept for history: its final phase
   slot (stay in Phase 4, follow CAS, or get its own phase) was
   deliberately left open — see `next-session.md`. Renumbering it
   speculatively before that call was made would just have created more
   churn to undo later.
4. **Docs site**: no code/config created this session (by design — see
   [docs-site-plan.md](docs-site-plan.md)). Recommends MkDocs Material,
   hosted via GitHub Pages/Actions, TI-guidebook-shaped workbook
   chapters, public user docs kept separate from the existing
   developer/spec docs tree. Suggested home: Phase 6.
5. **Design departures** (complex-valued variables/lists/matrices,
   home-screen matrix literals, vector ops, list↔matrix bridge, and the
   larger idea of unifying `matexpr`/`complexexpr`/`listexpr` into one
   tagged-value evaluator) are recorded as **ideas, not decisions** — see
   [design-departures-matrix-complex.md](design-departures-matrix-complex.md).
   Nothing there is committed; each would need its own scoping pass and
   its own D-numbered decision if picked up.
**Rationale**: CAS was always going to dominate whichever phase held it
(§1's own effort table made that visible before this session); giving it
a dedicated phase makes the phase-completion signal ("Phase 4 done") mean
something again instead of hiding CAS's much larger remaining scope
behind an already-shipped 4A–4C. Keeping MicroPython's slot open rather
than guessing avoids a second renumbering pass later. Plans-not-code for
the docs site and departures matches the session's own framing (stock-
taking/reflection, not implementation).
**Tradeoffs**: two renumbered phases (5→6) mean any old notes/commit
messages saying "Phase 5" now mean the app-framework phase's predecessor
concept, not CAS — worth reading dates when in doubt. `phase4-spec.md`'s
task/risk/question IDs (`4D.n`, `Risk 5`, `P4-1..3`) now point across a
file boundary into phase5-spec.md rather than staying self-contained.
**Revisit when**: MicroPython's phase slot gets decided (renumber then,
not speculatively now); the docs-site plan gets picked up for real
(Phase 6, or opportunistically earlier); any design-departures idea is
actually scheduled into a phase (gets its own D-number at that point).

---

## D31: Real math glyphs + a swappable 8x16 main font — Terminus default (testdrive)

**Date**: 2026-07-21
**Status**: Accepted (font test-drive session; supersedes D30 item 6's ASCII `<` polar stand-in and D24's pi-only glyph)
**Context**: On-device 4C testdrive (docs/notes/testdrive-2026-07-20-observations.md) flagged the ASCII `<` polar separator and plain `i` as papercuts, and asked to try a real font. Widened into a full pass over ASCII stand-ins used for math symbols across the UI, plus a font comparison.
**Decision**:
1. **The 8x16 main font is now a build-time choice** — `-DPICOCALC_FONT=spleen|juliamono|iosevka|unifont|terminus`, **default `terminus`**. The 5x8 small font stays Spleen always. Every variant ships the **same 32..140 slot map**, so the glyphs below work identically regardless of font. Rasters regenerate via `scripts/gen-<name>.sh`; external fonts are OFL/dual-licensed and vendored under `drivers/{juliamono,iosevka,unifont,terminus}/` (README + license only — the TTF/hex/BDF sources are fetched on demand, not committed).
2. **High-slot glyph map (127..140)**, baked into every font: 127 `π`, 128 ∠, 129 `θ`, 130 `σ`, 131 Σ, 132 χ, 133 `μ`, 134 imaginary-unit `i`, 135 store-arrow ⇒ (U+21D2), 136 `λ`, 137 `≠` (U+2260), 138 … (U+2026), 139 ² (U+00B2), 140 `√` (U+221A). Sourced from Unifont for spleen/terminus/unifont (Unifont sits 2px low, lifted via `--hexshift`/`--shift 2`); JuliaMono/Iosevka use their own TTF glyphs. The imaginary `i` is Unifont's serif **U+2139** for spleen/terminus/unifont (a slanted hand-drawn one "looked horrible" on device); JuliaMono/Iosevka use math-italic **U+1D456**.
3. **Substitutions applied** (see worklog): `format_complex` polar `<`→∠ and `i`→glyph; MODE Number row `a+bi`/`r∠θ`; pretty-print (`render/layout_builder`) rewrites `pi`/`i`/`theta` and the `->` store op up front in a `preprocess_glyphs` pass (so glyphs reach the plain-text fallback too, e.g. `3+2i`), while `sqrt` stays a real function identifier whose **name** renders as `√` (inline `√(x)`; big radical over the argument is KIV); home-screen result store indicator `>`→⇒; truncation `...`→… (`format_list`, matrix/complex text, slot editor); graph-trace/table polar label `th`→`θ`; stats results `σx`/`σy`, `Σx`/`Σx²`/…/`Σxy`, `r²`; inference `!=`→`≠`, `mu`/`sigma`→`μ`/`σ`; distribution `mu`/`lambda`→`μ`/`λ`.
4. **Tooling**: `bdf_to_utft.py` gained `--extra` (hand-drawn glyphs), `--hexfont`/`--hexmap`/`--hexshift` (bake 8-wide Unifont glyphs into a BDF font). New `scripts/ttf_to_utft.py` (freetype raster, `requirements-dev.txt` pins `freetype-py`) and `scripts/hex_to_utft.py` (native Unifont .hex, `--shift`).
**On-device font comparison (user, 2026-07-21)**: **Terminus + glyphs = preferred look.** Unifont looks good too but needed the 2px lift (done). Spleen + glyphs is the best pick if a thicker font is wanted. **JuliaMono looks worst** on the PicoCalc; Iosevka is ok but a little unbalanced from the rastering. The rasterized fonts would likely look good with antialiasing, a higher-res display, or a desktop emulator — all currently unplanned (see wishlist).
**Rationale**: a slot-map shared across fonts keeps every substitution font-agnostic and lets the font be a pure build flag; doing pretty-print glyph substitution as a string pass reaches the fallback path (implicit-multiply expressions like `3+2i`) that per-atom substitution misses; `sqrt` must stay an identifier or `1/sqrt(2)` loses its call/fraction structure (ASan-caught regression).
**Tradeoffs**: five committed 8x16 headers + four vendored license dirs (kept as a real selector, user's call — easy re-comparison); rasterized fonts read worse than the bitmap fonts at 8px with no antialiasing; `√` is inline-only (no radical vinculum yet); the JuliaMono/Iosevka `i`/glyphs use their own TTF shapes, not Unifont's, so the arrow/`i` differ slightly between fonts.
**Revisit when**: antialiasing / a higher-res panel / a desktop emulator makes the rasterized fonts viable (wishlist); the big-radical `√` display is wanted; someone wants to prune the non-default fonts.

## D30: 4C complex numbers as-built — complexexpr scalar-span reuse, i reservation, kText eigenvalues (P4-9, P4-7)

**Date**: 2026-07-20
**Status**: Accepted (Session 18, sub-phase 4C; user decisions taken upfront)
**Context**: Phase 4C needed the two calls the spec left open (P4-9 default number mode; P4-7 complex matrix eigenvalues — user picked "add now" over the spec's own "likely defer" note) plus the usual as-built reconciliation: `Complex` can't flow through tinyexpr (double-only), so the home-screen path needed the same "second small evaluator" treatment 4A gave matrices.
**Decision**:
1. **P4-9: REAL is the default number mode** on first boot (TI parity), matching the existing DEG/FLOAT-style defaults. `NumberMode {kReal, kRectangular, kPolar}` (`math/types.hpp`) mirrors `AngleMode`'s storage pattern exactly (file-local global in `functions.cpp`, `GraphState` shadow field, MODE-screen row) — persisted GraphState bumps to **PCG5**.
2. **`i` is reserved globally**, not just inside complex mode — same tier as `e` (D11): unbound in the real engine's variable table (`build_lookup` skips it), blocked as a store target everywhere a single-letter target is validated (`Engine::evaluate`, `matexpr::parse_store_target`, `listexpr`'s `seq` var, `solve_expr`'s solve var), all with a pointed "i is reserved (imaginary unit)" message. Case-sensitive per D19.
3. **`math::complexexpr`** (`src/math/complex_expr.{hpp,cpp}`) is a recursive-descent evaluator over `Complex`, structurally mirroring `matexpr` (D28) but simpler — no tagged scalar/matrix `Value`, everything is `Complex`. It special-cases exactly two things: the identifier `i` (and the adjacent-literal `2i` shorthand) and the spec's complex-aware function set (sqrt/exp/ln/sin/cos/tan/asin/acos/atan/abs/arg/conj/real/imag). **Everything else — `pi`, `e`, `theta`, `ans`, bare variables, and the entire rest of the real catalog (`ncr`, `factorial`, `round`, the distributions, ...) — is handed to `eval_field` as an opaque real span**, the same scalar-subterm technique `matexpr::parse_scalar_span` uses. This means non-REAL number mode doesn't lose access to the real catalog as long as a given catalog call's own arguments don't need to be complex.
4. **`evaluate()` is side-effect-free** (no Ans/store writes) — a deliberate divergence from `matexpr`/`listexpr`'s self-applying convention. The home-screen dispatch (`HomeScreen::evaluate_input`) needed it as a **probe**: in plain REAL mode with no literal `i`, it still calls `complexexpr::evaluate()` once before the real engine, purely to upgrade a would-be `NaN` (e.g. `sqrt(-4)`) into "Non-real result" — without double-committing Ans/a store, since the probe's own result is simply discarded when it's real or fails to parse (the common case falls through to the unchanged, full-catalog real path). When the mode is non-REAL or `i` appears, `complexexpr` is authoritative and the dispatch applies its `Ans`/store effects itself, exactly once.
5. **Complex results can't be stored** (`5->a` works if real; `2i->a` errors "Complex results can't be stored") — `Variables::vars` stays `calc_t`-only; widening it is out of scope. `Ans` likewise stays real-only: a genuinely complex result updates neither `Ans` nor the stored variable, matching the `Array`/list precedent (D22 §5: "`Ans` stays scalar") rather than adding a parallel complex-Ans cache.
6. **`format_complex`** (`math/format.{hpp,cpp}`): rectangular ("3 + 2i", pure-real/pure-imag/unit-coefficient elision) and polar. **No angle glyph was baked** — the vendored Spleen BDF has no true U+2220 ANGLE codepoint (checked; nearest are angle *quotation marks*, wrong shape), so polar uses **ASCII `<`** (`"2<60"`), the common EE phasor-notation convention, e.g. `2∠60°` written `2<60`. A real glyph is a follow-up if this reads as a papercut on hardware.
7. **P4-7: matrix eigenvalues now return the full spectrum.** The Hessenberg+Wilkinson-QR core (D28) is factored into `eigen_core` (`math/matrix.cpp`), which fills `Complex` (never errors on a conjugate pair now — that's a legitimate spectrum entry). Two public wrappers: `eigenvalues(Array&)` — **unchanged behavior**, still errors "Complex eigenvalues" if any pair is complex, so existing real-only callers/tests are untouched — and the new `eigenvalues_complex(Complex*, int*)`. `mat_expr.cpp`'s `eigenvals([A])` calls the new one: an all-real spectrum is still `Kind::kList` (storable into l1..l6, unchanged); a spectrum with a conjugate pair is a new **`Kind::kText`** (`matexpr::Result` gained a `text` field) — a formatted, unstorable string like `{i,-i}` (always rectangular, independent of the global number mode — matrix results aren't tied to the home-screen a+bi/polar setting). Storing a complex spectrum errors "Complex results can't be stored", same wording as item 5.
**Rationale**: Reusing `eval_field` for "everything not explicitly complex-aware" gets the whole real catalog into complex-mode expressions for free instead of hand-porting ~70 functions; a side-effect-free probe is the only way to get REAL-mode's "Non-real result" wording without either double-evaluating with side effects or duplicating the real engine's full dispatch; keeping `eigenvalues()`'s contract frozen avoids touching 4A's existing test surface while still exposing the richer spectrum through the string-expression layer where D28 already routes eigenvalue results.
**Tradeoffs**: No complex-valued variable storage (would need widening `Variables` — real work, deferred); no baked ∠ glyph (ASCII `<` stand-in); the REAL-mode NaN-upgrade probe re-parses every plain REAL expression once through `complexexpr` on Enter (home-screen-only cost, human interaction speed — never touches the graphing hot path per the spec's own performance note); `eigenvals()`'s complex form can't be stored into l1..l6 (lists are real-only) or nest inside a larger matrix expression (same limitation `dim`/`eigenvals` already had). Pico 1: text +7648 B, bss +68 B (~188.7 KB of 264 KB — unchanged watch item from D28).
**Revisit when**: on-device eval judges the `<` polar stand-in as confusing (bake a real glyph, extending the font by one slot past the D24 pi glyph at 0x7F); complex-valued variable storage gets requested; the REAL-mode probe's extra parse is ever measurable on hardware (it shouldn't be — Enter-rate, not frame-rate).

## D29: 4B graph analysis / CALC menu as-built — cursor-cycle intersect (P4-6), polar fnInt area-only (P4-8)

**Date**: 2026-07-20
**Status**: Accepted (Session 17, sub-phase 4B; resolves phase4-spec P4-6 and P4-8)
**Context**: 4B needed the two calls the spec left open — how CALC intersect picks curves when more than two are graphed, and whether polar `fnInt` is area-only or also offers arc length — plus the numeric calculus primitives (extremum, derivative, integral) that 4B needs and didn't exist yet.
**Decision**:
1. **P4-6: intersect curve picking is cursor-cycle (TI-84 behavior)**, not a list/explicit picker. `graph::AnalysisSession` (`src/graph/analysis_cursor.{hpp,cpp}`) drives it: Up/Down rides curves through "First curve?"/"Second curve?" prompts, ENTER locks each pick, and picking the same curve twice is refused. Once bounds are being collected the cursor locks to the chosen curve (no more slot cycling), matching the rest of the CALC flow.
2. **P4-8: polar `fnInt` computes area only** ($\frac{1}{2}\int r^2\,d\theta$), matching TI; no arc-length option. The integral always runs in radians internally regardless of the active angle mode, so the result is a true area even in DEGREE mode.
3. **Numeric calculus primitives** added to `src/math/numeric_solve.{hpp,cpp}` (the 4A solver file, per the spec's own file plan), each with a callback-based core (`EvalFn`) plus an expr-string wrapper — the callback form is required because parametric/polar integrands (e.g. `Y(t)*X'(t)`) aren't expressible as a single expression string:
   - `numeric_extremum`/`numeric_extremum_fn` — Brent's method (golden section + parabolic interpolation, derivative-free).
   - `numeric_derivative`/`numeric_derivative_fn` — central difference + one Richardson extrapolation step (O(h⁴)).
   - `numeric_integral`/`numeric_integral_fn` — adaptive Gauss-Kronrod G7-K15, depth-capped (`kMaxIntegralDepth` = 12) so oscillatory/singular integrands can't hang rather than converging exactly; Kronrod nodes skip panel endpoints, so endpoint singularities (`1/sqrt(x)`) integrate cleanly.
4. **Graph analysis engine** (`src/graph/analysis.{hpp,cpp}`) is mode-aware: parametric slope = (dy/dt)/(dx/dt); polar slope differentiates the Cartesian forms `r·cos(θ)/r·sin(θ)`, which stays correct in degree mode without a separate formula; intersect only finds same-independent-variable crossings — documented as a limitation for parametric/polar (won't find curves that cross at different parameter values).
5. **UI**: a new F6 softkey labeled "CALC" on the graph screen opens `apps::CalcMenuScreen` (7 ops, form-list); typed commands `calc`/`analyze` jump straight there. The interactive flow reuses the graph screen's existing `TraceCursor`-riding machinery, driven by `AnalysisSession`. Min/max keep the TI "Guess?" step in the UI for parity, but Brent's method underneath only uses the bracket and ignores the guess value — a judgment call, flagged to revisit if it feels wrong on hardware.
6. **Spec correction**: phase4-spec.md §4.3/4.5 assumed a Phase 3 shaded-region primitive existed to reuse for `fnInt` shading — it doesn't exist anywhere in the codebase (verified by exploration). Shading was built new in `graph_screen.cpp`: a column-based fill reading the existing cached plot-y column array (function mode only), respecting the §8 strip-safety idempotent-render rule.
**Rationale**: cursor-cycle intersect and area-only polar `fnInt` match TI-84 muscle memory the rest of the app already follows; callback-based numeric cores are the only way to support parametric/polar integrands without inventing a second expression grammar; depth-capped adaptive quadrature bounds worst-case runtime instead of hanging on pathological integrands.
**Tradeoffs**: intersect can't find parametric/polar curves that cross at different parameter values; the min/max Guess step is UI-only and not fed to the solver; polar `fnInt` has no arc-length option.
**Revisit when**: on-device eval judges the Guess-step cosmetic mismatch as confusing; arc length gets requested; multi-curve intersect at different parameter values becomes a real need.

## D28: 4A matrices + numeric solver as-built — TI [A]-[J] syntax, eigen errors on complex, dual solver surface

**Date**: 2026-07-20
**Status**: Accepted (Session 16, sub-phase 4A; user decisions taken upfront)
**Context**: Phase 4A needed three user calls (matrix reference syntax, real-only eigenvalue behavior on complex pairs, solver UI shape) plus the usual as-built reconciliation with the Phase 3 `Array` reality (get/set streaming, non-copyable — the spec's reference-returning `Matrix` class sketch doesn't fit).
**Decision**:
1. **TI-style `[A]`-`[J]` syntax** (user pick), lowercase `[a]` accepted, uppercase displayed. Matrices can't flow through tinyexpr and their operators aren't element-wise, so `math::matexpr` is a **recursive-descent evaluator** over tagged scalar/matrix values (not a listexpr-style rewrite): `[A]*[B]`, `2*[A]`, `[A]/3`, `[A]^-1`, `[A]^T` (also `^t`), `[A]^n` (n <= 100), `[A](r,c)` element access (1-based), det/rank scalars inline, inverse/transpose/rref/ref/augment/identity matrix-valued, `dim`/`eigenvals` whole-expression list forms, stores `-> [C]` / `-> lk` / `-> a`. Scalar subterms go through `eval_field` (full engine syntax). Routed **first** in `evaluate_input` ([X] tokens are unambiguous; `identity(` is the only no-token trigger). Matrix results land in a persistent **MatAns** buffer; history shows `[[1,2][3,4]]` truncated at ~40 chars.
2. **Matrix layer = `matops` free functions over 2-D `Array`** (`src/math/matrix.{hpp,cpp}`), the same deviation lists made: streaming row buffers (200 doubles), no references. LU det (direct 1x1-3x3), Gauss-Jordan inverse, rref/ref/rank with relative pivot tolerance (1e-12 * max|a|), reshape (row/col-overlap preserving — editor DIM). **Eigenvalues: Givens Hessenberg + Wilkinson-shifted QR, n <= 10, real only; a complex conjugate pair is an error** ("Complex eigenvalues", user pick — a partial answer would mislead; complex support is 4C/P4-7). Output is a 1-D list (descending) so results flow into l1..l6. `MatrixStore` [A]-[J] (10 slots, 99x99 editor cap; total elements <= 10000 per Array), persisted to `/picocalc/matrices.dat` (magic **PCM1**, lists_persist pattern incl. all-or-nothing PSRAM load + late-init retry).
3. **Matrix editor** (`matrix` / `mat` typed command): one matrix at a time, TAB cycles [A]-[J] + read-only Ans view, F7 DIM (`rows,cols` prompt, reshape), F8 clear, ENTER/typing edits cells (advance right-then-down), strip-safe cached render.
4. **Solver = both surfaces** (user pick): `math::numeric_solve` (bisection to a tight bracket + Newton polish; Newton-from-midpoint fallback when no sign change; lo == hi = explicit-guess form; solve variable saved/restored) behind (a) a `solve` form screen (equation with optional top-level `=`, variable cycle, Lower/Upper/optional Guess; root -> variable + Ans, residual + iterations shown) and (b) **inline `solve(f, var, lo, hi)` / `solve(f, var, guess)` / `solve(lhs=rhs, ...)`** substituted to a numeric literal pre-evaluation (composes anywhere; innermost-first like the list reductions).
5. **ArrayStore pools grown** for the matrix population: SRAM slabs 12 -> **28** (+32 KB bss), PSRAM regions 12 -> **24** (bookkeeping only). Pico 1 is now text ~337 KB / **bss ~188 KB of 264 KB** (~76 KB stack/heap headroom — recheck at the 3D.14/4A Pico 1 pass). `kMaxCatalogEntries` 56 -> 72 (12 help-only rows: matrix functions + solve).
**Rationale**: TI muscle memory for syntax; honest errors over silent partial eigen-results; the solver's two surfaces share one engine so 4B's zero/intersect reuse is free; matops-over-Array keeps everything host-testable (199 matrix + 27 solver checks).
**Tradeoffs**: No matrix literals on the home screen (editor is the entry path — watch on device); `dim`/`eigenvals` can't nest inside larger expressions; element *stores* (`5 -> [A](2,3)`) are editor-only; Pico 1 bss headroom narrowed to ~76 KB.
**Revisit when**: 4C complex lands (eigen complex pairs, P4-7); home-screen matrix literals get missed in practice; Pico 1 headroom pinches (shrink slab count or move small-tier matrices to PSRAM).

## D27: 3D inference + stat plots as-built — Alt-driven tests, separate intervals (P3-6), Plot1-3 layer (P3-5)

**Date**: 2026-07-20
**Status**: Accepted (Session 15, sub-phase 3D conventions; resolves phase3-spec P3-5 and P3-6)
**Context**: 3D needed the calls the spec left open: whether tests bundle confidence intervals (P3-6), how stat plots share the graph screen (P3-5), how the alternative hypothesis enters the API, and how 2-D inputs (contingency tables, ANOVA groups) map onto the flat l1..l6 lists.
**Decision**:
1. **P3-6: tests do NOT bundle intervals.** Intervals are their own entries (TI-style) in the same `test` screen — no clutter, clean signatures. `TestResult` carries statistic/p/df/df2/estimate/se; `Interval` carries center/low/high/moe/conf.
2. **P3-5: separate Plot1-3 stat-plot slots** (typed `plot` command, alias `plots`; config persisted in GraphState — magic bump to **PCG4**, one-time state reset on first boot). Plots draw in the graph viewport alongside (under) the mode's functions so regression overlays stay on top; slot colors orange/cyan/magenta; **'Z' on the graph screen = ZoomStat** (5% margins; histogram y from 0).
3. **Inference API** (`math::stats` in `src/math/infer.{hpp,cpp}`): mean/proportion/slope tests take an `Alt` (`!=`, `<`, `>`); p-values use the new one-sided `dist` survival functions directly (far-tail precision — no 1-cdf rounding). Two-sample t defaults to **Welch** (pooled optional, Welch-Satterthwaite fractional df through the real-df `t_inv`); paired t = one-sample on streamed differences; 1-prop z is the score test (se from p0), 2-prop z pooled; intervals use Wald proportion se. Chi-square 2-way and ANOVA take their **columns/groups as l1..lk lists** (2-6) — there is no 2-D Array until the Phase 4 Matrix (deviation from the spec sketch). GOF df = k-1 (no df override). linreg t-test: H0 slope=0, two-pass centered sums, n>=3. A lightweight streaming `mean_sd` replaces `one_var` in the t machinery (skips the 64-pass quartile selection).
4. **`test` command** (alias `infer`) — 15 kinds in one L/R cycle (10 tests + 5 intervals); t kinds get a Data/Stats source toggle (spec UI sketch); z kinds are summary-only (sigma-known problems are summary problems). Count fields reject non-integers. Results are cached text lines (strip-safe §8).
5. **StatPlot strip-safety split**: `recompute_stat_plots()` (called from GraphScreen's recompute) caches histogram bins (<= 64), box five-number summaries + 1.5-IQR fences + within-fence whiskers, and normprob **sorted-copy + Blom-quantile Arrays** (render never calls ndtri — Pico 1 softfloat). `draw_stat_plots()` only streams and draws. Box plots are **modified box plots** (outliers past the fences as marks) drawn in fixed horizontal bands at 1/4, 2/4, 3/4 viewport height, ignoring the y window (TI behavior). Histogram auto bin width = span/10.
6. **Deviations noted**: no freq-list weighting in stat plots v1 (spec sketch had `freq_list`); scatter/normprob draw all points per frame — fine on the Pico 2 full framebuffer, re-judge per-strip cost at the 3D.14 Pico 1 pass.
**Rationale**: TI conventions where users have expectations (separate TESTS/intervals, modified box plot, ZoomStat), Welch as the safer 2-samp default, and the cache/draw split keeps every new render path §8-legal.
**Tradeoffs**: 15-entry kind cycle is long (no submenu); contingency tables live in lists rather than a matrix editor until Phase 4; PCG4 resets saved graph state once; Pico 1 bss now ~147 KB of 264 KB and text ~305 KB (map re-check queued for 3D.14).
**Revisit when**: 4A matrices land (native contingency tables); stat-plot freq weighting or a histogram-of-frequencies is missed on device; the kind cycle grates (add category grouping).

## D26: Storage health — retry-forever heartbeat, SD hot-plug, red status-bar indicators

**Date**: 2026-07-19
**Status**: Accepted (Session 15 — implements the Session 14 observation batch, `session14-observations-verbatim.md`)
**Context**: On-device: SD showed "no card" after an extended power-off and only a reboot recovered it. Root cause: the D14 late-init retries ran every 2 s but stopped for good at 30 s uptime. Also unhandled: card eject/insert while powered (DET pin was only read inside init), and there was no visible signal that a subsystem was down.
**Decision**:
1. **Retries never give up**: the 30 s window is now just the fast phase (2 s cadence, D14 rail settle); after it, an unhealthy SD or PSRAM keeps retrying on a **10 s heartbeat** indefinitely. `run_self_tests()` skips subsystems already green (the PSRAM word test bump-allocates 256 B per run and must not repeat forever; the SD probe stops rewriting its file once OK).
2. **Hot-plug**: the main loop polls the DET pin every ~1 s. **Eject** → `Storage::on_card_removed()` (f_unmount + `sd::invalidate()` so FatFs sees NOINIT), storage marked down, `g_sd_test = kNoCard`. **Insert** → the retry timer resets, so the remount attempt is immediate.
3. **Persisted state loads exactly once** per power-on: a late-mounted card loads then, but an eject + re-insert must NOT reload — the in-memory working state is newer than the files. (Lists already load-once.)
4. **Status-bar indicators**: red `SD` / `PSRAM` after the title while the subsystem is unhealthy (`ui::set_health_flags`, drawn by `draw_status_bar`); the main loop updates on any change and repaints the status band (battery-refresh pattern). Only screens using the shared chrome show them (WINDOW/editors draw their own plain bars — acceptable; the home screen is where you look).
5. Same batch: Y=/PAR/POL editor rows truncate long expressions with `...` before the enable checkbox (stored regression models ran beneath it).
**Rationale**: "Disappear when the retries finally work" (the request) requires retries that never stop; eject-drop-the-mount prevents half-written state files; load-once protects the session from stale reloads.
**Tradeoffs**: A failing-but-present card costs a ~1 s blocking init attempt every 10 s forever (visible as a periodic hitch only in that broken state); DET polling assumes the pin is configured (guaranteed — boot always reaches `sd::init()`'s GPIO setup); mid-write ejects can still corrupt the file being written (poll is 1 s — hardware can't prevent it).
**Revisit when**: the periodic init hitch is noticeable in real use (make the attempt async/backoff), or a future RTC/logging feature needs write-behind flushing on eject.

## D25: 3C distributions as-built — two-sided CDFs (P3-4), real-df wrappers, `dist` guided screen

**Date**: 2026-07-19
**Status**: Accepted (Session 14, sub-phase 3C conventions; resolves phase3-spec P3-4)
**Context**: 3C.2-3C.8 needed the naming/convention calls the spec left open: one- vs two-tailed CDFs (P3-4), how discrete arguments behave, error signalling without exceptions, and the helper-screen shape.
**Decision**:
1. **P3-4: TI-style two-sided CDFs** for the continuous distributions — `cdf(lo, hi, ...)` = P(lo <= X <= hi), `hi < lo` is a domain error; open tails use +/-1e99 (any far value works — `normal_cdf` saturates |z| > 40 before cephes, whose `ndtr` overflows on 1e99). `inv(area, ...)` inverts the **lower tail**, area in (0,1) exclusive (chisq/F additionally accept area = 0 -> 0). Rationale: the two-arg form is what the stats workflows actually use, and it matches the TI the UI imitates; lower-tail inv matches invNorm/invT.
2. **Spec naming kept** (`normal_pdf/cdf/inv`, `t_*`, `chisq_*`, `f_*`, `binomial_pmf/cdf`, `poisson_*`, `geometric_*`), registered **full-arity** (no default-arg shorthands; tinyexpr is fixed-arity — spec §5.3). Deviation from the spec sketch: discrete `k`/`n` parameters are `calc_t`, not `int`, so every function binds directly to the parser; the TI integer rule is enforced at runtime — **pmf arguments must be integers** (within 1e-9, else NaN), **cdf floors k** (P(X <= floor(k))). Geometric counts trials until first success (k >= 1). Domain errors return NaN (project error convention; the engine already displays NaN).
3. **Real-valued degrees of freedom throughout**: t/chisq/F are built on `incbet`/`incbi`/`igam`/`igamc`/`igami` directly (the reason the integer-df cephes wrappers were not vendored, 3C.1); pdfs/pmfs are `std::lgamma` closed forms. No local bisection needed — cephes' own inverses cover all three inverse CDFs (`t_inv` via `incbi` + the symmetric-tail transform, `chisq_inv` = 2·igami(df/2, 1-area), `f_inv` via `incbi` + the beta-to-F transform).
4. **`dist` typed command** (D20) → guided form: Distribution and Function rows (L/R cycle; pdf|pmf/cdf/inv per distribution), the combination's parameters as **InputLine full-expression fields** (WINDOW pattern, ENTER edit / DEL clear-and-edit), Calculate. Parameter values live in shared named slots, so switching pdf -> cdf keeps mu/sd. Calculate builds the equivalent catalog call, **evaluates it through the engine** (so Ans updates — TI DISTR paste-and-run behavior) and shows both the call and the result.
5. **Registration mechanics**: catalog fp3/fp4 casts, `kMaxCatalogEntries` 32 -> 56 (18 new rows, 47 total), help FUNC summaries draw at `max(kSummaryCol, sig_width + 1)` so `normal_cdf(lo,hi,mu,sd)` doesn't overlap. Signatures now use descriptive parameter names; the host test correspondingly builds a numeric call at the declared arity instead of parsing the signature text.
6. **`isfinite` link shim** (`src/math/cephes_support.c`, compiled into the cephes target): cephes `gamma.c` calls `isfinite()` as an extern function, but newlib and macOS libm only provide the macro — the cephes static lib had never been linked into a binary until `lgam` got referenced. Fix lives outside the read-only vendored tree per the AGENTS.md driver-workaround rule.
7. **Reference-value infra**: `tests/host/gen_dist_vectors.py` generates the test vectors at 50-digit precision with mpmath; Python dev-deps go in the gitignored `.venv` and are tracked in `requirements-dev.txt` (developer rule, this session).
**Rationale**: Matches the handheld conventions users know, keeps every function parser-bindable with one signature, and reuses cephes' inverses instead of writing a solver.
**Tradeoffs**: No one-sided CDF shorthand (type `-1e99` for the lower tail); pmf integer strictness means `binomial_pmf(2.0000001,...)` errors rather than rounding; `chisq_inv` computes `1-area` (precision loss for area within ~1e-16 of 1 — irrelevant at calculator precision); Pico 1 text grew ~30 KB now that cephes really links.
**Revisit when**: 3D inference needs additional tail conventions or vectorized (list-argument) distribution calls; a one-sided shorthand is repeatedly missed on device.

## D24: Session 13 usability batch — lift operands (literal broadcast fix), range(), reduction args, aliases, pi glyph

**Date**: 2026-07-19
**Status**: Accepted (Session 13 — dispositions for the on-device observation batch in `phase3A-3B-observations-verbatim.md`)
**Context**: First real usage of the Session 12 firmware produced a verbatim observation list: two outright bugs (brace-literal broadcast rejected; home screen unreachable after HOME/trace use in the graph screen), several direct requests (range(), mean/median/std on the home screen, command shortcuts, a computing indicator, pi glyph, list-editor color), and design questions to park.
**Decision**:
1. **Lift operands**: inside a vector-lifted expression, top-level brace literals and wrapper calls (sort_asc/sort_desc/cumsum/delta_list/seq/range) are evaluated into side arrays (4 slots, handed out monotonically per evaluate() and released per lift, so nesting never aliases) and bound as extra engine variables (`lopa`..`lopd`). Fixes the reported `{1,2,3}+2` / `{1,2,3}+{2,2,2}` errors at the general level and makes wrapper results compose: `cumsum(range(1,4))+1`, `range(1,9)*l1`.
2. **`range(lo, hi[, step])`**: inclusive endpoints, default step of +/-1 toward hi (`range(5,1)` counts down), backed by `listops::seq` with the identity formula; same 10000 cap. The quick generator the large-array testing gap asked for.
3. **Reductions gain `mean`/`median`/`stdev`** (sample Sx, via `stats::one_var`; `std` accepted as an alias), **and reduction arguments generalize** from bare list names to any list expression, substituted innermost-first — this lifts the D22 bare-arg limitation (`sum(range(1,10000))`, `mean(l1*2)` work). NaN reduction results (stdev of 1 element) error as "Undefined result".
4. **Typed-command aliases**: `?` = help, `list` = lists, `stat` = stats (no collisions — none parse as expressions).
5. **Stats Calculate pushes one "Computing..." frame** before the synchronous compute (flag + forced `render_frame()`, render stays idempotent). Closes the D23 revisit — added ahead of the timing-feel eval since it costs one frame.
6. **Pi glyph**: `bdf_to_utft.py` gained `--map DEST:CODEPOINT`; the 8x16 main font bakes U+03C0 at the unused DEL slot (0x7F, `gfx::kGlyphPi`) and the layout builder renders the identifier `pi` as that glyph. The 5x8 BDF has no pi, so the small font is untouched (draws blank if ever asked).
7. **HOME nav invariant**: `ScreenManager::switch_to()` never replaces the root screen — at depth 1 it pushes instead. Root cause of the observed breakage: F4 trace from the home screen went through `switch_to` → `replace()`, overwriting the stack root with the graph screen, after which ESC/HOME had no home screen to return to.
8. **List editor placeholder color** is decided by exact cell text ("_" / "---"), not a leading '-', so negative numbers render white like positive ones.
9. **Parked on the wishlist**: greek letters/subscripts in stats output, JuliaMono font swap (licensing + baking check), scientific constants, unit conversions, >6 lists and SD list-data files (CBL/CBR).
**Rationale**: One general operand mechanism fixes the literal bug and delivers range() composability instead of two special cases; generalized reduction args + range() directly serve the observed "no quick way to generate/test large arrays" gap; the rest are verbatim developer requests from device use.
**Tradeoffs**: ~9 KB more bss (operand chunk buffers + 5 static Arrays; Pico 1 bss now ~135 KB of 264 KB); `lopa`..`lopd` are technically bindable identifiers while a literal is present (harmless, undocumented); reduction substitution evaluates arguments eagerly, so a slow argument computes even when the surrounding expression later errors; literals stay capped at 64 elements, operands at 4 per lift.
**Revisit when**: the 4-operand or 64-element caps pinch in practice; complex dtype (4C) reaches the lift/operand path; or `sum({...})`-style reduction-over-operand syntax confusion shows up on device.

## D23: 3B stats as-built — LM for iterative fits, rank-selection quartiles, TI r conventions

**Date**: 2026-07-19
**Status**: Accepted (Session 12, sub-phase 3B implementation decisions; resolves phase3-spec P3-3)
**Context**: Task 3B needed four calls the spec left open: the iterative solver for logistic/sinusoidal (P3-3), how to get quartiles/medians without mutating or copying a possibly-80 KB PSRAM list (the D22 API has no `data()` to sort through), what r/r² mean per model, and where the results UI lives.
**Decision**:
1. **P3-3: Levenberg-Marquardt** (classic Marquardt damping, analytic Jacobians, <= 100 iterations, ~2 streaming passes per iteration) for logistic and sinusoidal. Seeds: logistic from a linearized logit at ceiling `1.05*max(y)`; sinusoidal from a frequency scan (0.25-cycle grid over the x-span, <= 64 candidates, each solved as a linear sin/cos fit in one shared streaming pass) — LM alone cannot find the frequency basin. `converged=false` (surfaced as a results-screen warning) when the cap is hit; the fit is still returned.
2. **Quartiles/medians by streaming rank selection, not sorting**: binary search over the order-preserving uint64 image of the doubles, one weighted counting pass per bit, all requested ranks advancing in the same passes (<= 64 passes per batch total). No temp region, no allocation, identical code path for plain, frequency-weighted (integer freq >= 0, TI rule; freq 0 excludes the element), and x-range-filtered selections (which is how median-median gets its group y-medians). Med-med caveat: x-ties straddling a group boundary land whole in the outer group (value-based grouping), a documented deviation from a strict positional split.
3. **r/r² per TI convention**: `r` only for linear and the linearized fits (ln/exp/power, where it describes the linearized regression, as on the handheld); polynomial degree >= 2, logistic, sinusoidal, med-med report `r²` only, computed as 1 - SSE/SST on the original data (NaN skips the results line). Polynomial fits standardize x (center+scale) before forming normal equations, then expand coefficients back — quartic on year-scale x stays conditioned.
4. **UI: typed `stats` command** (D20 pattern) → one screen, form + results phases. Form: Analysis (1-Var / 2-Var / 10 regressions), source lists, optional freq list (1-Var), optional Store-to y1..y7 (regressions, task 3B.8 — writes the numeric model via `format_model`, enables the slot, saves graph state; SinReg coefficients are degree-converted when the global mode is DEGREE per spec §10). Results are cached text lines (strip-safe render), scrollable.
**Rationale**: LM is the spec's lean and the robustness matters more than the extra solve code (shared with the polynomial path anyway); rank selection keeps stats allocation-free and O(passes) on PSRAM lists where a pair-sort would have needed new machinery; TI conventions keep results comparable to the handheld the UI imitates.
**Tradeoffs**: Selection costs up to 64 streaming passes (~0.7 s worst case on a full 10000-element PSRAM list — one-shot per Calculate, acceptable); no `r` for the nonlinear fits (TI-consistent); sinusoidal frequency grid caps at ~16 cycles over the x-span (denser oscillations need pre-scaled x).
**Revisit when**: 3C/3D need the same quantile machinery with real-valued weights; a user hits the frequency-grid cap; or the stats screen needs a "computing..." indicator (compute is synchronous in on_key — judge on device with large lists).

## D22: Array as-built API (get/set, tiered store) + home-screen list syntax

**Date**: 2026-07-19
**Status**: Accepted (Session 11, task 3A implementation decisions)
**Context**: Implementing D21's `Array` surfaced two facts the spec sketch (§2.1) didn't account for: (1) the PSRAM is SPI-attached and **not memory-mapped** (`platform::Psram` hands out addresses, access is `read()`/`write()`), so `calc_t& at()` / `calc_t* data()` cannot exist for the PSRAM tier; (2) `Psram::alloc` is bump-only (no free), so resizable lists need an allocation scheme above it. Separately, lists cannot flow through tinyexpr (scalar `double` values only), so §3.2's "callable from the home screen" needed a concrete syntax layer.
**Decision**:
1. **Element access is `get(i)`/`set(i, v)` plus bulk `read_range`/`write_range`** (chunked DMA underneath); no reference-returning accessors, no raw `data()`. This is also the natural shape for D21's tag-aware access rule — future complex elements change the accessor internals, not the callers.
2. **`ArrayStore` = fixed-size recycling on both tiers**: a pool of 12 x 2 KB SRAM slabs (one slab = one small array, <= 256 doubles) and up to 12 x 80 KB PSRAM regions (one region = one large array at full 10000-double capacity) handed out from a free-list over the bump allocator. Fixed sizes make recycling trivial and fragmentation impossible; crossing 256 elements migrates slab→region (and back on shrink, freeing the region).
3. **Home-screen list syntax** (`math::listexpr`, layered above the engine): `{1,2,3}` literals (elements are full expressions), `l1..l6` references (lowercase, D19), `-> lk` store, `sum/prod/length(l1)` **scalar reductions with bare-list args only** (substituted as numeric literals, so they embed in any scalar expression), wrappers `sort_asc/sort_desc/cumsum/delta_list(X)` + `seq(expr, var, lo, hi, step)`, and **vector lift**: any other engine expression mentioning `l1..l6` (e.g. `sin(l1)+2*l2`) is compiled once with `l1..l6` bound as per-element variables and evaluated element-wise in 256-element chunks.
4. **Sort semantics**: `sort_asc(l1)` with a bare list arg sorts **in place** (spec §3.2); compound args (`sort_asc(l1+0)`) sort a copy. Large-list sorts use an external merge sort through one temp PSRAM region (~256-element runs, streaming merges).
5. **`Ans` stays scalar** — list results display but don't set Ans; list persistence (`lists.dat`, magic `PCL1`, per-list dtype+count header + streamed raw elements) saves on every mutating edit/command, all-or-nothing load that waits for PSRAM on cold boot (D14).
6. **Editor entry is the typed `lists` command** (D20 command layer); in-editor ops use non-global keys: ENTER/type to edit, DEL delete row, F6/F7 (Shift+F1/F2) sort, F8 (Shift+F3) clear list. F1-F5 keep the global scheme.
**Rationale**: The seam (`math::psram_backend`) keeps the whole stack host-testable (malloc shim); fixed-size regions bound PSRAM use at 960 KB worst case against 8 MB; the vector lift reuses the engine's compile-once path instead of a second expression grammar; textual reduction substitution keeps tinyexpr untouched.
**Tradeoffs**: Reductions only accept bare list names (`sum(cumsum(l1))` doesn't parse — store the inner result first); list literals can't appear inside element-wise arithmetic (`2*{1,2}` is an error; use a stored list); one 80 KB region per large list even when barely over 256 elements; wrapper nesting capped at 2.
**Revisit when**: 3B stats/regression needs richer expressions (consider promoting list_expr to a real tagged-value evaluator), the complex dtype lands (accessor internals + `lists.dat` tag), or Phase 4 matrices need >80 KB (region size is a constant).

## D21: Phase 3 Array — 999 cap, SRAM-only, double elements with a dtype tag

**Date**: 2026-07-18
**Status**: Accepted (resolves phase3-spec P3-1 and P3-2 ahead of task 3A.1)
**Context**: Phase 3 3A needs the `Array` primitive's shape decided up front. The spec suggested a 10000 cap "given PSRAM", but that predates D10's quarantine of bulk PSRAM transfer (only word r/w is verified), and the complex-numbers wishlist (2026-07-18) raised the element-type stakes for Phase 4's Matrix reconciliation.
**Decision**: (1) **P3-1: max list length 999** (TI parity). (2) **Storage: SRAM-only for Phase 3** — six full lists are ~48 KB, inside even the Pico 1's ~195 KB headroom; `ArrayStore` keeps the backing abstract so PSRAM can be added later without caller churn. (3) **P3-2: elements are `calc_t` (double) only, but `Array` and the `lists.dat` image carry a dtype tag.** The tag is not speculative: **complex-valued lists/matrices are a committed direction** (developer, 2026-07-18) — they land with the unscheduled complex-numbers feature, and the dtype tag + persistence format must make that a non-breaking addition.

**Amended same day (post-D10 fix, developer call): cap raised to 10000 with the PSRAM tier enabled.** The D10 bulk-PSRAM root-cause fix landed hours after this decision (see D10 addendum: ~6.8 MB/s verified), removing the constraint that motivated SRAM-only. Storage now follows the spec's §2.2 design as written: small arrays (<= 256 elements) in the SRAM pool, larger in PSRAM via the chunked bulk path — a full 10000-element list is 80 KB, ~12 ms to stream, six lists 480 KB against 8 MB. Arrays and Phase 4 matrices both use PSRAM. Non-blocking caveat: PSRAM is late on cold boot (D14, unresolved) — lists just aren't loadable until late-init brings it up, which is acceptable since nothing needs PSRAM at boot.
**Rationale**: Zero coupling to D10 — 3A cannot be blocked by a hardware session; fastest stat sweeps (no per-element SPI); TI-parity cap covers the realistic on-device datasets; the dtype tag future-proofs persistence and the Phase 4 `Matrix`-on-`Array` reconciliation for a few bytes now.
**Tradeoffs**: No >999-element lists in Phase 3; ~48 KB of SRAM reserved at full occupancy (acceptable on both boards). Integer lists' space savings forgone.
**Revisit when**: The D10 review un-quarantines bulk PSRAM (raise the cap behind ArrayStore), Phase 4 matrices outgrow SRAM, or the complex-numbers feature is scheduled (the dtype tag then goes live — planned, not conditional; note a full 999-element complex list is ~16 KB, so six lists ~96 KB still fit SRAM, but Phase 4 complex matrices likely force the PSRAM tier).

## D20: Global F-key scheme + typed command layer

**Date**: 2026-07-18
**Status**: Accepted (supersedes the D16 key bindings F4/F9 and the boot-era F6 diag toggle)
**Context**: Test-drive feedback item 7 (inconsistent F-keys across screens, WINDOW unreachable from the graph) plus the wish to demote the diag screen from the prime F6 slot. Settled in a design quiz with the developer.
**Decision**: TI-84-shaped global scheme on every screen: **F1** mode-dependent editor (Y=/PAR/POL), **F2** WINDOW (table screen: table setup), **F3** MODE, **F4** TRACE (opens the graph tracing from other screens; toggles on the graph), **F5** GRAPH / graph↔table toggle (split: pane focus), **Alt+F5** split toggle (HW-verified: Alt reaches F-keys with its flag intact; Shift+F5 is eaten by the STM32). Graph zoom moves to **`-`/`=`** (`+` also zooms in); S/T presets unchanged. Screen-local row/field ops move to non-F keys: **ENTER** edit, **SPACE** toggle slot enable, **DEL** clear (editor rows, WINDOW/setup fields — edit-from-empty; ASK-table row delete). Rarely-used surfaces become **typed commands** on the home screen (lowercase, matched before evaluation): `help`, `diag`, `files`, `cls`, `clrhist`; a grey right-aligned "type help" hint sits on the empty input line. F6-F9 are freed/reserved.
**Rationale**: One muscle-memory map across screens, matching the TI-84 F-row (Y=|WINDOW|·|TRACE|GRAPH); WINDOW finally reachable from the graph; diag demoted without losing access.
**Tradeoffs**: Commands (help included) only work from the home screen; diag lost its toggle-from-anywhere (serial late-init lines cover the cold-boot check). FILES moved out of the diag screen. The old F2-to-Step table shortcut was dropped as low-value.
**Revisit when**: KIV — F3 might become ZOOM (its TI slot, e.g. a preset menu) with MODE moving elsewhere; judge after real use.

## D19: Expression input is case-sensitive

**Date**: 2026-07-18
**Status**: Accepted (refines D11's wording)
**Context**: The typed-command layer (D20) wants exact lowercase matches; the engine blanket-lowercased every expression before tinyexpr, making input case-insensitive. Developer preference: case-sensitive.
**Decision**: Remove the lowercase folds (preprocess + store-op target). Identifiers — functions, variables a-z, `theta`, `ans`, commands — are lowercase only; uppercase input fails with a parse error, and `expr->A` gets a pointed "Variables are lowercase a-z" error. Numeric literals are unaffected (tinyexpr parses via strtod: `1E10` == `1e10`). `e` remains Euler's constant and not a variable; uppercase `E` is now just an unknown identifier.
**Rationale**: Exact matching is simpler to reason about, matches the lowercase-canonical engine internals that already existed, and the STM32 keyboard types lowercase by default so the day-to-day feel is unchanged.
**Tradeoffs**: Previously-persisted uppercase expressions (if any) now fail compile — editor rows render red and need retyping. Caps-lock typing errors instead of silently working.
**Revisit when**: A future need for uppercase identifiers (e.g. distinct A-Z variable bank, TI-style).

## D18: Defer the Pico 1 Phase 2 verification pass to post-Phase 3

**Date**: 2026-07-18
**Status**: Resolved 2026-07-22 (task 3D.14)
**Context**: The Phase 2 test drive (2.24) passed on the Pico 2; the Pico 1 pass was
deferred from Session 8. Both Picos share one PicoCalc mainboard, so a Pico 1 pass
costs a tedious physical module swap. Question: run it before Phase 3, or fold it
into Phase 3's own both-boards pass (task 3D.14)?
**Decision**: Defer. One combined Pico 1 pass after Phase 3 covers the Phase 2 sweep
(split-pane clipping headline, Session 8 fix list) plus Phase 3 acceptance — one
board swap instead of two.
**Rationale**: The board-conditional surface is four files, and the Pico 2 "full
framebuffer" mode is the strip path with one buffer-sized strip — the pane/strip clip
intersection (`set_pixel`/`fill_rect`) is shared code already exercised on the Pico 2,
so the D16 bleed worry is largely covered. RAM is a non-issue: the RP2040 build uses
62.5 KB static of 264 KB. The genuinely Pico-1-only risks — render non-idempotency
across ~20 strip passes per band, and perf feel (strip mode re-renders the scene per
strip; split full-redraws both panes) — produce localized visual/tuning bugs, not
architectural rework.
**Tradeoffs**: Bugs surface farther from their commit (mitigated: rebuild any tagged
firmware, e.g. 079a8b2, to bisect phase-2 vs phase-3 fallout in one flash). If Phase 2
introduced a non-idempotent render pattern, Phase 3 may copy it before hardware
catches it — mitigated by the strip-safety rule added to `phase3-spec.md` §8: new
screen `render()`s must be idempotent (may run ~20x/frame in strip mode).
**Revisit when**: Phase 3 grows new rendering machinery beyond ordinary screens
(animations, new split layouts) — then swap boards *before* that work starts; or any
host-side strip-mode regression harness appears, which would shrink the deferred risk
further.
**Resolution (2026-07-22)**: Task 3D.14 ran — the Pico 1 was reflashed to current
HEAD (Session 19 font/glyph build) and put through the full Phase 2 sweep, the
Session 8+9 fix list, and Phase 3 acceptance. All passed. Two non-blocking findings
carried to backlog, not yet investigated: `!` (factorial) throws a syntax error on
this board (pre-Phase-3 feature, not a regression — possibly a physical-keyboard
mapping quirk specific to this unit), and the list editor / a 5000-point scatter
plot both feel sluggish (not profiled). See `phase3-retro.md` and
`session3D14-pico1-observations-verbatim.md`.

## D17: Licensing — MIT own code, GPL-2.0 combined firmware

**Date**: 2026-07-12
**Status**: Accepted
**Context**: Open-sourcing the project. The firmware links GPL-2.0 vendored drivers (Coyote OS lcdspi/i2ckbd/pwm_sound + font1 bitmap font compiled into gfx::main_font); everything else (FatFs, tinyexpr, rp2040-psram, Pico SDK) is permissive.
**Decision**: Hybrid ("Option 3"): the project's own code is MIT (root LICENSE); the combined firmware binary is distributed under GPL-2.0 (treated as v2-only — Coyote files carry no per-file headers or "or later" language). NOTICE.md carries the component table and the scoped path to a fully permissive release — **kept as a future option, deliberately not on any roadmap**.
**Rationale**: Ships today with zero engineering work; the reusable subsystems (math/graph/render) stay permissively reusable; the door to full-MIT stays open since the GPL surface sits behind the platform/ HAL by design.
**Tradeoffs**: Repo carries two licenses (must stay clearly documented). GPLv2-only firmware means no code may ever be ported from GPLv3 projects (e.g. DB48X, the Phase 4 CAS reference — design reference only). Apache-2.0 was ruled out for the MIT side (incompatible with GPLv2-only).
**Revisit when**: Phase 4 wants GPLv3-licensed code, or the D9 font swap happens anyway (first step of the permissive path), or Coyote upstream clarifies/relicenses.

## D16: Split-screen — horizontal, singleton reuse, nearest-row sync, F4/F9 keys

**Date**: 2026-07-12
**Status**: Accepted (developer decisions, session 7)
**Context**: Task 2.19 needed the P2-1 orientation call plus four implementation choices.
**Decision**: (1) **Horizontal** split (graph top / table bottom) — the graph keeps its full 320px width, so the column caches and trace x-mapping are untouched; only viewport height changes. (2) **Reuse the live GraphScreen/TableScreen singletons** with runtime pane geometry (`set_pane`) — the spec's embedded-instances sketch would fork ~14 KB of caches and trace state. (3) Trace↔table sync is **nearest-row** (option c); "trace steps by table-step in split" (option b) is KIV pending the test drive. (4) **All three modes** supported from the start. (5) Keys: **F4 switches graph↔table** (full-screen: push/pop; split: pane focus), **F9 (Shift+F4) toggles split**, ESC exits.
**Rationale**: Horizontal turns 2.19 into a viewport-height change instead of a cache re-architecture; singleton reuse makes trace sync nearly free; nearest-row keeps round table values while tracing.
**Tradeoffs**: Graph pane is short (138px). Sync is approximate in function mode (pixel-grid x vs table grid). Panes render full logic clipped — ~1.5x frame cost per spec §12.
**Revisit when**: Test drive verdict on sync feel (upgrade to option b) or pane sizes.

## D15: One SlotEditorScreen base, thin per-mode editors

**Date**: 2026-07-12
**Status**: Accepted
**Context**: Task 2.5 needs a parametric editor; polar (2.9) follows. Options: generalize the HW-verified Y= editor into one mode-aware screen (spec §2's implied design), duplicate it per mode, or extract a shared base.
**Decision**: Extract `apps::SlotEditorScreen` (selection, InputLine edit lifecycle, dirty-band row invalidation, key dispatch, render loop) with per-mode virtuals (labels/text/toggle/clear/checkbox, `after_commit`). Y=, parametric, and later polar are thin subclasses. Done in two commits: pure extraction (no behavior change), then the parametric subclass.
**Rationale**: Three variants meet the rule of three. The D13 dirty-band footgun (a missed `invalidate()` = stale rows) lives in exactly one file instead of three; parametric keeps its natural pair-field model (2 fields/slot, checkbox on the X row) without if-mode branches in shared code.
**Tradeoffs**: Virtual-call indirection in the editor (irrelevant: keypress-rate). The extraction touched a HW-verified screen — mitigated by the mechanical two-step and a queued HW spot-check.
**Revisit when**: A mode editor needs a fundamentally different layout (e.g. scrolling lists for more slots) that the base's fixed row model can't express.

## D0: Track decisions in this file

**Date**: TBD (Phase 0)
**Status**: Accepted
**Context**: Solo project, but decisions made early (e.g., expression engine choice, HAL layering) need to be traceable later when their consequences surface.
**Decision**: Maintain `docs/notes/decisions.md` as a chronological log. Append entries as decisions are made, not retrospectively.
**Rationale**: Future-me will not remember why the math engine uses `double` instead of `float`, or why the framebuffer lives in PSRAM on Pico 1. A log is cheaper than re-deriving.
**Tradeoffs**: Minor maintenance overhead. Mitigated by keeping entries short (~10 lines).
**Revisit when**: Project becomes multi-developer and an ADR (Architecture Decision Record) format with separate files might be preferable.

---

## D14: Deferred PSRAM/SD late-init for the RP2350 cold-boot rail settle

**Date**: 2026-07-11
**Status**: Accepted
**Context**: First Pico 2 (RP2350) bring-up: display, keyboard, and battery came up fine, but PSRAM and SD both failed on **cold power-on** and both worked on every warm reboot. Instrumented cold-boot traces (buffered `dbg_log`, dumped over USB serial) measured the failure directly: at 0.5 s the PSRAM reads back zeros; at 0.6-2.5 s it returns almost-correct data (single bit errors, or the whole word shifted one bit — analog-marginal behavior, independent of SPI clock: same at 75 MHz and 18.75 MHz); at 7.5 s it is perfect. The SD card answers CMD0/CMD8 immediately (comms fine, R7 voltage echo clean) but never completes ACMD41 — its power-sensitive init — until, at 7.5 s, it inits instantly. Conclusion: the peripheral rail needs **~5-8 s to settle after cold power-on with the Pico 2 module**; the Pico 1 module doesn't exhibit this (Phase 1 was fully verified on it, same mainboard, same card). Community reports match (RP2350 PSRAM cold-boot failures; fuzix SD failure on PicoCalc Pico 2).
**Decision**: Do not block boot. Boot-time init runs as always (instant home screen); if PSRAM or SD failed, the main loop retries every 2 s for the first 30 s of uptime: PSRAM via `Psram::reinit()` (re-sends the chip reset through the already-configured PIO — deliberately not `psram_spi_init()`, which re-adds the PIO program and claims 2 DMA channels per call), SD via a fresh `Storage::init()` (f_mount re-runs `disk_initialize`). When storage arrives late, the self-tests re-run and history/variables/graph state load then; the current screen is fully invalidated so the UI reflects it.
**Rationale**: A calculator that boots in 0.3 s shouldn't stall 8 s on one board variant. Warm reboots and Pico 1 hit the success path at boot and never enter the retry loop.
**Tradeoffs**: During the first ~10 s of a Pico 2 cold boot, persistence isn't available yet and a failing SD attempt (card inserted, rail still low) blocks the loop up to ~1 s per retry — brief input lag if the user types immediately. History appears a few seconds after boot rather than instantly.
**Revisit when**: The rail settle is understood at the hardware level (measure 3V3 with a scope; possibly a PicoCalc mainboard/Pico 2 SMPS interaction), or a keyboard-firmware/mainboard revision changes the power path. If Phase 3/4 needs PSRAM immediately at boot, reconsider a short blocking wait with a splash.

## D13: Opt-in dirty-band partial redraw (rows, not rectangles)

**Date**: 2026-07-11
**Status**: Accepted; HW-verified same day (typing instant, no stale-row artifacts)
**Context**: With synchronous full-frame rendering (D10), every keypress cost ~200 ms — the SPI push dominates (recompute is only 15-17 ms), and push time is proportional to pixel count. Task 5.6 part 2.
**Decision**: Screens track a dirty **row band** (`[y0, y1)`, full width); `ScreenManager::render_frame()` consumes it and `Framebuffer::render_frame()` renders/pushes only the strips inside the band. Tracking is **opt-in** per screen (`track_dirty()` in the constructor + `invalidate()` on every state change in `on_key`); non-tracking screens keep full-frame redraws. Any screen surfacing to top of the stack is fully invalidated by the manager. Opted in: home screen (typing = input band, ~28 of 320 rows; Enter = everything above the softkeys, which also keeps the battery/mode status fresh) and the Y= editor (per-row bands). An empty band skips the render+push entirely, so unconsumed keys cost nothing.
**Rationale**: A y-band is enough — the hot regions (input line, editor rows) are full-width, so x-cropping would add a strided push path for no measurable win. Opt-in keeps the default safe: a screen that never calls `invalidate()` can't accidentally stop redrawing.
**Tradeoffs**: Tracking screens must invalidate every band their key handler touches — a missed call shows as stale rows on the panel (visible, not corrupting). The battery indicator refreshes only on Enter/screen changes, not per keystroke. Renderers still run their full draw code per band (clipping discards out-of-band work), so CPU cost is unchanged — fine while push time dominates.
**Revisit when**: Graph interactions need help (trace/zoom redraw the ~280-row plot area anyway, so bands don't win there — that wants a faster SPI clock, DMA, or plot-region caching); or a screen needs non-full-width updates.

## D12: Shell-style input recall on UP/DOWN; modifier+arrows scroll the view; HOME pops to root

**Date**: 2026-07-11 (revised same day after HW verification)
**Status**: Accepted; scroll modifier revised to Alt/Ctrl
**Context**: On hardware, UP recalled only the newest expression once, then further UP scrolled the output view — no way to walk back through older inputs. The HOME key did nothing visible.
**Decision**: Plain UP/DOWN walk backward/forward through past inputs (the in-progress line is stashed and restored); **Alt+UP/DOWN or Ctrl+UP/DOWN** scroll the history view. HOME pops to the home screen from any screen (global intercept in the main loop, like F6); on the home screen it falls through to the input line's cursor-to-start. *Revision:* Shift was the original scroll modifier, but HW verification (2026-07-11) showed the STM32 swallows Shift on arrow keys (it emits a shift-release then a plain arrow); Alt and Ctrl pass through with flags intact, so scroll moved to them. Shift is still accepted in case a future keyboard firmware reports it.
**Rationale**: Shell-style recall is the behavior every terminal user expects, and the keyboard has no PgUp/PgDn — shift is the only spare modifier and its state is already tracked in `KeyEvent`.
**Tradeoffs**: Editing a recalled entry then pressing UP discards the edit (bash-like, not zsh-like). View scrolling is now two-handed.
**Revisit when**: a keyboard firmware update reports arrows with shift held. Re-checked on fw v1.6 (2026-07-11): still swallowed — the kShift press arrives with no arrow event at all — so Alt/Ctrl scroll stands.

## D11: `e` is Euler's constant; variable E is reserved

**Date**: 2026-07-11
**Status**: Accepted (test-drive feedback)
**Context**: `e` evaluated to 0 on the device. `build_lookup()` bound all 26 letters as variables, and tinyexpr consults the user lookup before its builtin table — so the variable E shadowed the builtin Euler constant (`pi`, being two letters, never collided).
**Decision**: Do not bind the letter `e` as a variable; `e` reaches tinyexpr's builtin constant. Storing to E (`5->E`) returns "E is reserved (Euler's e)". Convention: single letters = variables, `pi`/`e`/`theta`/`ans` and function names = reserved words.
**Rationale**: A calculator where `e` isn't 2.718... fails the least-surprise test; TI users rarely store to E (on TI it's the exponent token anyway). Case sensitivity (e vs E) was rejected — the preprocessor lowercases everything and the win isn't worth reworking that.
**Tradeoffs**: 25 letter variables instead of 26. The E slot still exists in `Variables` storage (persisted file format unchanged).
**Revisit when**: Someone actually misses variable E.

## D10: Synchronous core-0 rendering; PSRAM bulk path and dual-core display deferred

**Date**: 2026-07-10
**Status**: Accepted (from first hardware bring-up)
**Context**: First flash to real PicoCalc (Pico 1) showed a screen of random colors and a dead keyboard. Bisecting on hardware (vendored-only diagnostic + USB-serial boot tracing) found three distinct bugs:
1. **Boot hang** — `run_self_tests()` called the vendored *bulk* PSRAM transfer (`psram_read`/`psram_write`, 1 KB), which hangs on this hardware, even though single-word `psram_read32`/`write32` work. Boot froze after display init but before the first draw, so the panel showed power-on noise ("random colors").
2. **Dual-core display stall** — routing strip pushes through a core-1 service over the multicore FIFO stalled on the first frame.
3. **Dead keyboard** — the I2C read/write timeouts (2 ms) were shorter than a 2-byte transfer on the 10 kHz keyboard bus (~3.5 ms), so every read timed out.
**Decision**:
- Render **synchronously on core 0** using the vendored blocking `spi_write_fast` path (proven good by the diagnostic). Core 1 is left idle; `display_service_main` and `push_rect_dma` are retained but unused, as the basis for a future revisit.
- Quarantine the **bulk PSRAM** API (`Psram::read`/`write`) as known-hanging; expose and use only single-word `read_word`/`write_word`. Phase 1 needs no bulk PSRAM (framebuffer is line-buffered in SRAM).
- Set the keyboard I2C timeout to 100 ms (`kI2cTimeoutUs`), comfortably above the 10 kHz transfer time.
- Rendering is **event-driven**: a full-frame push is ~200 ms (5 fps), so redraw only after a key press, not every loop.
**Rationale**: Get a correct, working calculator on hardware first. The DMA push, dual-core split, and bulk PSRAM are all optimizations/future-phase needs, not Phase 1 requirements; each is a separate investigation.
**Tradeoffs**: ~200 ms full-screen redraw latency per keypress (single-threaded, full-frame). Acceptable for a calculator; the fix is dirty-rectangle / partial updates (and possibly a faster SPI clock or revisiting DMA), tracked for task 5.6.
**Revisit when**: task 5.6 performance work — profile, then add partial updates and re-evaluate DMA/dual-core (the bulk PSRAM leg is resolved).

**Dual-core display leg ROOT-CAUSED AND FIXED 2026-07-25 (HW-verified on the Pico 1).** The D10 "core-1 FIFO stall on frame 1" was never the FIFO handshake and never DMA — both are healthy in isolation (a raw FIFO echo round-trips in us with USB connected; `push_rect_dma` renders correctly on core 0, warm and cold boot, ~160 ms/frame). The real fault: core 1 executing the display push path **from flash (XIP)** hard-faults (whole chip wedges, USB drops) when core 0's tinyusb/`stdio_usb` stack is active — the shared XIP cache can't serve both. Necessary conditions are *both* core-1 display access *and* USB plugged (the crashing firmware boots fine with USB unplugged). The original bring-up misattributed this to the FIFO because the dead core-1 service happened to call the DMA push, and a hung push never acks, so core 0 blocked on `drain_acks()` forever — indistinguishable from a FIFO stall. **Fix**: mark the entire core-1 display call path `__not_in_flash_func` (RAM-resident) — `Display::push_rect_dma`/`dma_push`/`dma_wait`/`convert_565_666` (`display.cpp`) and `define_region_spi`/`hw_send_spi`/`lcd_spi_raise_cs`/`lcd_spi_lower_cs` (`lcdspi.c`), extending the pattern the vendored driver already applied to `spi_write_fast`/`spi_finish`. Verified: core 1 runs `push_rect_dma` + acks reliably with USB connected (11+ consecutive pushes, ~2 ms/band, heartbeats intact, color-cycling test band renders correctly). The RAM-residency fix landed first as its own commit (dormant — nothing launched core 1 yet); the production dual-core display pipeline followed immediately (core 0 renders the next dirty strip while core 1 DMAs the current one — `gfx::display_service_main` + a two-buffer pipeline in strip-mode `render_frame`, launched by `start_display_service()` at boot, Pico 1 only; Pico 2's full-framebuffer push stays synchronous, untested-board caution). **Measured on the Pico 1** (A/B vs the pre-session synchronous blocking path, full-frame redraw): light render 173.6→146.5 ms (−16%), medium render 190.3→147.8 ms (−22%), status band 15.4→13.5 ms (−12%). Pipeline frame time is flat ~146-148 ms regardless of render load (compute hides under the push), so the win grows with screen complexity; push floor ~146 ms is the SPI wire time. Compute-bound screens (render > ~146 ms) get ~0 benefit — those want the secondary candidate (parallelize `recompute_function` onto a second engine), still open.

**Leg A (extend the pipeline to Pico 2) DONE + HW-VERIFIED 2026-08-02 on the RP2350.** `start_display_service()` now launches the core-1 service on both boards (was gated Pico-1-only via `if constexpr (!config::kUseFullFramebuffer)`; the `service_running` latch still prevents a double launch). The Pico 2 full-framebuffer path in `render_frame()` hands its band push to core 1 asynchronously through the existing `submit`/`drain_acks` machinery instead of blocking core 0 with a synchronous `push_rect` — since it's a single `frame_buf`, each frame calls `drain_acks()` first to wait for the previous frame's push before reusing the buffer; a synchronous `push_rect` fallback remains for the pre-service boot window (`service_running == false`). No new static state. The original risk this leg carried over from the Pico 1 fix — core 1 hard-faulting from XIP while core 0's USB stack is active (chip wedge, USB drop) — had never been exercised on the RP2350; flashing confirmed sustained boot with USB enumerated throughout, steady core-0 heartbeats, and `graph recompute:`-triggered core-1 pushes with no wedge/fault/USB-drop. Developer follow-up interactive pass (rapid screen navigation, fast typing, graph pan/zoom under key-repeat) came back clean: no tearing, no corruption, no freeze. Both `build/pico` and `build/pico2` build clean; full host suite green (the multicore TU isn't in the host build, so host behavior is unchanged). **Leg B (compute-parallelize `recompute_function` onto a second engine/vars context) remains the one open D10 follow-up** — still needed for compute-bound screens (render > ~146 ms push budget) where the async push offload gives ~0 benefit.

**Bulk-PSRAM leg RESOLVED 2026-07-18 (Session 10 round 3, HW-verified on the Pico 2).** Root cause: the PIO program takes 8-bit transfer counts (`out x, 8` / `out y, 8`), so one transaction maxes at 255 bits (31 bytes); the vendored `psram_write()`/`psram_read()` let the count byte wrap above 27/31 data bytes — `(4+count)*8 mod 256` — desyncing the PIO from the DMA byte stream (a wrapped count of 0 underflows `jmp x--` into a ~2^32-bit shift loop), wedging the blocking DMA wait forever. That was the Phase 1 boot hang; the upstream driver's own 4-byte-and-under fast paths never hit it. Fix: `Psram::read`/`write` now chunk internally (27-byte writes / 31-byte reads — also keeps CS-low under the chip's ~8 µs tCEM). Un-quarantined; guarded by a watchdog-armed boot self-test (hang → 2 s reboot → scratch-marker skip, no boot-loop) covering cap-straddling sizes, unaligned starts, and cross-chunk addressing. Measured on HW: 1 KB write 150 µs / read 156 µs (~6.8 MB/s). Dual-core display service remains deferred as before.

## D3: Trace coordinate readout at the bottom of the viewport

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision — show the trace `(x, y)` at the top or bottom of the graph? Top risks overlapping the plotted curves near the peak; bottom risks the softkey bar.
**Decision**: Bottom of the viewport, in a dark strip just above the softkey bar, matching the TI-84.
**Rationale**: Curves cluster around the top/middle more often than the very bottom edge; TI users expect it there.
**Tradeoffs**: A curve that dips to the bottom edge is briefly obscured by the readout. Acceptable.
**Revisit when**: A cleaner overlay (semi-transparent, or auto-repositioning away from the cursor) is worth the code.

## D5: Keep `double` for graph evaluation (float deferred)

**Date**: 2026-07-08
**Status**: Deferred (revisit after hardware profiling)
**Context**: Open decision — use `float` instead of `double` for graph point evaluation on Pico 1 (no FPU) to roughly halve softfloat cost?
**Decision**: Keep `double` (`math::calc_t`) everywhere for now, including the graph sweep. The compile-once/eval-many path already removes the dominant cost (re-parsing per point), so evaluation is 320 `te_eval`s per function, not 320 compiles.
**Rationale**: Correctness first; can't profile without hardware. `calc_t` is a single typedef, so a `float` graph-eval variant is a localized change if profiling shows plotting is too slow.
**Tradeoffs**: Softfloat `double` is ~2x slower than `float` on RP2040; may matter with 7 functions. Measured lever, not a guess.
**Revisit when**: Task 5.6 profiling on real Pico 1 hardware shows graph render missing the <50 ms target.

## D2: Fractions stack only for "simple" operands

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision — should `a/b` always render as a stacked fraction, or only when the operands are simple? Always-stacked is less code; a heuristic reads better for messy expressions.
**Decision**: Stack `a/b` into a `FractionNode` only when both sides are "simple" — a number, a variable, a parenthesized group, an already-built fraction, **a function call, or a power** (the last two added 2026-07-11). Otherwise render inline with a text `/`. Also require the division to be the first operator in its term (no chaining an inline `*` into a stacked fraction), so `a*b/c` stays inline.
**Rationale**: Matches the spec's section 6.2 guidance and TI behavior; `(x+1)/(x-1)` stacks (operands are parens) while `1+2/3+4` keeps `2/3` inline-sized within the sum. Keeps trees shallow and predictable.
**Tradeoffs**: A few expressions a user might expect stacked stay inline; acceptable and consistent.
**Revisit when**: User feedback, or when an equation editor needs full 2D editing (Phase 2+).
**Revision (2026-07-11)**: HW test drive hit the tradeoff — `1/sqrt(2)` rendered inline because a function call parses to an HBox. Calls (recognized structurally: `HBox[alpha-name, paren]`, which excludes unary-minus HBoxes) and superscripts now count as simple, so `1/sqrt(2)` and `x^2/2` stack.

## D1: Variable store operator is `->` (arrow)

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision from the spec — how to store a value into A-Z/theta. Options: TI `→`, `=`, `:=`, or a dedicated STO key.
**Decision**: Use ASCII `->` typed as two chars (e.g. `2->A`, `x^2->B`). The engine splits on the last `->` whose right side is a bare variable name; `=` stays free for future comparison/equation use.
**Rationale**: No special key mapping or font glyph needed now; reads clearly; avoids the `=` ambiguity the spec flagged. A dedicated STO key can emit `->` later without changing the engine.
**Tradeoffs**: Two keystrokes vs. one; `->` can't appear elsewhere in an expression (fine — it has no other meaning).
**Revisit when**: A physical STO/→ key is added, or equation solving needs `=`.

## D4: History persisted as plaintext TSV

**Date**: 2026-07-08
**Status**: Resolved 2026-08-03 (history-persistence bugfix session)
**Context**: Open decision — history storage format: plaintext vs binary.
**Decision**: Append `expr\tresult\n` lines to `/picocalc/history.txt`. On boot, read the last 8 KB and parse backwards into the ring buffer. Variables persist separately as a binary blob (`variables.dat`, 28 doubles).
**Rationale**: Plaintext history is debuggable and hand-editable; parsing cost is trivial at 50 entries. Variables are fixed-size binary because they're not meant to be edited and round-trip exactly.
**Tradeoffs**: History file grows unbounded (append-only) — a compaction pass is a future cleanup; 8 KB tail read caps what's loaded regardless.
**Revisit when**: History file size becomes a concern, or results need structured metadata.
**Resolution (2026-08-03)**: Both revisit triggers fired the same session. A Phase 5 regression (symbolic CAS results losing their `ResultKind` on reload — always came back `kPlain`) led to auditing the whole load/save path, which also turned up two latent pre-Phase-5 bugs: `load_state` read from the file *head* via `Storage::read_file`, not the tail its own comment claimed, so once `history.txt` outgrew the 8 KB window a reboot restored the oldest entries, not the newest; and the promised compaction pass had never been written, so the file really did grow unbounded. Fixed all three in one pass: a third tab-separated column (`expr\tresult\tS|P\n`, backward compatible) carries the kind; a new `Storage::file_size()` lets `load_state` seek to the true tail; and `HomeScreen::compact_history()` rewrites the file down to its last 8 KB once it exceeds 24576 bytes. See worklog's 2026-08-03 entry for full detail. The plaintext-TSV format itself is unchanged and still the right call — only the read offset, growth bound, and column count needed fixing.

## D6: RGB565 framebuffers, RGB666 on the wire

**Date**: 2026-07-08
**Status**: Accepted
**Context**: The Coyote OS panel init programs COLMOD 0x66 (18-bit color, 3 bytes/pixel over SPI) — the ILI9488-family serial interface does not accept RGB565. The spec assumed RGB565 end-to-end.
**Decision**: Keep all render buffers RGB565 (as spec'd); convert to 3-byte RGB666 in `platform::Display` during push, using 5-to-8/6-to-8 bit LUTs, chunked through two 4-scanline staging buffers so conversion overlaps DMA.
**Rationale**: Preserves the spec's memory budget (2 B/px buffers) and the proven panel init. Conversion is a few cycles/pixel on core 1, which is otherwise idle waiting on SPI.
**Tradeoffs**: 50% more SPI traffic than true 565 (~98 ms/full frame @ 25 MHz — partial updates and/or a higher SPI clock are the perf levers; see worklog).
**Revisit when**: Profiling (task 5.6) shows the panel accepts COLMOD 0x55 at speed, or SPI overclocking changes the math.

## D7: Non-blocking keyboard poll state machine

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Vendored `read_i2c_kbd()` sleeps 16 ms between the FIFO register select and the data read — unacceptable in a per-frame poll loop.
**Decision**: `platform::Keyboard::poll()` reimplements the same I2C protocol (reg 0x09, addr 0x1F) as a two-phase non-blocking state machine (select, then read at least 10 ms later). The vendored driver still provides bus init and scan-code reference.
**Rationale**: Keeps the main loop responsive; drivers stay unmodified.
**Tradeoffs**: Two places know the STM32 protocol (vendored driver + wrapper).
**Revisit when**: STM32 firmware changes its register map, or an interrupt-driven design is needed.

## D8: FatFs local config — LFN enabled, CP437

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Spec filenames (`variables.dat`) exceed 8.3; the FatFs default config has LFN off and Shift-JIS codepage tables (large flash cost).
**Decision**: `ffconf.h`: `FF_USE_LFN=1` (static buffer), `FF_CODE_PAGE=437`. Documented as a local modification (ffconf.h is FatFs's designated user-config file).
**Rationale**: Smallest change that supports the spec's file layout.
**Tradeoffs**: LFN=1 is not thread-safe — fine, all file I/O happens on core 0.
**Revisit when**: File I/O moves off core 0.

## D9: Interim 8x12 font (Coyote font1) instead of 8x16

**Date**: 2026-07-08
**Status**: Resolved 2026-07-18 (Session 10)
**Context**: Spec calls for an 8x16 font generated from a public-domain BDF; that conversion needs font tooling not yet in the repo.
**Decision**: Ship milestone 1 with the vendored Coyote OS `font1` (8x12, UTFT layout) behind `gfx::Font`, which reads any UTFT-format header. Generate proper 8x16 + 6x8 fonts before milestone 5.
**Rationale**: Unblocks all text rendering now; the Font abstraction makes the swap a data change.
**Tradeoffs**: Slightly smaller glyphs than designed; layout metrics tuned later.
**Resolution (2026-07-18)**: Swapped to **Spleen** (BSD-2-Clause, one family for both sizes): 8x16 main + **5x8 small** (`gfx::small_font()`, in place of the spec's 6x8 — Spleen has no 6x8 and 5x8 suits axis labels better). Tooling: `scripts/bdf_to_utft.py`; sources vendored in `drivers/spleen/`. This was also step 3 of the D17 permissive path — `font1` is no longer compiled in. Layout impact was a data change plus three row-height constants, as designed.

---

<!-- New decisions go above this line. Below: pre-Phase-0 decisions captured retrospectively from the spec & feasibility report. -->

## D-prelude-3: Use C++17 with the Pico SDK

**Status**: Accepted (pre-Phase-0)
**Decision**: C++17 in `src/`, plain C in vendored `drivers/`.
**Rationale**: C++17 gives `std::optional`, `std::variant`, `constexpr if`, structured bindings, and inline variables — all useful for the architecture. The Pico SDK is C with C++ wrappers; both languages mix cleanly. C++20 modules are not yet practical with the SDK.
**Tradeoffs**: Slightly larger binaries than C-only, but well within our 2 MB Pico 1 flash budget. Some templates and STL features are off-limits because they allocate; this is documented in `AGENTS.md`.

## D-prelude-2: Layered architecture with strict HAL discipline

**Status**: Accepted (pre-Phase-0)
**Decision**: `apps → ui → math/render → platform → drivers + Pico SDK`. Application code never calls Pico SDK functions directly.
**Rationale**: Dual-target support (Pico 1 + Pico 2) is the project's tightest constraint. Without HAL discipline, target-specific code metastasizes and the dual build becomes unmaintainable.
**Tradeoffs**: Some duplication in trivial wrappers. Worth it for testability and target portability.
**Revisit when**: Adding a third target (e.g., desktop simulator).

## D-prelude-1: Coyote OS as driver foundation

**Status**: Accepted (pre-Phase-0)
**Decision**: Vendor Coyote OS's C drivers (`lcdspi`, `i2ckbd`, `rp2040-psram`, `pwm_sound`) as read-only third-party code under `drivers/`.
**Rationale**: Coyote OS is the only known PicoCalc-native firmware with working drivers for our target hardware. Reimplementing them from scratch costs weeks and gains nothing.
**Tradeoffs**: We inherit any bugs in those drivers. Mitigated by wrapping them in `platform/` so fixes/workarounds happen at one layer.
**Revisit when**: A driver bug is unfixable from the wrapper layer.

## D-prelude-0: Pico SDK + CMake + Ninja

**Status**: Accepted (pre-Phase-0)
**Decision**: Use the official Raspberry Pi Pico SDK with CMake (Ninja generator). `CMAKE_GENERATOR=Ninja` is set in shell environment.
**Rationale**: Standard, well-supported, dual-target ready (`-DPICO_BOARD=pico` / `pico2`). Ninja is faster than Make for incremental builds and integrates better with clangd's `compile_commands.json`.
**Tradeoffs**: Requires Ninja installation. Negligible.
