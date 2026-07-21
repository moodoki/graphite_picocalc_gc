# Phase 3 retrospective

**Window:** 2026-07-19 → 2026-07-22 (Sessions 11-15 for the code, plus the
deferred 3D.14 Pico 1 pass closing it out three days later).
**Outcome:** Phase 3 complete. The calculator now has typed lists (`l1`-`l6`)
with a full editor, one- and two-variable descriptive statistics and all ten
regression models, continuous/discrete probability distributions, a 15-kind
hypothesis-test/confidence-interval screen, and a StatPlot layer (scatter,
xy-line, histogram, modified box plot, normal probability plot) — all
verified on both boards. The Pico 1 verification pass, deferred since Phase
2 (D18), ran today and folded in the outstanding Phase 2 strip-render sweep
at the same time.

## What shipped

- **`Array` primitive + `ListStore`** (3A, D21/D22): dtype-tagged elements
  with an SRAM/PSRAM tiered backing store (999-element cap, later widened
  to 10000 with the PSRAM tier — D21 amendment), `l1`-`l6` with
  `lists.dat` persistence, list ops including an external merge sort for
  PSRAM-tier lists, and the home-screen `math::listexpr` layer (literals,
  `->lk` store, reductions, vector lift). List editor screen via the typed
  `lists` command.
- **Descriptive stats + regression** (3B, D23): 1-Var (incl.
  freq-weighted) and 2-Var stats with rank-selection quartiles (streaming
  binary search over the double bit space — no sort, no temp copy); all
  ten regression models, including Levenberg-Marquardt for the iterative
  logistic/sinusoidal fits and Tukey median-median; `format_model` stores
  a fit straight to a Y-slot for graphing. Typed `stats` command.
- **Probability distributions** (3C, D25): cephes `cprob` vendored for
  gamma/beta/normal primitives; continuous and discrete distribution
  wrappers with two-sided CDFs and real-df support; a guided `dist`
  screen.
- **Storage health** (D26, folded in from a Session 14 observation): the
  D14 30-second late-init window became an indefinite retry heartbeat for
  unhealthy SD/PSRAM; SD hot-plug (eject/insert) is now handled at
  runtime instead of only at boot.
- **Inference + StatPlots** (3D, D27): `math::infer` — z/t/proportion
  tests, chi-square GOF and independence, one-way ANOVA, a linear
  regression slope t-test, and the six confidence-interval families, all
  through one 15-kind `test` form screen. `stat_plot.{hpp,cpp}`: scatter,
  xy-line, histogram, modified box plot (1.5-IQR outliers), and a Blom
  normal probability plot, with a cache/draw split for strip-renderer
  safety; `Plot1`-`Plot3` config persists (PCG4, one-time reset);
  `Z` = ZoomStat.
- **Usability batch** (Session 13, D24): brace-literal broadcast fix,
  HOME-nav root replacement, list-editor negative-color fix, `range()`,
  bare-arg reductions, `?`/`list`/`stat` aliases, pi glyph.
- **Task 3D.14 — the combined Pico 1 pass (D18), closing Phase 3**: after
  three days on Session 7 firmware, the Pico 1 was reflashed to current
  HEAD (Session 19) and put through the full Phase 2 sweep (headline:
  split-pane clipping on the strip renderer), the Session 8+9 fix list,
  and the entire Phase 3 acceptance checklist above — all passed. See
  `session3D14-pico1-observations-verbatim.md`.
- **Quality floor**: host suite grew from 473 checks (post-3B) to **716**
  by the end of 3D (new `test_infer`, 91 checks); lint stayed clean
  throughout; both boards build (Pico 1 ended Phase 3 at ~305 KB text,
  ~147 KB/264 KB bss).

## Timeline vs plan

The spec budgeted weeks 17-25 (nine weeks). Wall-clock: four calendar days
for the code (Sessions 11-15, 2026-07-19/20) plus a three-day gap before the
Pico 1 board swap (task 3D.14, 2026-07-22) — the same host-testable-core +
HW-PENDING-batching pattern as Phases 1 and 2, and the fastest phase yet in
elapsed working time.

## What went well

