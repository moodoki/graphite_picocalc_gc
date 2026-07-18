# Phase 2 retrospective

**Window:** 2026-07-12 → 2026-07-18 (three working sessions: 7, 8, 9).
**Outcome:** Phase 2 complete. The calculator now graphs in three modes
(function, parametric, polar) with a value table, split graph|table view with
trace↔row sync, unified persistence, built-in help, a TI-84-shaped global
keymap, and a typed command layer — all verified on Pico 2 hardware. The
Pico 1 verification pass is deliberately deferred to Phase 3 (D18).

## What shipped

- **`graph/` subsystem** (task 2.1 first, per plan): `Viewport` +
  `Plotter`/`PointSource` extracted from the Phase 1 graph screen,
  behavior-preserving, with the viewport formulas locked by host tests. Then
  `Mode`/`GraphState`, `FunctionSource`, `ParametricSource` (slot-indexed
  `eval_compiled` sweeps t), `PolarSource` (theta sweep, angle-mode aware),
  and the extracted mode-aware `TraceCursor`.
- **Editors** per D15: one `SlotEditorScreen` base; Y=/parametric/polar
  subclasses (pair auto-focus/auto-enable, red rows for non-compiling
  expressions). Mode-aware WINDOW (Tmin/Tmax/Tstep, THmin/THmax/THstep rows).
- **Tables** (2.12–2.18): host-testable `table_model` (mode-aware columns),
  `TableSetupScreen` (Start/Step/AUTO-ASK), `TableScreen` (auto infinite
  scroll, ASK accumulation, column scroll, cached visible window).
- **Split view** (2.19–2.21, D16): framebuffer pane clip rect; horizontal
  split reusing the live graph/table singletons with runtime pane geometry;
  nearest-row trace sync in all modes.
- **Unified persistence** (2.23): magic-tagged `graphstate.dat` binary image
  — mode, all three modes' slots, window, t/theta ranges, table config, and
  (after the Session 8 fix) the MODE-row math settings; one-time migration
  from the Phase 1 files.
- **Built-in help** (2.26–2.28): `math::catalog` as the single source for
  both the parser's `build_lookup` and the FUNC tab; `HelpScreen` with
  FUNC/KEYS/SYNTAX tabs.
- **Session 8–9 hardening from on-device use**: D14 late-init self-test
  retries + first boot-path serial instrumentation; real key-FIFO drain
  (`Keyboard::fifo_empty()`); battery API split (`battery_status()`
  cache-only, `battery_poll()` at a deliberate 5 s); charging-bit decode
  fixed and confirmed; `ScreenManager::switch_to` (stack-leak fix);
  expression eval in every numeric field (`math::eval_field`); sweep
  endpoint clamp so closed curves close.
- **UX layer from the usage rounds (D19/D20)**: case-sensitive input; global
  F-key scheme (F1 editor / F2 window / F3 mode / F4 trace / F5 graph↔table,
  Alt+F5 split, -/= zoom); typed commands (`help`, `diag`, `files`, `cls`,
  `clrhist`); DEL/SPACE row-and-field semantics; graph status bar + plot-row
  clipping; square ZStandard (y ±8.75); FILES screen.
- **Quality floor**: clang-tidy lint baseline clean and gating
  (`WarningsAsErrors: '*'`); host checks grew 112 → **210** (97→105 math,
  33 layout, +72 graph — the graph suite is new this phase).

Perf on hardware (2.25 baseline, Pico 2): recompute is nowhere near the
bottleneck — parametric pair ~1.0–1.4 ms, function set ~5 ms, polar
~2–5.3 ms, split ~4.1 ms vs the ~200 ms frame push. The one perceived perf
bug (table scroll lag/overrun) was event backlog, not compute — fixed in the
key-drain rework, so the planned compile-once-per-regenerate lever was never
needed.

## Timeline vs plan

The spec budgeted eight weeks (weeks 9–16). Wall-clock: seven calendar days,
three working sessions. Same two structural reasons as Phase 1 — the
host-testable core (all sources, the table model, and the viewport iterate at
laptop speed) and the HW-PENDING queue batching hardware acceptance into
developer-driven live sessions. New this phase: the **observation → quiz →
green-light rhythm** for design-heavy work (the keymap remap went from
feedback to HW-verified implementation inside one session because the design
was fully settled — including a hardware feasibility check — before any code).

## What went well