- **Tiered storage designed once, reused across the phase**: the
  SRAM/PSRAM `Array` split (D21) built for lists in 3A needed no rework
  for stats' large-sample regressions or inference's list-backed tests —
  it was just "another Array consumer."
- **Rank-selection quartiles**: solving for quartiles via streaming binary
  search over the double bit space avoided a full sort or temp copy on
  every stats call, on both SRAM and PSRAM tiers, without special-casing.
- **The observation → fix → re-verify loop kept working past Phase 2**:
  Session 14's on-device batch (SD retry gap, Y=-editor overlap) went from
  logged observation to fixed, HW-verified code (D26) inside the next
  session, same as Phase 2's Sessions 8-9 rhythm.
- **Deferring the Pico 1 pass to one combined session (D18) paid off**:
  folding the Phase 2 strip-render sweep into 3D.14 meant one board swap
  instead of two, and the Pico-1-specific risks the decision flagged
  (render idempotency, split-pane clipping) both came back clean on the
  first pass.
- **Cache/draw split for StatPlots**: designing the strip-safety rule
  (phase3-spec §8) into the StatPlot layer from the start, rather than
  retrofitting it, meant the Pico 1 pass found zero strip-render bugs in
  the newest, most strip-sensitive screens.

## What was hard (and the lessons)

- **A phase can code-complete and still sit unverified on one board for
  days.** The Pico 1 stayed on Session 7 firmware through all of Phase 3's
  development — every 3A-3D feature was HW-verified on the Pico 2 only
  until 3D.14. The gap was a deliberate, decided tradeoff (D18), not
  neglect, but it means "Phase 3 code-complete" and "Phase 3 verified" were
  three days apart in practice. Lesson: say so explicitly in status
  language (as this project's docs already did) rather than letting
  "code-complete" read as "done."
- **A years-old basic feature can still surface a first-time bug on new
  hardware.** The 3D.14 pass found `!` (factorial) throwing a syntax error
  on the Pico 1 — a pre-Phase-3 feature, not a regression from this
  phase's work, and not yet root-caused (possibly a physical-keyboard
  mapping quirk specific to this unit, per the observations file). Lesson:
  a "new board" pass isn't just a regurgitation of the new phase's
  checklist — basic functionality deserves a spot-check too, because it's
  the first time *this hardware* has run *any* of the current build.
- **Perf feel doesn't automatically transfer across boards.** The list
  editor and a 5000-point scatter plot both felt sluggish on the Pico 1
  during 3D.14, despite the Pico 2 baseline (2.25) showing recompute was
  never the bottleneck. Not yet profiled — could be the strip renderer's
  per-band re-render cost, the RP2040's slower CPU, or both. Lesson: a
  perf baseline captured on one board doesn't stand in for the other;
  Pico 1 needs its own timing pass before assuming Phase 2's "recompute is
  not the bottleneck" conclusion still holds there.

## Decisions to revisit later

- **Factorial `!` syntax error on Pico 1** (found in 3D.14, not yet
  investigated) — check whether it's a parser regression or a
  physical-keyboard `!`-key mapping issue specific to this unit.
- **List editor and large-scatter-plot perf on Pico 1** (found in 3D.14) —
  not yet profiled; unclear if it's strip-renderer overhead, RP2040 clock
  speed, or something list/plot-specific.
- D17 licensing: display/keyboard rewrites still open (carried from
  earlier phases, untouched this phase).
- 340-point curve cache cap, audio HAL, dual-core display service: still
  parked (carried from Phase 2).
- Array/Matrix reconciliation (phase3-spec §10 note) — now actually due,
  since Phase 4A shipped `Matrix` independently while Phase 3 was still
  closing out its own Pico 1 pass.

## Carried into Phase 4

- Phase 4A-4C (matrices/solver, CALC menu, complex numbers) already
  shipped code-complete during the gap before 3D.14 and are themselves
  awaiting their own Pico 1 hardware pass — deliberately out of scope for
  3D.14 per this session's decision to keep the Pico 1 pass to Phase 2 +
  Phase 3 only.
- Backlog otherwise as listed in `next-session.md`.