- **Refactor-first paid off**: doing 2.1 (extraction) before any new mode
  meant parametric and polar were each "one more PointSource," and split
  view reduced to a viewport-height change plus a clip rect (D16's singleton
  reuse). The 14 KB cache fork the spec sketched never happened.
- **D15's editor base class**: three editors for roughly the price of one;
  the red-row error feature and the DEL/SPACE remap were each single-site
  changes.
- **One persistence image with a magic tag**: adding MODE-row settings was a
  bump-the-magic one-liner with a predictable one-time reset, and migration
  from Phase 1 files just worked.
- **Catalog as single source of truth**: help FUNC tab and parser lookup
  cannot drift; adding functions is one table row.
- **Same-session fix loops on hardware** (Sessions 8 and 9): every bug found
  on-device was root-caused, fixed, flashed, and re-verified within the
  session. Nothing aged in a backlog.
- **Cheap serial instrumentation, added exactly when needed**: `late-init:`
  lines cracked the stuck-FAIL retry gap; the `battery: raw=` print settled
  the charging-bit layout in one capture; the diag key echo let us verify
  Alt+F5 viability *before* designing the keymap around it.

## What was hard (and the lessons)

- **A "fix" that doesn't change observable behavior can be a no-op.** The
  Session 8 key-drain fix broke on the first `kNone` — which the two-phase
  poll state machine returns mid-cycle — so it still consumed one event per
  frame, exactly the bug it claimed to fix. It passed build, lint, and code
  review; only the on-device re-check caught it. Lesson: for timing/queueing
  fixes, verification on hardware is part of the fix, and the API contract
  (`kNone` meant two different things) was the real bug.
- **Freshness budgets must be end-to-end.** The battery status bar polled a
  cache at 1 Hz — while the cache refreshed every 30 s underneath. The "~1 s"
  target was structurally unreachable, and the observed 2–3 s was a lucky
  phase. Lesson: trace a latency requirement through every layer; then set
  the cadence deliberately (5 s — STM32 stability outranks snappiness).
- **Sweep endpoints: `floor(range/step)` drops the partial last step.** The
  cardioid didn't close in degree mode (up to one full step short of THmax);
  the radian default had the same ~1.9° gap unnoticed. Fix: emit a final
  clamped sample at the range end. Lesson: closed curves are an endpoint
  test, not just a density test.
- **Push-toggles leak the screen stack.** Editor F4 pushed the graph even
  when the editor sat on top of it; at max depth every push silently no-oped
  ("F4 stops working"). `switch_to` (pop/replace) is the right primitive for
  toggle-style navigation.
- **The keymap you spec isn't the keymap you want.** Like Phase 1's test
  drive, real use produced the actual design: the D20 remap (TI-84-shaped,
  consistent across screens) replaced the organically-grown per-screen
  bindings, and the fix was cheap *because* it was designed in one sitting
  against a live inventory of every binding. Lesson: schedule the usage
  round; don't polish the spec's keymap first.
- **macOS + BOOTSEL is reliably unreliable.** The RP2350 volume stopped
  mounting (Session 8 → picotool), then picotool load hung for minutes
  (Session 9 → volume again), and the volume needs ~15 s to mount after
  reset. Both paths stay documented; assume neither.

## Decisions to revisit later

- D20 KIV: F3 as MODE vs ZOOM (its TI slot) — judge after real use.
- D16 KIV: trace-sync option b (trace steps by table-step in split).
- 340-point parametric/polar cache cap — revisit if fine steps matter.
- D10 leftovers: bulk PSRAM path (Phase 3's lists will want it), dual-core
  display service.
- D14: scope the 3V3 rail settle before Phase 3/4 leans on PSRAM at boot.
- D9 font upgrade and D5 float-eval lever: still parked, still unused.

## Carried into Phase 3

- **The Pico 1 combined pass (D18)** — Phase 2 sweep (headline: split-pane
  clipping on the strip renderer) + Session 8/9 fixes + Phase 3 acceptance,
  as task 3D.14. Until then the Pico 1 stays on Session 7 firmware.
- **Strip-safety rule** (phase3-spec §8): new `render()`s must be idempotent
  (~20×/frame in strip mode); no host coverage exists to catch violations.
- The `Array` primitive (phase3-spec §2) should be reconciled with Phase 4's
  `Matrix` per the spec's §10 note when Phase 4 begins.
- Backlog otherwise as listed in `next-session.md`.
