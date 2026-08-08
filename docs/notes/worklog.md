# Worklog

Running log of build sessions. Newest entries at the top of each section. This file is the
session-surviving source of truth for "where are we and what's next" — read it first when
resuming work. Task status checkboxes live in `docs/phases/phase1-plan.md` (and the Phase 0
checklist in `docs/phases/phase0-prep.md`); this file carries the narrative: what was done,
what was decided, what's blocked, and what to pick up next.

Conventions:

- One entry per work session or checkpoint, headed by date + summary.
- `HW-PENDING`: implemented and building, but acceptance requires real PicoCalc hardware
  that automated sessions don't have. These accumulate in the table below and get cleared
  manually by the developer.
- Decisions go to `docs/notes/decisions.md`; this log only references them.

---

## Current status

> ⚠️ **Frozen at the Phase 1/2 era — do not trust as current.** The live
> "where are we / what's next" handoff moved to
> [`next-session.md`](next-session.md); the dated entries below carry the
> running narrative (newest first). Kept here as a historical snapshot.

- **Phase**: 1 code complete; **Pico 1 fully HW-verified** (2026-07-11) and **Pico 2
  brought up** (2026-07-11/12): full-framebuffer display path works, cold-boot
  PSRAM/SD failure root-caused to a ~5-8 s peripheral rail settle and fixed with
  deferred late-init (D14, verified on a cold power-on). STM32 keyboard fw v1.6;
  battery, dirty-band rendering (D13), SD persistence, store op, pretty math (D2
  revised), trace/presets, mode, bootloader reboot all verified on Pico 1.
- **Open HW items (small)**: none on the Pico 2 — charging decode verified
  2026-07-18 (Session 9); the Session 8 test drive covered the functional
  spot-check. Remaining HW work is the deferred Pico 1 pass (D18).
- **Phase 1 declared done 2026-07-12** (retro: `docs/notes/phase1-retro.md`).
- **Phase 2 started 2026-07-12** (Session 7): lint baseline is clean and gating
  (clang-tidy installed + config fixed, `WarningsAsErrors: '*'`), and **task 2.1 is
  done** — `src/graph/` now holds `Viewport` + `Plotter`/`PointSource`, GraphScreen
  routes through them, behavior-preserving (viewport formulas locked by host test).
- **Tasks 2.2–2.4 also done** (same session): `graph::Mode` + `GraphState`
  (apps::graph_model delegates into `graph::state()`); `FunctionSource` feeds
  GraphScreen's recompute; `Engine::eval_compiled` got a slot-indexed overload
  and `ParametricSource` sweeps t (host-tested with a unit circle).
- **Tasks 2.5–2.7 done** (same session): editors per D15 (`SlotEditorScreen`
  base + Y=/parametric subclasses); mode-aware WindowScreen (Tmin/Tmax/Tstep
  rows); parametric plotting with a 340-point/pair pixel cache + mode-aware
  `graph::TraceCursor` (the §14 trace extraction). Graph F5 routes to the
  right editor per mode. **The whole parametric path is code-complete but
  unreachable until the mode selector (2.22) and unpersisted until 2.23.**
- **Minimal 2.22 pull also done**: MODE screen gained a "Graph mode" row
  (FUNC<->PARAM) so the parametric path is reachable on-device; polar joins the
  cycle with 2.8–2.11.
- **Built-in help done (2.26–2.28, pulled from week 16)**: `math::catalog`
  drives both `build_lookup` and the FUNC tab; `HelpScreen` (FUNC/KEYS/SYNTAX)
  on Home F5. Addresses the 07-11 test-drive discoverability pain (§10).
- **Polar week done (2.8–2.11)**: `PolarSource` (theta-slot sweep, angle-mode
  aware conversion), `PolarEditorScreen` (D15 subclass), THmin/THmax/THstep
  window rows, GraphScreen polar branch sharing the parametric point cache,
  MODE cycles FUNC/PARAM/POLAR. **All of weeks 11–13 is code-complete.**
- **2.23 done (pulled from week 16)**: unified `graphstate.dat` (magic-tagged
  binary image) persists everything — mode, all three modes' slots, window,
  t/theta ranges, table config; one-time migration from yfuncs.txt/window.dat;
  parametric/polar editors + mode row now save. **No unpersisted state left.**
- **Table view done (2.12–2.18)**: host-testable `table_model`
  (mode-aware columns + `evaluate_table_row`), `TableSetupScreen`
  (Start/Step/AUTO-ASK, persists via 2.23), `TableScreen` (auto infinite
  scroll, ask accumulation, horizontal column scroll, cached visible
  window). Entry: Graph F4 "TBL".
- **Split-screen done (2.19–2.21, D16)**: framebuffer pane clip rect;
  horizontal split reusing the live graph/table singletons with runtime
  pane geometry; nearest-row trace sync (all modes); F4 = pane focus,
  F9 = split toggle. Spec §13 open questions P2-1..P2-6 all resolved.
- **2.24 test drive DONE on Pico 2 (Session 8, 2026-07-17/18)**: all
  checklist steps pass; found-bug fixes + quick features implemented,
  built, linted, flashed same session (see Session 8 entry). 2.25 perf
  baseline captured (`docs/notes/testdrive-phase2-observations.md`).
- **Session 9 (2026-07-18)**: all ten Session 8 fixes verified on the
  Pico 2 (offline spin passed 8/10; items 8+9 re-root-caused, re-fixed,
  re-verified same session — see Session 9 entry). Charging-bit decode
  confirmed. Six-item usage-feedback batch shipped and HW-verified
  (D19 case-sensitive input, D20 keymap + typed commands, graph chrome,
  square ZStandard, DEL/SPACE). The full Pico 1 pass is **deferred to
  post-Phase 3** (D18) — folds into task 3D.14 to save a board swap.
- **Phase 2 declared done 2026-07-18** (retro:
  `docs/notes/phase2-retro.md`). Close-out: 2.24 closed on the Pico 2
  passes (Pico 1 leg → 3D.14 per D18); 2.25 closed on the baseline —
  recompute ≪ frame push, the scroll symptom was event backlog (fixed),
  compile-once lever not needed; 2.22 closed — MODE row + D20 made mode
  fully integrated (F3 everywhere, mode-aware F1/WINDOW/labels).
- **Session 10 (2026-07-18)**: pre-Phase-3 deferred-item batch, all four
  items code-complete, lint-clean, flashed to the Pico 2 (HW eval
  pending): **D9 done** — Spleen 8x16 main + 5x8 small font (BSD-2,
  `drivers/spleen/`, `scripts/bdf_to_utft.py` converter), Coyote `font1`
  no longer compiled (D17 step 3, NOTICE updated); **rand() seeded**
  (xorshift64* in math::fn, `get_rand_64()` at boot, host-deterministic);
  **ZoomFit** (`F` on graph); **numeric axis tick labels** in the small
  font (`L` toggles — evaluation feature).
- **Session 10 later rounds (same day)**: round-1 eval passed (labels
  kept → persisted, PCG3); **D10 bulk PSRAM root-caused, fixed,
  HW-verified** (~6.8 MB/s); **D21 amended — Array cap 10000 with the
  PSRAM tier**; D14 parked as the NEXT BENCH SESSION (non-blocking,
  scope plan + schematic findings in next-session.md).
- **Session 11 (2026-07-19): Phase 3 sub-phase 3A code-complete and
  flashed** — `Array` (dtype-tagged, SRAM slab / PSRAM region tiers per
  D21) + `ArrayStore`, `ListStore` l1-l6 with `lists.dat` persistence
  (+ `Storage::read_file_range`), list ops incl. external merge sort
  for PSRAM-tier lists, the `math::listexpr` home-screen layer
  (literals, `->lk` store, reductions, vector lift — see **D22**), and
  the list editor screen (typed `lists` command). 106 new host checks;
  lint clean; **Pico 2 flashed, boot verified over serial (HW eval
  pending — see HW-PENDING)**.
- **Session 12 (2026-07-19): Phase 3 sub-phase 3B code-complete and
  flashed** — `math::stats`: 1-var (incl. freq-weighted) / 2-var
  descriptive stats with **rank-selection quartiles** (streaming binary
  search over the double bit space — no sort, no temp copy), all ten
  regression models (polynomial via standardized normal equations,
  linearized ln/exp/pwr, **LM** for logistic/sinusoidal per **D23**
  which resolves P3-3, Tukey median-median via filtered selection),
  `format_model` → Y-slot store (3B.8, DEGREE-converted SinReg), and
  the **typed `stats` command** → form + results screen (3B.9). 122 new
  host checks (every model string engine-compile-checked); lint clean;
  **Pico 2 flashed, boot verified over serial (HW eval pending — see
  HW-PENDING)**.
- **Session 12 addendum: 3C.1 done** — cephes `cprob` subset vendored
  to `drivers/cephes/` (12 files; `incbet`/`incbi`/`igam`/`igami`/
  `ndtr`/`ndtri` + deps; integer-df wrappers deliberately skipped —
  `math::dist` will build on the real-df primitives). CMake `cephes`
  static lib with `gamma`/`erf`/`erfc` → `cephes_*` renames (no libm
  collisions), linked into the firmware; both boards compile it.
  `docs/dependencies.md` + `NOTICE.md` updated; provenance in
  `drivers/cephes/README.md` + `readme-netlib.txt`.
- **Next up**: on-device eval of the 3A+3B batches; then continue 3C
  (spec §5): `math::dist` wrappers (3C.2-3C.6) on the vendored
  primitives, P3-4 naming call at 3C.2, catalog registration (3C.7 —
  needs fp3/fp4 helpers, a `kMaxCatalogEntries` bump past 43, and a
  help FUNC-tab tweak: long signatures like `normal_cdf(lo,hi,mu,sd)`
  overflow the fixed 19-char summary column), `dist` helper screen
  (3C.8). Note the §8 strip-safety rule and the 3D.14 combined Pico 1
  pass.
- KIV: F-key layout rethink (feedback item 7) — Session 8 shipped the
  uncontroversial part (home F1 mode-dependent); F3/F4 consistency and
  WINDOW-from-graph still open, help KEYS must move with them.
- **Phase 3 declared done 2026-07-22** (retro: `docs/notes/phase3-retro.md`).
  Task 3D.14 — the deferred Pico 1 combined pass (D18) — ran today: reflashed
  to current HEAD (Session 19 build), the full Phase 2 sweep + Session 8/9 fix
  list + Phase 3 acceptance all passed. Two non-blocking findings carried to
  backlog, neither root-caused yet: `!` (factorial) throws a syntax error on
  this board, and the list editor / large scatter plots feel sluggish. D18
  resolved.
- **Both boards build**: yes (`./scripts/build-all.sh`). Diagnostic target: `picocalc_diag`.
- **Host tests**: `./scripts/host-tests.sh` → 136 math + 37 layout + 72 graph + 106 lists + 122 stats = 473 checks, 0 failures

### Hardware bring-up debugging kit (learned 2026-07-10)

- Flash without touching the board: from running firmware, `stty -f /dev/cu.usbmodem* 1200`
  triggers the RP2040 1200-baud reset into BOOTSEL, then `cp build/pico/*.uf2 /Volumes/RPI-RP2/`.
- USB serial: `cat /dev/cu.usbmodem*` (pico_enable_stdio_usb is on). printf boot-tracing
  was how the boot hang was located.
- `picocalc_diag` (src/diag_main.cpp) is a vendored-only display test — the bisection tool
  that proved the panel/driver work, isolating bugs to our code.

## HW-PENDING verification queue

> ⚠️ **Frozen at the Session 14 era — do not trust as current.** Later
> HW-verification status lives in [`next-session.md`](next-session.md) and the
> dated worklog entries below. Kept here as a historical snapshot.

Firmware now boots to the **home screen**; the diagnostics screen is the F6 overlay.

**Verified on Pico 1 hardware 2026-07-10:** display (home screen renders, text + colors
correct), keyboard (keys read with correct ASCII over I2C), PSRAM word r/w, backlight,
boot to a usable home screen. Bugs found & fixed: bulk-PSRAM boot hang, dual-core
display stall, keyboard I2C timeout — all in D10.

**Verified on Pico 1 hardware 2026-07-11 (round 1, Session 5):** evaluation + history,
input-history walk (UP/DOWN) and Alt/Ctrl+UP/DOWN view scroll (D12 revised), `e`
constant + E-reserved error (D11), HOME-pops-to-root, ESC exits diagnostics, graph
grid/palette colors, scientific-literal display (`1e10`), shifted F-keys F6-F9,
graphing with zoom, **graph recompute 15.3-17.1 ms for 2 functions (<50 ms target —
task 5.6 profiling done)**. Battery indicator: this unit's STM32 fw lacks the battery
register — shows "--" by design (see Session 5 entry).

**Verified on Pico 1 hardware 2026-07-11 (rounds 2-3, Session 6):** dirty-band
rendering (D13), battery % + cold-boot grace fix, SD card r/w self-test +
persistence, store op `2->A`, pretty math incl. the D2 revision (`1/sqrt(2)`,
`x^2/2` stack), trace + S/T presets, mode toggles, reboot-to-bootloader, full
exit test. **Pico 1 verification is complete.**

**Verified on Pico 2 hardware 2026-07-17/18 (Session 8 test drive):** everything in
the 2.24 checklist — function-mode parity post-refactors, Y= editor feel, home
basics, parametric acceptance (circle/Lissajous, editor auto-focus + pair
behavior), polar acceptance in both angle modes (cardioid/rose), tables (auto
infinite scroll, ASK add/delete/hint, setup + F2-to-Step, column scroll +
markers, detail precision), split-screen + trace sync, help tabs, keymap, cold
power cycle persistence. Bugs found were fixed the same session (commit 079a8b2).

**Verified on Pico 2 hardware 2026-07-19 (Session 14 batch, developer eval):**
new distribution features and functions spot-check OK; docs/catalogue OK. The
same eval produced a new observation batch — **logged, not yet fixed**
(`session14-observations-verbatim.md`): Y=-editor long-expression/selection
overlap (truncate with `...`), SD "no card" after an extended power-off
(root cause: late-init retries stop at 30 s — analysis in the file), proposed
red `SD`/`PSRAM` top-bar health indicators, and runtime SD eject/insert being
unhandled (DET pin never polled after boot).

**Verified on Pico 2 hardware 2026-07-19 (Session 13 batch, developer eval):**
the D24 bug fixes (brace-literal broadcast, HOME-nav root replacement, list-editor
negative color) and new implementations work correctly on device; large-array
regressions feel OK in the stats screen; the "Computing..." indicator shows; the
`?`/`list`/`stat` aliases work. The Session 13 HW-PENDING row is cleared; the
Session 12 row's timing-feel question is resolved by the same eval.

**Verified on Pico 2 hardware 2026-07-18 (Session 9 — offline spin + re-fix
verification):** all ten Session 8 fixes, in two rounds, **plus the Session 9
six-item improvement batch (D19/D20 keymap, typed commands, graph chrome,
square ZStandard, DEL/SPACE) — all verified on-device same day** (minor
polish rounds: diag text, hint color/wording, diag line wrap). The offline
spin passed 8/10 on 079a8b2 (including the split-F1 watch-item and
MODE/graphstate persistence across reboots — which also closes the PCG2
one-time-reset check).
The two failures (held-key scroll overrun; battery staleness) were
re-root-caused, re-fixed, flashed, and **verified on-device same session**.
Item 9's verification (charger plug → status bar follows within ~5-6 s, battery
at 84%) also **confirms the charging-bit decode** (`raw & 0x8000`, value-byte
bit 7) — the last open battery question from Session 6.

**Verified on Pico 1 hardware 2026-07-22 (task 3D.14 — the deferred combined
pass, D18):** reflashed to current HEAD (Session 19 font/glyph build) after
three days on Session 7 firmware; serial confirmed a healthy boot (PSRAM OK,
battery telemetry sane, graph recompute running, no crashes/hangs across three
capture windows). Everything in the Phase 2 sweep (headline: split-pane
clipping on the strip renderer — no bleed across the divider), the full
Session 8+9 fix list (screen-stack leak, held-key scroll overrun, status-bar
overdraw/staleness, charging-bit decode, DEG/RAD persistence, wording fixes,
ZStandard, typed commands, F-key remap, case-sensitivity, DEL/SPACE, polar-gap
fix, split-trace activation), and the Phase 3 acceptance checklist (list
editor 3A, stats 3B, distributions 3C, inference 3D, StatPlot layer incl.
box-plot-outlier and normal-vs-skewed contrast tests) passed. Two
non-blocking findings: `!` (factorial) throws a syntax error on this board
(pre-Phase-3 feature, not a regression — **root-caused 2026-07-22 by code
scan, not a hardware/keyboard quirk**, see the Phase 4A-4C paragraph below);
the list editor and a 5000-point scatter plot both feel sluggish (not
profiled). Full verbatim record: `session3D14-pico1-observations-verbatim.md`.
**This closes Phase 3** (retro: `phase3-retro.md`) and resolves **D18**.

**Verified on Pico 1 hardware 2026-07-22 (Phase 4A-4C pass, same session as
3D.14, same build):** first-ever on-device eval of Phase 4A (matrices +
solver), 4B (CALC menu / graph analysis), and 4C (complex numbers) on any
board with a human actually working through the checklists (Pico 2 was
flashed at the time but never hands-on walked for these three — see the
table rows below). Matrix editor, arithmetic, stores, persistence, solver
(form + inline), and big-matrix PSRAM-tier perf all passed (big-matrix ops
feel fine, unlike the list/scatter sluggishness above); CALC menu passed in
full across function/parametric/polar and both angle modes including the
strip-render-risky tangent-line and shaded-fnInt draws; complex numbers
passed in full (mode cycling, arithmetic, store rules, polar glyph,
eigenvalue text display). Two reports cross-checked against `decisions.md`
as intentional, not bugs (matrix+scalar addition dim mismatch — only
scalar-*multiply* is defined, D28; case-insensitive matrix names — deliberate
D28 exception to D19). One real UX gap: no typeable `MatAns` home-screen
token (the last matrix result is editor-only, unlike scalar `ans`). One
feature request: fnInt shading should follow the curve's own color
(alpha-blend or hatching). Full verbatim record:
`phase4abc-pico1-observations-verbatim.md`.

**`!` (factorial) syntax-error root cause, found by code scan (no hardware
needed) 2026-07-22:** `math::complexexpr::evaluate` (`src/math/complex_expr.cpp`)
has no equivalent of `math::engine`'s `preprocess()` postfix-`!`-to-`fac()`
rewrite. In `kReal` number mode (default), `HomeScreen::evaluate_input`
(`src/apps/home_screen.cpp`) only uses `complexexpr` as a discardable probe
and falls through to `math::engine()` (which has the rewrite) on failure —
`5!` works. In `kRectangular`/`kPolar` mode, `complexexpr`'s result is
authoritative and its "Syntax error" (no `!` handling) is shown directly —
`math::engine()` is never called. **Reproduces on any board**, in any
non-REAL Number mode (MODE screen F3 "Number" row) — not a per-unit
keyboard/hardware quirk as originally speculated in the 3D.14 paragraph
above. Fix candidate: add the same `!`-postfix rewrite to `complexexpr`'s
input (or share `engine::preprocess`), or route bare-`!` expressions through
`math::engine()` when they don't otherwise need complex support. Not yet
fixed — logged as a backlog item (see next-session.md).

Still to verify on hardware:

| Item | What to check on hardware |
|------|---------------------------|
| Session 10 round 2 — **CLOSED 2026-07-22** | `L` toggle survives a reboot: confirmed. `rand()` shows a sensible, varying value each call: confirmed. ZTrig tick labels short (`1.571`-style): confirmed. `F` ZoomFit auto-fits the y-range correctly: confirmed. (The original PCG3 one-time-reset transition itself isn't re-testable at this point — both boards are long past it, now on PCG5 — but steady-state persistence is what actually matters going forward, and that's confirmed.) |
| Session 10 round 3 (bulk PSRAM) — **CLOSED, both legs done** | Pico 2 verified 2026-07-18 (`psram-bulk: OK`, 150/156 us); Pico 1 leg verified 2026-07-22 (3D.14): `psram-bulk:` heartbeat and diag `PSRAM: word OK, bulk OK` both clean there too. Nothing further to check |
| Session 11 — Phase 3A lists (flashed 2026-07-19, boot + psram-bulk heartbeat verified over serial) | Home: `{1,2,3}->l1`, `l1+l2`, `l1*2`, `sum(l1)`, `sort_asc(l1)`, `seq(x^2,x,1,10,1)->l2`, error cases (`l1+l6` length mismatch, `5->l1`); results render in history (short lists + `,...` truncation). Editor (`lists` cmd): navigation, type-to-edit, append advance, DEL row shift, F6/F7 sort, F8 clear, horizontal scroll to l4-l6. Persistence: lists survive a reboot; big-list path: `seq(x,x,1,1000,1)->l1` (PSRAM tier) then sort + reboot. Cold power-on: lists appear after late-init (D14 wait, `late-init: lists loaded` if late). Regression: normal scalar eval, history recall, help tabs (new LISTS sections, wider FUNC summary column). Pico 1 leg: the list-editor and list-acceptance ground was covered by the 3D.14 pass (2026-07-22) — see the Pico 1 hardware paragraph above (note: perf feel flagged sluggish there, not yet profiled). **Pico 2 leg closed as a formality (2026-07-22):** same rationale as the Session 16-18 rows — board-independent logic, and the harder rendering case (Pico 1) passed this exact checklist the same day |
| Session 15 — 3D inference + stat plots (D27; flashed 2026-07-20) | **First boot: PCG4 one-time graph-state reset** (window/mode/plots back to defaults — re-set once, then persistence resumes). `test` cmd: T-Test on `{12.9,13.5,12.8,15.6,17.2,19.2,12.6,15.3,14.4,11.3}->l1` vs mu0=14 → t≈.634, p≈.542; same data 2-SampT vs l2, Welch df≈17.65; Stats source entry; 1-PropZ x=57 n=100 p0=.5 (>) → z=1.4 p≈.0808; ANOVA over l1..l3; T-Interval C=.95; error paths (n non-integer, conf=1). `plot` cmd: scatter l1 vs l2 + `Z` ZoomStat on graph; histogram (auto + manual bin width); box plot with an outlier (e.g. append 99); NormProb of a normal-ish list ≈ straight line; three plots at once + a Y= function overlay. Help: COMMANDS test/plot rows, KEYS TEST + STAT PLOTS sections, graph Z row. Regression: trace/table/split unaffected; stats/dist screens fine. Pico 1 leg: covered by the 3D.14 pass (2026-07-22, incl. box-plot-outlier and normal-vs-skewed contrast) — see the Pico 1 hardware paragraph above. **Pico 2 leg closed as a formality (2026-07-22):** same rationale as the Session 16-18 rows |
| Session 15 — storage health (D26) + editor truncation — **CLOSED 2026-07-22, both legs confirmed** | Hot-plug/retry-forever confirmed on both boards (Pico 2 originally, Pico 1 via the 3D.14 pass). Y=-editor truncation (`...` before the checkbox, no overlap) also directly observed incidentally during other on-device testing. Nothing further to check |
| Session 12 — Phase 3B stats (flashed 2026-07-19, boot + psram-bulk heartbeat verified over serial) | `stats` command opens the form; row set follows the analysis (Freq row for 1-Var, Y list + Store for regressions). 1-Var on a small list (`{2,4,4,4,5,5,7,9}->l1` → mean 5, sigx 2, med 4.5, Q1 4, Q3 6), then with a freq list; 2-Var; LinReg on l1,l2 (check r, r², model line); QuadReg exact parabola; SinReg on `seq(2*sin(1.5*x+0.5)+3, x, 0, 12.5, 0.5)->l2` (converged, b≈1.5); Med-Med. **Store to y1** → F5 graph shows the fit; SinReg store in DEGREE mode plots correctly (D23/§10). Error paths on-screen: empty list, length mismatch, LnReg on negative x, non-integer freq. ~~PSRAM-tier timing feel~~ **verified 2026-07-19 (Session 13 eval): large-array regressions feel OK, "Computing..." indicator shows** (D23 revisit closed). Results scroll (2-Var = 17 lines). Help: KEYS commands list + STATS sections. Regression: `lists` editor unaffected, home eval fine. Pico 1 leg: the stats-screen ground was covered by the 3D.14 pass (2026-07-22) — see the Pico 1 hardware paragraph above. **Pico 2 leg closed as a formality (2026-07-22):** same rationale as the Session 16-18 rows |
| Session 16 — Phase 4A matrices + numeric solver (D28; flashed 2026-07-20, boot + psram-bulk heartbeat verified over serial) | `matrix`/`mat` editor: TAB cycles [A]-[J]+Ans(RO), F7 DIM reshape, F8 clear, cell edit/advance feel; bracket typing (`[`/`]`) on the physical keyboard. Home: `[A]*[B]`, `2*[A]`, `[A]^-1`, `[A]^T`, `[A](2,3)` element read, `det([A])`/`rank([A])` inline scalars, `inverse`/`rref`/`ref`/`augment`/`identity`, `dim([A])`/`eigenvals([A])` (list results into l1-l6), `-> [C]`/`-> lk`/`-> a` stores, MatAns re-use. `matrices.dat` first save + a power cycle (magic PCM1). `solve` form screen (Lower/Upper/optional Guess, residual + iterations) and inline `solve(f,x,lo,hi)` / `solve(f,x,guess)` / `solve(lhs=rhs,...)`. Big-matrix (>16x16, PSRAM tier) edit/op timing feel. Help: COMMANDS matrix/solve rows, catalog entries. Regression: lists/stats/dist/infer/graph unaffected, home eval fine. **Pico 2 leg closed as a formality (2026-07-22):** logic is board-independent (no `#ifdef` branches), and the harder strip-render case (Pico 1) passed this checklist in full the same day — see the Phase 4A-4C paragraph above. Only genuinely board-specific gap: perf feel hasn't been re-measured on the Pico 2 against current code (only the pre-Phase-3 2.25 baseline exists) |
| Session 17 — Phase 4B graph analysis / CALC menu (D29; flashed to the Pico 2 as part of Session 19's build 2026-07-21, though not hands-on walked there — the "NOT flashed" note in the original entry below only describes the state as of Session 17 itself) | F6 "CALC" softkey on the graph screen (all three modes) and typed `calc`/`analyze`: menu feel, cursor-riding curve pick, the TI-style step prompts ("Left Bound?"/"Right Bound?"/"Guess?", "First curve?"/"Second curve?" for intersect). Value/Zero/Min/Max/dy-dx/fnInt on a function (e.g. `4-x^2`), a parametric pair (unit circle slope), and a polar curve (cardioid/circle area) in both angle modes. Tangent-line draw for dy/dx; shaded fnInt region (function mode) for strip artifacts; result readout + Ans/independent-variable store. Intersect on two curves, and the same-curve-refusal case. Judge whether the min/max "Guess?" step feels wrong given it doesn't feed Brent's bracket (D29 judgment call). Regression: existing trace/table/split/matrix/stats/dist/infer screens unaffected. **Pico 2 leg closed as a formality (2026-07-22)** — same rationale as the Session 16 row above; the full CALC-menu checklist, including the strip-render-risky tangent-line/shaded-fnInt draws, passed on the Pico 1 the same day |
| Session 18 — Phase 4C complex numbers (D30; flashed to the Pico 2 as part of Session 19's build 2026-07-21, though not hands-on walked there — the "NOT flashed" note in the original entry below only describes the state as of Session 18 itself) | MODE screen "Number" row cycles REAL/a+bi/r<t and persists (first boot after upgrade: **PCG5 one-time graph-state reset**). Home screen in REAL mode: `3+2i`, `sqrt(-4)`, `(1+i)^2` etc. now say "Non-real result" instead of showing `NaN` — judge whether that read is clear. Switch to a+bi: `3+2i`, `sqrt(-4)`->`2i`, `(1+i)^2`->`2i`, `e^(i*pi)`->`-1`, `abs(3+4i)`->5, `conj`/`real`/`imag`, store `5->a` works, `2i->a` errors "Complex results can't be stored". Switch to r<t (polar) mode: same expressions display as `r<theta` (ASCII `<` stand-in for ∠ — judge if that reads OK or needs a real glyph). Non-REAL mode should still reach the rest of the real catalog (`ncr(5,2)`, `round(3.456,1)`, distributions) as long as their own arguments aren't complex — spot check a few. Matrix: `eigenvals([A])` on a rotation-like 2x2 (`[[0,-1][1,0]]`) now shows `{i,-i}` as text instead of erroring; storing it (`-> l1`) still errors. Regression: existing REAL-mode home eval, matrices, lists, stats, dist, infer, graph analysis all unaffected — this was the largest single-session diff yet (7 new/changed math source files) so a broad sanity pass is worth it, not just the new surface. **Pico 2 leg closed as a formality (2026-07-22)** — same rationale; full complex-number checklist passed on the Pico 1 the same day (note: this checklist's own `r∠θ` description is stale — superseded by the real ∠ glyph, D31; and its "spot check the real catalog" item is now known to have one gap, the `!` postfix-factorial bug root-caused the same day, see the paragraph above) |
| 4D Batch 1 — complex variables/Ans (4D.15) + complex lists (4D.24) — **CLEARED 2026-07-26, same day** | Developer ran the full checklist on the Pico 1 (complex var store/recall/errors, complex list literals/arithmetic/reductions, editor entry + migration, PCV1 one-time reset + persistence, graph sanity with a complex x) — all passed. One finding: complex display in the list editor was weird under polar mode (complex elements fell back to a+bi; real-valued elements showed `r∠0`) — **root-caused and FIXED the same day** (see the 2026-07-26 addendum below), reflashed, and developer-confirmed working as intended |
| 4D Batch 9 — device polish (4D.19-20; flashed 2026-07-26, boot + temp/psram-bulk heartbeats verified over serial). **The interactive half of this batch is exactly what needs the board in hand** | Typed `settings` (alias `setup`): LCD brightness LEFT/RIGHT visibly changes the panel (16 steps + 100%); Kbd backlight row lights the keys (vendored STM32 reg 0x0A — first-ever use, watch for I2C weirdness); Auto power-down cycles OFF/1/2/5/10/30 min. **APD (default 5 min)**: leave the unit idle → screen dims to black (backlight 0, kbd light off); ANY key wakes it and that key must NOT type into the input line; verify the unit never wedges across several sleep/wake cycles (STM32 writes are paced 250 ms + bus-idle-gated — but this is the first new STM32 write path since battery). Settings survive a power cycle (`settings.dat`, PCS1); a fresh card keeps STM32 boot-default brightness. Cold boot: settings load may arrive via late-init (serial `late-init: settings loaded`). Regression: keyboard feel unchanged (polling untouched), battery indicator still updates ~5 s, no I2C errors on serial during sleep/wake. DISPOFF deferred (backlight-only sleep, v1) |
| 4D Batch 8 — matrix eigenvectors (4D.23; flashed 2026-07-26, boot + temp/psram-bulk heartbeats verified over serial) | Home: `[[2,1][1,2]]->[A]` then `eigenvec([A])` → columns `[.707,.707]` and `[.707,-.707]` (order matches `eigenvals([A])` = {3,1}); `2*eigenvec([A])` composes; `eigenvec([A])->[B]` stores. Defective/repeated: `[[2,1][0,2]]` and `identity(2)` error "No unique eigenvector"; rotation `[[0,-1][1,0]]` errors "Complex eigenvalues"; a complex-valued `[A]` errors "Non-real matrix". Help FUNC tab shows the eigenvec row. Regression: eigenvals/eig unchanged |
| 4D Batch 7 — display & formatting (4D.1-5; flashed 2026-07-26, boot + temp/psram-bulk heartbeats verified over serial) | MODE > Display now cycles FLOAT/FIX/SCI/**ENG**: in ENG, 12345 shows `12.345e3`, 0.005 shows `5e-3`, exponents always multiples of 3. Home: `0.75>frac` → `3/4`, `1/3+1/6>frac` → `1/2`, `ans>frac`, negative and integer cases; irrational values (e.g. `sqrt(2)>frac`) fall back to decimal; `>dec` evaluates plain. Graph: ZTrig ('T') then 'L' labels — x ticks read `π/2`, `π`, `3π/2`, `2π` with the real `π` glyph (y ticks stay numeric); non-`π` windows unchanged — **first eval found the `π` blank (small font lacked slot 127); fixed + reflashed same day, re-check on the current build**. Stats 1-Var results show true subscripts `Sₓ`/`σₓ` (2-Var keeps Sx/Sy pairs plain — no subscript-y glyph exists); check the subscript-x glyph shape in situ (all five fonts regenerated with slot 141). Pretty print: `1/2+3` — the fraction bar now centers on the text midline instead of hanging the stack low; check `1/sqrt(2)`, `x^2/2` still stack correctly. Regression: FLOAT/FIX/SCI displays, existing pretty-print layouts, stats output otherwise unchanged |
| 4D Batch 6 — named lists (4D.13; flashed 2026-07-26, boot + temp/psram-bulk heartbeats verified over serial) | Home: `{1,2,3}->costs` creates a named list (2-5 chars, letter-first; reserved/function/constant names and `l7`-style rejected); `costs`, `costs*2`, `l1+costs`, `sum(costs)`, `dot(costs,qty)`, `sort_asc(costs)` (in place), `costs->l2` all work; result suffix shows the name with the store glyph; 21st list errors "Too many named lists". Editor (`lists`): LEFT/RIGHT scrolls past l6 into named columns (header shows `costs:3`); cell entry/append/delete/sort/F8 work there; **Alt+N** prompts for a new list name (jumps to its column), **Alt+R** renames (named only; l1-l6 refuse), **Alt+X** deletes (column disappears, file removed). Stats: List/X/Y/Freq rows cycle past l6 into named lists, result header shows the name. Plot config: X/Y list rows cycle named lists too; a scatter over named lists draws; deleting a named list a plot references leaves that plot silently empty. Persistence: named lists + directory survive a power cycle (`listdir.dat` + `nlist<idx>.dat`); complex named lists work (PSRAM tier). **Sort-persistence fix**: home-screen `sort_asc(l1)` (no store) now persists across a reboot — pre-4D.13 it silently didn't (D35 gap) and could write a bogus file from an out-of-bounds read. Regression: l1-l6 flows unchanged |
| 4D Batch 5 — data & catalog glue (4D.12/14/17/18/22; flashed 2026-07-26, boot + temp/psram-bulk heartbeats verified over serial). **2026-07-27: one bug found — `MatAns` does not survive a power cycle** (unlike named matrix variables `[A]` etc., confirmed persistent under Batch 2). Briefly reclassified 2026-08-02 as by-design (a transient global never written to SD); **reversed the same day (D39): MatAns now persists to `/picocalc/matans.dat`, HW-verified surviving a cold boot on the Pico 2 — see the "MatAns now persists" 2026-08-02 worklog entry** | Home: `[[1,2][3,4]]` literal evaluates (and `det([[1,2][3,4]])`, `[[1,2][3,4]]->[E]`, ragged rows error); `matans` recalls the last matrix result (`2*matans`, `matans->[F]`); `list2mat(l1,l2)` packs columns (shorter list zero-pads) and `mat2list([A],l1,l2)` unpacks + persists the lists ("Done" text); `dot({1,2,3},{4,5,6})`=32, `cross` of 3-elem lists (storable), `norm({3,4})`=5, `norm([A])` Frobenius (complex too); `clight`, `navo`, `rgas` etc. evaluate as identifiers; typed `const` opens the constants picker — ENTER inserts the name into the input line; `convert(1,"mi","km")`≈1.609 with or without quotes, case-insensitive, composes inline (`2*convert(1,hr,min)`), temp conversions (`convert(100,c,f)`=212), cross-family errors "Units don't match". Help FUNC tab shows the new rows. Regression: existing matrix/list expressions, solve() inline, complex paths unaffected |
| 4D Batch 4 — zoom + shading (4D.9-11) — **CLEARED 2026-07-27** | ZDecimal, ZSquare, ZBox (crosshair/rubber-band/Alt-move/ESC-cancel), per-function shade toggle (none/above/below) with persistence, two-curve `H` band shading, and fnInt shaded-region color all verified on the Pico 1 — no findings. Full checklist as originally logged: Graph keys: 'D' ZDecimal (trace x lands on clean 0.1 steps, origin-centered), 'Q' ZSquare (a drawn circle looks round after it), 'B' ZBox (crosshair, ENTER first corner, rubber-band rect, ENTER zooms into the box; Alt+arrows move 10 px; ESC cancels; double-ENTER on the same spot does nothing). Y= editor: 'S' on a slot cycles shade none→above→below ('^'/'v' marker beside the checkbox, persists across reboot via PCG6 shade_mode); graph shades above/below the curve in the slot's own color, dimmed, curve on top. 'H' on the graph (function mode): pick lower curve (UP/DOWN, candidate draws thick), ENTER, pick upper, ENTER → band between the curves shades; 'H' again clears. fnInt (F6 CALC) shaded region now follows the curve's palette color darkened instead of fixed blue (2026-07-22 feature request). Split-pane: ZBox/shades render inside the pane. Regression: existing zoom keys (S/T/F/Z/-/=), trace, CALC ops unaffected; PARAM/POLAR/SEQ unaffected by shading keys |
| 4D Batch 3 — sequence graphing (4D.6-8) — **CLEARED 2026-07-27, two minor bugs found (not blocking, not yet fixed)** | Verified on the Pico 1: MODE cycling, basic ramp, Fibonacci table, cross-referenced sequences, window/ZoomFit, WEB-mode cobweb convergence, bad-form error cases, F6 CALC no-op — all passed. Two findings: (1) SEQ-mode trace (F4) doesn't snap to exact n/u(n) values — shows float noise instead of the exact integers the table shows for the same points; (2) the sequence color swatch in the editor list tracks recursive-vs-explicit form, not the assigned plot color — recursive definitions (reference u(n-1)/u(n-2)) always show red, explicit ones (computed directly from n) always show white, regardless of the actually-assigned color (the graph itself still plots in the correct color). See `testdrive-2026-07-27-observations.md` for the raw report. Full checklist as originally logged: **First boot: PCG6 one-time graph-state reset** (window/mode/plots to defaults, then persistence resumes). MODE > Graph mode now cycles FUNC/PARAM/POLAR/SEQ; new "Seq plot" row (TIME/WEB). F1 in SEQ mode opens the sequence editor: enter `u(n)=u(n-1)+1`, seed `u(nMin)=1`, check enable → F5 graph plots the ramp; trace (F4) reads `u  n=… x=… y=…` and steps by PlotStep. Fibonacci: `u(n)=u(n-1)+u(n-2)` with seed `{1,1}` → 1,1,2,3,5,8… in the table (F5 TBL, integer n column; non-integer TblStart rows show NaN by design). Cross-ref: `v(n)=2*u(n-1)`. Window (F2) shows nMin/nMax/PlotStart/PlotStep + X/Y (10 rows). ZoomFit ('F') fits the time series. WEB mode: `u(n)=0.5*u(n-1)+2`, seed 1 → map line + y=x diagonal + cobweb stair converging to 4; non-eligible seqs (cross-ref/lag-2/explicit-n) don't draw in WEB. F6 CALC is a no-op in SEQ mode (v1). Bad forms error out (slot inactive): `u(n)` circular, `u(n-3)`, `u(3)`. Regression: FUNC/PARAM/POLAR plotting, trace, table, split, stat plots unaffected; persistence of all modes' slots across a power cycle (post-PCG6-reset) |
| 4D Batch 2 — complex matrices (4D.25) — **CLEARED 2026-07-27** | Editor a+bi/polar migration, REAL-mode "Non-real result" guard, `det`/`rref`/`ref`/`[A]^-1`/`rank`/`transpose`/`augment`/`[A]^2`, scalar `i*[B]` multiplication, mixed-matrix add, element read, complex scalar store, and power-cycle persistence all verified on the Pico 1 — no findings (one result that looked wrong mid-session turned out to be a data-entry mistake, not a bug). Full checklist as originally logged: Editor: type `1+i` into a real matrix cell in a+bi mode → whole matrix migrates to the complex tier, cells show short complex forms, entry line shows the full form; polar mode shows `r∠θ` cells; REAL mode entry of `2i` errors "Non-real result"; F8 clear reverts the matrix to real. Home: `det([A])` on a complex `[A]` (e.g. `[[1+i,2][3,4-i]]` → `-1+3i`), `[A]^-1` then `[A]*Ans`-style check by hand, `rref`/`ref`/`rank`/`transpose`/`augment`/`[A]^2`, `i*[B]` and `2i*[B]` on a real `[B]`, mixed `[A]+[B]`, element read `[A](1,1)`, complex scalar store `det([A])->z`. REAL mode: any expression touching a complex matrix errors "Non-real result". `eigenvals([A])` on a complex matrix errors "Non-real matrix". Persistence: complex matrix survives a power cycle (PCM2 header unchanged, 16 B/elem payload); old firmware would skip it as corrupt. Regression: real-matrix arithmetic/editor/persistence unchanged; big real matrix ops still fine |
| Session 19 — font system + real math glyphs, `eig` alias, list UX (D31; flashed 2026-07-21, **Terminus** default build, boots healthy, PSRAM/storage/battery telemetry clean) | This session's own on-device font comparison across all five builds is already done (D31: Terminus picked as the shipped default; Unifont good with the 2px lift; Spleen best if a thicker font is wanted; JuliaMono worst, Iosevka a bit unbalanced) — remaining is a **glyph-correctness sweep on the Terminus build in situ**: home-screen complex results (`3+2i`, polar `2∠60`, store `⇒`), MODE Number row (`a+bi`/`r∠θ`), pretty-printed expressions (`π`, `θ`, inline `√(x)`, `3+2i` via the plain-text fallback), stats `σx`/`σy`/`Σx`/`Σx²`/`Σy`/`Σy²`/`Σxy`/`r²`, inference `≠`/`μ`/`σ`, distribution `μ`/`λ`, graph-trace + table polar label `θ`, and `…` truncation in list/matrix/complex history + slot editor. Also: `eig` as a drop-in alias for `eigenvals([A])` (whole-expression only, same as `eigenvals`/`dim`); list history LEFT/RIGHT horizontal scroll on the newest result when the input line is empty, using the new compact (4-sig-fig) number format so more list elements fit per screen. Regression: existing REAL-mode home eval, matrices, lists, stats, dist, infer, graph analysis, table all unaffected. **Informally spot-checked 2026-07-22** during the Phase 4A-4C Pico 1 pass — complex/MODE-row/pretty-print/stats glyphs incidentally seen and reported looking correct — but not a dedicated sweep against this row's own full list (still worth doing properly if time allows) |

---

## 2026-08-08 — Y= editor lockup + ZTrig in DEGREE: the D45 bug class, three times over (D47). **HW-verified on the Pico 1**

Both items from `testdrive-2026-08-05-observations.md`.

**The Y= freeze was a core-0 stack overrun into core 1's stack.**
`SlotEditorScreen::render()` coloured each row by calling
`math::engine().compile()` — **inside the renderer** — and that frame measured
**2,232 B** on the linked Pico 1 ELF, almost all of it
`te_variable lookup[122]` (1,952 B) rebuilt on the stack every compile. Core 0
has 4 KB before `__StackOneTop`. Strip mode renders 16-px bands and the header
bar is exactly 16 px, so strip 0 pushed fine, then core 0 rendered strip 1
**while core 1 was mid-DMA on strip 0**, overran into its stack and killed it;
core 0 then blocked forever in `wait_one_ack()`, taking key polling with it.
That is the reported "first few pixel rows of the header, then dead, every
time, power cycle only", exactly. The graph screen survived the same stored
expressions because `recompute_function` sits ~200 B shallower and runs
outside `render_frame` with core 1 parked.

**A second, deeper instance turned up while fixing it.** The new frame report
(below) found `HomeScreen::evaluate_input` (872) → `listexpr::evaluate`
(1,192) → `eval_list_into` (**2,248 and recursive**) = 4,312 B at depth 1 —
so a plain `{1,2,3}` on the home screen was already overrunning, silently, on
a path HW-verified since Phase 3A.

Fixes, per **D47**:

- **`render()` only draws.** Validity is cached in a `valid_mask_` bitfield,
  refreshed from `on_activate()`/`on_key` — the contract `list_editor.hpp` has
  documented since Phase 3A and the slot editors never got. Also removes ~140
  `te_compile` calls (with mallocs) per frame.
- **tinyexpr's binding table moved to bss**, built once (it is stable after
  startup). `Engine::compile` **2,232 → 280 B**, `eval_internal` the same,
  `compile_with` 2,368 → 288 — firmware-wide, so graph recompute, table, seq
  and home eval all benefit.
- **List path:** `noinline` on the four leaf evaluators (inlined, their
  `kMaxLen` locals — `eval_seq`'s `arg[5][256]` worst — were charged once per
  recursion level), non-reentrant buffers to bss, genuinely-per-level buffers
  **depth-indexed** like the file's existing `g_temp[kMaxDepth]`, and a hard
  `RecGuard`/`kMaxRec = 3` cap in `eval_list_into` itself (the old `ctx.depth`
  never covered the sort or lift paths). `eval_list_into` **2,248 → 32 B**.
- **`PICO_USE_STACK_GUARDS=1` + `PICO_STACK_SIZE=4096`** — the top deferred
  item from the last handoff — putting an MPU trap at
  `__StackBottom == __StackOneTop`, with a new `src/platform/fault.cpp`
  recording the faulting PC in a watchdog scratch register and rebooting.
  Next boot prints `fault: previous boot hard-faulted at pc=0x...` on the 30 s
  heartbeat. Without that handler the guard would turn silent corruption into
  an indistinguishable lockup (the SDK default handler is an infinite loop).
- **`scripts/size-report.sh` gained a stack-frame listing** — walks prologues
  out of the ELF and reports anything over a threshold. This is the
  measurement that root-caused the bug and then found the list-path instance;
  it is also the deterministic half of the guard, which on RP2040 is only a
  32-byte MPU subregion a large frame can step over.

**ZTrig now follows the Angle mode** (`zoom_trig`): DEGREE gives
x -360..360 / Xscl 90, RADIAN unchanged at $\pm 2\pi$ / $\pi/2$. `tick_label` needs no
change — `pi_multiple(90,…)` does not match, so degree ticks label
numerically.

**Measured** (Pico 1): Y= render path ~3,200+ and overrunning → **424 B**;
worst home-screen list expression ~4,300+ and overrunning → **3,152 of 4,096**
(944 B margin). Host suite green, 12 suites — `test_math` 230 → **235**
(compile_with/compile shared-table aliasing), `test_lists` 239 → **241**
(recursion cap). Both boards build clean; `lint.sh`/`format.sh` clean.

**Cost, and it is not small:** Pico 1 `.bss` **198,836 → 209,120 (+10,284)`,
headroom 61.8 → 51.8 KB. That comes straight out of the **Phase 6 margin** —
with a 48 KB MicroPython heap the spare drops from ~14 KB to ~4 KB, so the
levers in `pre-phase5-review.md` (heap 48→40 KB, ArrayStore slab cut,
`g_chunk` fold) are now likely rather than optional. Note also that `size`'s
total jumps a further **4,096 B that is not real**: `PICO_STACK_SIZE`
2048 → 4096 doubles both `.stack_dummy` sections, which live in the dedicated
`SCRATCH_X`/`SCRATCH_Y` banks and were never allocatable. **Compare `.bss`
alone across this change, not `size`'s total.**

**Flashed to the Pico 1 — and the pass found a third instance of the same
class, which is the interesting part.** Boot was clean and the guard did not
trip through init, but F1/F4/F5 still failed — now as "black screen, then back
to the home screen" instead of dead keys. That is the new fault handler
working. Serial on the next boot:

```
fault: previous boot hard-faulted at pc=0x100551da
```

→ `factor+0xa` in `tinyexpr.c`, the `push {r5, r6, r7, lr}` **prologue**
instruction. A fault on the prologue push is unambiguous stack overflow, and
it named the gap this session had missed: **D45 capped the CAS parser's depth;
tinyexpr's parser never had a cap.** Its recursion costs **200 B/level**
(measured), so depth is whatever the input says — and the input was Y1, still
holding one of the "up to 20 nested trig calls" perf stress probes from
`testdrive-2026-08-02`. The Y= path allows 16. Hence "every time", and hence
F4/F5 failing too (`recompute_function` compiles the same slot).

Added **`kMaxParseDepth = 8`** in `eval_internal`/`compile`/`compile_with`,
sized to the tightest caller (the list-lift path leaves ~1,696 B → eight
levels; the Y= path alone would allow sixteen, but one cap has to hold
everywhere). Over-deep input is a parse error now: the row draws red and stays
editable. `test_math` 235 → **242** (depths 9/20/40 through both `compile` and
`evaluate`).

**Verified on the Pico 1 after the reflash:** Y= opens and renders, **Y1 draws
red** (the stress probe, correctly rejected), the graph works, and three
consecutive `graph recompute:` lines came in at 103,163 / 103,107 / 103,019 us
with no fault — the same path that had been faulting deterministically.

The sequencing is the lesson: guard + fault reporter turned a second unknown
instance of this bug class into a PC and a one-line diagnosis on the first
flash. Shipping the guard *without* the handler would have produced another
indistinguishable lockup.

**ZTrig in DEGREE confirmed working on the same pass**, closing the second
item from the 2026-08-05 testdrive.

**Follow-on, same session: `math::complexexpr` capped too (D47).** Flagged
above as the next uncapped parser, and it is — but it needed a different
treatment. Two recursion cycles, both through `parse_unary`: paren/function
nesting, and right-associative `^` (`parse_power` -> `parse_unary`) which has
**no parentheses at all**, so `2^2^2^...` nests once per caret and a
paren-depth pre-scan like tinyexpr's would miss it entirely. At 360 B/level
one cap could not serve both entry points — the home screen enters ~1,208 B
in, list/matrix evaluation ~2,400 B in — and a single conservative 4 would
have broken `test_real_pow_exact`'s D45 rung-4 ladder case. So the cap belongs
to the entry point: `kMaxParseDepth = 7` / `kMaxParseDepthNested = 4`. Home
3,728 / list 3,840 / matrix 3,496, all of 4,096. Getting the list prefix down
needed the same bss treatment on `listexpr::evaluate` (720 -> 280 B) and
`eval_clift` (360 -> 176 B) — **and one of those was wrong**: making
`eval_clift`'s `CTerm terms[]` static segfaulted `2i*l1`, because only `.sign`
is assigned per use and the rest comes from default member initializers that
run per call for a stack local but once for a static. `test_lists` caught it;
reverted to the stack. `test_complex_expr` 113 -> **122**; `.bss` 209,120 ->
209,888.

**`math::matexpr` is now the last uncapped parser and the worst of the three**
— 808 B/level, ~2 levels of headroom. Left alone: it needs its own measurement
and probably frame reduction first.

**Still unverified on hardware:** the complexexpr cap, and the wider
guards-are-live sweep (matrix ops, large-list editor, stats, the D45 nesting
ladder) — plus the whole Phase 5 CAS on-device checklist this bug had been
blocking. Pico 2 not flashed. See `next-session.md`.

---

## 2026-08-05 — Phase 5 Stage 5: CAS hardening (4D.22, D45) + two complex-evaluator bugfixes (D46). **PHASE 5 CLOSED, HW-verified on both boards.**

Stage 5's brief was "stress testing + edge cases". The audit found a live
memory-corruption bug first, and the bench pass found two more in Phase 4C.

**D45 — the headline.** `simplify_sum` and `simplify_product` each held four
`kMaxOperands = 64` arrays on the stack: **1,144 B and ~1,140 B frames**,
measured on the linked Pico 1 object. They nest through `simplify_rec` once
per level of ADD-inside-POW-inside-ADD. Core 0 has 2 KB declared
(`__StackBottom 0x20041800`) and only **4 KB before `__StackOneTop`** — core
1's stack, running the display service on both boards since D10 leg A.
`PICO_USE_STACK_GUARDS` is not set, so nothing traps. Reach is wider than CAS
calls: `exact_form()` runs parse + two simplify passes on *every* home-screen
input whose literals are all integers.

**Reproduced on the Pico 2, and the repro is the interesting part.** The
ladder `(2+1)^2+1` → … out to rung 6 (six nested levels, ~6.9 KB) **returned
the correct answer** (1.173e32) with 46 `temp:` and 46 `psram-bulk:`
heartbeats, no gap, no fault, no reboot. It overran past core 1's stack top
*and* its declared bottom into the unused gap above the heap; core 1's
display loop only occupies the top few hundred bytes of its region, so
nothing live was hit. **Silent memory corruption whose blast radius depends
on what core 1 is doing at that instant — not a deterministic fault**, and
structurally invisible to the host suite (8 MB stack).

Fixes: the `ExprPool` arena is now **two-ended** (nodes bump up, pass scratch
bumps down under LIFO mark/release via `ScratchScope`) — scratch cannot share
the node end because `simplify()` runs its fixed-point loop up to 50 times
without resetting; **stated depth caps** replace bounds that fell out of input
length (parser 12, simplifier 8), sized to the measurement now that the
deepest-recursing frame is `integrate_rec` at 172 B; **Risk 2 implemented**
(sticky `overflowed()` + `near_capacity()`, `evaluate_home` reports "Too
complex", `exact_form` leaves the decimal standing) instead of `simplify()`'s
"last good form" masquerading as converged; and **`expand()` no longer
simplifies twice** (~5.4 KB of a 22.5 KB arena spent re-canonicalising an
already-canonical tree — enough alone to push `expand((x+1)^10)` over).

Largest recursive CAS frame **1,144 B → 172 B**; worst-case CAS stack ~2.6 KB,
bounded. Pico 1 bss **201,096 → 198,836**. `test_cas` **272 → 368** checks.
The new `test_stress_edge_cases()` earned itself immediately by catching a
defect in the first cut of this very change: `alloc_raw` bounded the node end
against the arena end rather than the scratch end, so nodes bumped through
live scratch arrays and a pass read back overwritten `Expr` pointers.

**D46 — two Phase 4C defects found on the bench** (not Phase 5 regressions;
both have shipped since Session 18, and surfaced only because the Stage 4
script sends the tester into RECT/POLAR). (1) **DEGREE mode was silently
ignored** in non-REAL Number modes: `c_sin(z)` calls `std::sin` on the raw
value with no `rad()` conversion, so the complex evaluator answered every
trig call in radians — `sin(30)` gave `0.5` from the real path and
`-0.9880316241` from the complex one. Fixed with angle wrappers in
`complex_expr.cpp`'s `kFns` table (the only caller of the `c_*` trig
functions, so the wrapper layer is complete by construction); `complex.cpp`
stays pure math because `Complex` is shared with matrices/lists/stats.
(2) **`c_pow` was not exact for real powers** — unconditional
`c_exp(c_ln(base) * exp)`, so `10202^2` came back a hair off `104080804`,
failing `format_number`'s `x == floor(x)` test, dropping from `"%.0f"` to
`"%.10g"`, printing a fractional digit, and rendering amber where REAL mode
rendered white. Only rung 4 of the ladder showed it: rungs 1-3 are too small
for the drift to reach ten significant digits and rungs 5-6 take the
scientific branch — **104080805 is nine digits, the only value whose tenth
significant digit lands in the fraction.** The amber itself was *not* a bug;
the exact-form feature was working as designed and the defect was upstream in
the arithmetic. `test_complex_expr` **75 → 113** checks.

**Hardware.** Pico 2 reflashed and verified. **The Pico 1 was flashed with
Phase 5 for the first time** and passed: the ladder computes clean, CAS ops
are perceptibly slower but well inside budget (no FPU), the D46 fixes hold,
and — uniquely testable on this board, since it was still on Phase 4D — the
**legacy two-field `history.txt` migrated correctly** (old lines reload as
plain, new symbolic results reload amber), closing the 2026-08-03 fix's
outstanding on-device confirmation.

**Deliberately deferred, recorded rather than fixed**: (a)
`PICO_USE_STACK_GUARDS=1` + `PICO_STACK_SIZE=4096` would trap this whole bug
class, but it is a whole-firmware change — this session proved one path
overran silently and others may currently work *because* nothing traps; it
needs its own soak. (b) **A latent MODE-clobber confirmed by code reading but
not observed on hardware**: `main.cpp:432` re-runs `load_graph_state()` when
storage arrives late (D14 rail settle), and `graph_persist.cpp:56` is
`*this = g_image.state` — a whole-struct overwrite — so a MODE toggle made
before storage mounts is silently reverted (its `save_graph_state()` also
failed). Needs a genuine cold power-on plus a toggle inside the window.
(c) **Inverse-trig exact forms** (`asin(1)` → $\pi/2$) — a missing feature,
not a bug; on the wishlist.

`PICOCALC_PHASE` bumped `"4D"` → `"5"`. Full host suite green (14 suites),
both boards build clean, lint/format clean. Commits `b39ae3e` (D45),
`5ef025f` (D46).

---

## 2026-08-03 — Stage 4 follow-ups: Alt+Enter decimal escape, exact trig, non-REAL modes (flashed to Pico 2)

Three gaps that surfaced on the first Pico 2 flash of the Stage 4 work below.
Decision **D44**. (Session ran past midnight — the Alt+Enter rebind landed
2026-08-04 as commit `fd61849`; kept in this entry since it is the same piece
of work.)

**Alt+Enter is the decimal escape.** With an expression entered it evaluates
with the exact-form probe suppressed, identically to a trailing `>dec`. With the
input line empty and the newest result being an exact form, it re-runs *that*
expression as a decimal — so an amber `√2` becomes `1.414213562` without
retyping. **Alt, not Shift**: the first cut bound Shift+Enter, but the diag
screen showed that chord arriving as key code 59 (`kInsert`) rather than 52
(`kEnter`) with `shift_held` — the STM32 *translates* Shift chords into their
own scan codes (Shift+Enter -> 0xD1, the same family as Shift+F1..F4 ->
F6..F9) instead of reporting base-key + modifier, so a Shift binding would
never have fired. Rebound to Alt, which passes its flag through intact the
way Alt+UP/DOWN already does, and which leaves the real Insert key free; the
translation behavior is now recorded in `platform/keyboard.hpp` beside the D12
arrow note. Implemented as `evaluate_input(bool force_decimal)` feeding the
existing `to_dec` flag, so it is one parameter, not a new display path. Commands (`cls`, `help`, ...) are unaffected. Documented in
the on-device HELP `#HOME` block and a new `#EXACT FORMS` block on the SYNTAX
tab.

**Exact trig at special angles.** `sin`/`cos`/`tan` of a rational multiple of $\pi$
with denominator in {1,2,3,4,6} now fold: `sin(pi/3)` → `√3/2`, `tan(pi/6)` →
`sqrt(3)/3`, `cos(pi/3)` to `1/2`. A table indexed in *twelfths of $\pi$* covers
both the $\pi/6$ and $\pi/4$ families in one lookup, and `cos(x) = sin(x + pi/2)` is an
index shift so only sine and tangent tables exist. **Angle-mode aware**: in
DEGREE mode the argument is read as degrees, so `sin(60)` folds exactly as
`sin(pi/3)` does in RADIAN — which is where a user most naturally types it.
Recognition is `math::frac::pi_multiple`, finally used for what the Stage 4 plan
originally expected it to be used for.

**Non-REAL number modes now get exact forms** for real-valued results — the D43
v1 limitation, which was a scoping decision with no technical reason behind it.
The probe moved into a shared `apply_exact_form` helper called from both the
REAL and the complex dispatch branches. Genuinely complex values stay decimal
(the CAS reserves `i` as a variable, which the no-variables gate rejects).

**"Interesting" now compares formatted strings rather than doubles.** A bare
integer is still normally not upgraded — the numeric path already shows it — but
it *is* when the numeric path would display something else. That is what lets
`sin(pi)` show `0` instead of `1.224646799e-16` and `cos(pi/2)` show `0` instead
of `6.123233996e-17`: float noise the numeric path cannot avoid and the exact
path knows the answer to. Comparing doubles would also have caught `tan(pi/4)`,
whose `0.9999999999999999` already formats as `1`; comparing `format_number`
output is the precise test, because the display is what the gate is about.

**Verification**: host suite green, `test_cas` **238 → 272** checks (new
`test_exact_trig` covers both angle modes, the undefined `tan(pi/2)`, non-special
angles, and the float-noise cases), 0 failures across all 15 suites. Both boards
build clean; Pico 1 bss **201,096, still exactly flat**; flash +5.3 KB (the trig
table and helpers). `lint.sh`/`format.sh` clean. **Flashed to the Pico 2**
(`picotool load -f -x`), clean boot confirmed over serial — psram-bulk healthy,
die temp 39 C. Interactive confirmation is the user's next pass.

Files: `src/math/cas/exact.cpp` (trig table, `eval_numeric`, `fold_exact_trig`,
formatted-string "interesting" test); `src/apps/home_screen.{hpp,cpp}`
(`apply_exact_form` helper, `evaluate_input(bool)`, Alt+Enter handling);
`src/platform/keyboard.hpp` (documents the Shift-chord translation);
`src/apps/help_screen.cpp` (HELP text); `tests/host/test_cas.cpp`.

---

## 2026-08-03 — Phase 5 Stage 4: exact-form (surd) display, source changes, host-verified

Stage 4 of Phase 5 (`phase5-spec.md` §10.1, tasks **4D.23** + **4D.24**), on the
`phase-5` branch. Home-screen results that have a clean closed form now display
that form instead of a truncated decimal: `sqrt(2)` → `√2`, `sqrt(8)` → `2√2`,
`1/sqrt(2)` → `√2/2`, `pi*2` → `2π`, `pi/2` → `π/2`, `1/3` → `1/3`. Decision
**D43**, which also resolves the two open questions the spec left on this
feature: **P5-5 → always-on** (no MODE toggle; `>dec` is the per-result opt-out)
and **P5-6 → yes, `pi` is included**.

**Recognition (4D.23)** lives in a new `src/math/cas/exact.cpp`, deliberately
**not** in `simplify()`: `simplify()` runs inside `integrate()`, `solve()`,
`factor()` and the derivative fixed-point loops, so a `POW(NUM, 1/2)` rewrite
there would change node shape mid-loop for passes that pattern-match on `POW`
(§13 Risk 1) for zero Stage-4 benefit. Keeping it probe-path-only held the blast
radius to one call site — proved by the 199 pre-existing `test_cas` checks coming
through unchanged. The implementation works in `POW(u, 1/2)` space rather than
`FUNC sqrt(u)` space so the existing simplifier does the factor collection for
free: `sqrt(2)*sqrt(2)` collapses to `2` via the like-base merge in
`simplify_product`, `sqrt(2)+sqrt(8)` combines to `3*sqrt(2)` via `split_term`,
and `1/sqrt(2)` becomes `POW(2,-1/2)` so denominator- and radicand-
rationalization are one code path. Perfect-square extraction is trial division to
$d = 1000$ (radicands capped at $10^6$, ~0.5 ms worst case on the M0+), reusing
`math::frac::decimal_to_fraction` rather than new rational recognition.

**The probe (4D.24)** is a second side-effect-free pass in `HomeScreen::
evaluate_input`, mirroring the D30 `complexexpr` pattern: it runs *after*
`engine().evaluate()` has committed `Ans`/store/variables and can only change the
string handed to `push_entry`. Five gates decide whether the decimal is replaced —
(1) a finite, non-store numeric result and no `>dec` suffix; (2) every literal in
the *parsed* input is an integer; (3) no variables anywhere; (4) a whitelist
grammar of rational coefficients + square-free `sqrt` + `pi`, and "interesting"
(a bare integer is never upgraded); (5) agreement with the numeric result to
$10^{-9}$ relative. Gate 2 is what makes always-on safe (`2.5` stays `2.5` rather
than becoming `5/2`; `0.1+0.2` stays `0.3` rather than `3/10`; and the decimals
`convert()`/`solve()` substitution splices into the expression buffer are
rejected). Gate 3 is not optional — the CAS parser has no `ans` or `e`, so `ans`
parses as `a*n*s` and `e` as a variable while the numeric engine gives both real
values. Gate 5 makes CAS-vs-`tinyexpr` parser divergence unable to alter a
displayed answer by construction. Rendering reuses the D42 path unchanged
(serialize → `render::build_layout`, amber `kSymbolic`, right-aligned), and the
2026-08-03 history fix earlier the same day means exact forms reload as symbolic
across a reboot with no extra work.

**Layout builder** got the two changes the spec's acceptance text implies: a
single-atom radicand drops its parens (`√2`, not `√(2)`) except before a `^`
(there is no vinculum, so parens are the only grouping and `√x^2` must not read
as `sqrt(x^2)`), and a coefficient before a radical or a symbol glyph multiplies
implicitly (`2√2`, `2π`). `is_call()` was relaxed to accept the bare-radicand
HBox shape — without that, `sqrt(2)/2` stops stacking as a fraction, which is
the 2026-07-11 ASan-caught regression that made `sqrt` stay an identifier in the
first place; there is now an explicit anti-regression test for it.

**Behavior changes to watch on device**: `1/3` now shows as an amber stacked
fraction where it used to show `0.3333333333`, and `pi` shows as `π`. Both are
deliberate but visible. `>frac` results stay white flat text, so `1/3` and
`1/3>frac` look different despite meaning the same thing. Expressions naming a
variable or `Ans` never get an exact form, and non-REAL number modes get none at
all in v1 (the `force_complex` branch is not wired — a ~6-line follow-up).

**Verification**: full host suite green — `test_cas` **199 → 238** checks,
`test_layout` **44 → 54**, 0 failures across all 15 suites, and the 199
pre-existing CAS checks unchanged. Both boards build clean; Pico 1 bss
**201,096 bytes, exactly flat** (the CAS pool overlays the existing kCompute
arena, and `exact.cpp` has no statics — the only new storage is a 48-byte stack
buffer). `lint.sh` and `format.sh` clean. **Not yet flashed to either board** —
on-device confirmation folds into Stage 5, which already has both boards on the
bench for the history-persistence fix and the Pico 1's first Phase 5 build.

Files: new `src/math/cas/exact.{hpp,cpp}`; `src/apps/home_screen.cpp`
(`to_dec` flag, `rkind` threading, probe call); `src/render/layout_builder.cpp`
(`is_call`/`is_radical`/`is_symbol_glyph`, bare radicand, implicit multiply);
`tests/host/test_cas.cpp` (`test_exact_form`), `tests/host/test_layout.cpp`;
build wiring in `CMakeLists.txt` and `scripts/host-tests.sh`. Docs:
`phase5-spec.md` §10.1 amended + P5-5/P5-6 resolved + 4D.23/4D.24 ticked,
`decisions.md` D43.

---

## 2026-08-03 — bugfix: home-screen history persistence — symbolic kind lost on reload, plus two latent I/O bugs found and fixed

Root-caused and fixed the "BUG to investigate" flagged at the end of the
2026-08-02 Stage 3 session. On the `phase-5` branch.

**Root cause (the flagged bug): symbolic CAS results lost their `ResultKind`
on reboot.** `history.txt` stored only `expr<TAB>result`, and `load_state`
hardcoded `ResultKind::kPlain` for every reloaded line. `kind` drives how a
history entry *renders* (`kSymbolic` typesets 2D via `render::build_layout`
in the amber accent color; `kPlain` is one flat white text line), so a CAS
result like `x^4 / 4` or `2/3` displayed correctly as a typeset amber
fraction when entered but reverted to flat white text after a reboot — a
genuine Stage 3 regression (there were no symbolic results before Stage 3,
so plain-on-reload was previously always correct). Ruled out two scarier
theories first: the CAS serializer (`src/math/cas/serialize.cpp`) emits
only tab-free ASCII, so no delimiter corruption is possible, and the
late-init `load_state` retry (`main.cpp:431`) is guarded against
double-loading by `state_loaded`.

**Fix**: `history.txt` lines gained a third tab-separated column —
`expr<TAB>result<TAB>S|P\n` — backward compatible (a legacy two-field line
still reloads as `kPlain`). `persist_history_line` now takes a `ResultKind`
parameter; all five `evaluate_input` call sites updated (the CAS path
passes `rkind`, the four numeric paths pass `kPlain`). `load_state` parses
the optional trailing kind column and restores `kSymbolic` when present.
Also fixed a max-length boundary bug in the same function: `snprintf`'s
return value can exceed the buffer size, and using it unclamped as the
write length could drop the trailing newline on a long line — now clamped
with `std::min`, buffer bumped 256 -> 288 bytes.

**Two pre-existing latent I/O bugs found during the investigation (not
Stage 3 regressions — both predate CAS):**
- **Head read instead of tail.** `Storage::read_file` reads from offset 0,
  despite `load_state`'s own comment saying "Load the tail." Once
  `history.txt` grew past the 8 KB read window, a reboot restored the
  *oldest* 50 entries instead of the newest. Added `long
  Storage::file_size(const char*)` (via `f_stat`) to
  `src/platform/storage.{hpp,cpp}`; `load_state` now seeks to `fsize -
  8191` via the existing `read_file_range` and skips the partial first
  line so a reboot keeps the newest lines.
- **Unbounded growth.** `history.txt` was append-only and never trimmed
  (only the `clrhist` command deleted it outright). Added
  `HomeScreen::compact_history()`, called after every append: once the
  file exceeds 24576 bytes (3x the 8 KB tail) it rewrites the file down to
  its last 8 KB, line-aligned. Appends are human-paced, so the rare O(file)
  rewrite is cheap.
- Both paths now share one file-scope `g_hist_io[8192]` buffer (replacing
  `load_state`'s old function-local `static tail[8192]`), so bss stays
  flat.

**Decision**: persist the kind and restore typeset display on reload
(rather than not persisting symbolic results at all, or leaving them
permanently plain after a reboot) — user-approved during the session. This
directly satisfies **D4**'s own "Revisit when" clause ("History file size
becomes a concern, or results need structured metadata" — both fired this
session); D4 updated in place with a resolution note rather than opening a
new decision number, matching the D9 precedent for in-place resolution.

Files: `src/apps/home_screen.{hpp,cpp}`, `src/platform/storage.{hpp,cpp}`.

Verification: both boards build clean; Pico 1 bss **201,096 bytes** — flat
vs. the prior baseline (the shared buffer replaced the old static, no new
statics added). `scripts/lint.sh`/`scripts/format.sh` clean. Full host
suite green (15 suites, 0 failures; `test_cas` 199, unchanged — this path
isn't in host coverage, same as other firmware-only persistence). A
standalone host logic check of the round-trip (tail read + partial-line
skip, kind-column parse, compaction line-alignment, legacy-format
survival, missing-final-newline survival) ran 600 checks, 0 failures.

Known limitations / deferred:
- This is a firmware-only path (history persistence isn't exercised by the
  host suite), so on-device confirmation that history now survives a
  reboot correctly is still open — folds into Stage 5's Pico 1/Pico 2
  flashing rather than a dedicated bench pass.
- No decision number consumed (bugfix + latent-issue cleanup, not a new
  design call) — D4 amended in place instead (see above).

## 2026-08-02 — Phase 5 Stages 0–3: CAS engine + home-screen UI integration, HW-verified on the Pico 2

Two sessions on the `phase-5` branch (not yet merged to `main`). The first
built the full symbolic engine (Stages 0–2, host-only, no UI contact); the
second wired it into the home screen and flashed/tested it on-device
(Stage 3). Twelve commits total, `403c205`..`6c1c6c2`.

**Stage 0 — Expr tree, pool, parser, serializer (4D.1–4D.3, `403c205`).**
`src/math/cas/expr.{hpp,cpp}`: an `Expr` tree (NUM/VAR/ADD/MUL/POW/NEG/FUNC/
EQ) over an `ExprPool` bump allocator (modeled on `render::pool.hpp`,
`std::align`, reset-to-reclaim, `nullptr` on exhaustion). **D41**: the pool
is an SRAM raw-pointer arena overlaying the shared scratch `kCompute` region
(pre-Phase-5 review's arena) rather than the spec's sketched PSRAM plan —
PSRAM is offset-addressed and unusable for a pointer-native rewrite tree.
`src/math/cas/parser.{hpp,cpp}`: recursive-descent parser with CAS-mode
implicit multiplication (`2x`, `xy`, `(x+1)(x-1)`), `i` reserved as a
variable, `pi` as a nullary constant function. `src/math/cas/serialize.cpp`:
`expr_to_string` with precedence parenthesization, round-trips structurally.
`test_cas.cpp` new, 51 checks.

**Stage 1 — simplify core (4D.5–4D.7, `e5bb2f8`).** The canonical-form
simplifier every later pass depends on: bottom-up normalization in a
fixed-point loop (hard 50-pass cap, spec §13 Risk 1). Identity/annihilation
+ constant folding, like-term/like-factor collection with canonical
ordering and `i^2=-1`, fraction reduction (`2x/(4x^2)` → `1/(2x)`). Tests
98.

**Stage 2 — calculus & algebra (4D.8–4D.19, `b7c1b80`..`d7f80c0`, 5 commits).**
- **2a diff** (`derivative.{hpp,cpp}`): sum/product/power/chain rules,
  `differentiate_n` for higher orders; unknown functions return `nullptr`
  ("can't differentiate"). Tests 114.
- **2b expand**: binomial-theorem shortcut for two-term bases (the naive
  iterative multiply exhausted the ~560-node pool at `(x+1)^5`); multi-term
  bases fall back to iterative multiply-and-simplify. Tests 123.
- **2c solve + poly helper**: `poly_coeffs()` (shared with factoring);
  linear, quadratic (exact rational roots on a perfect-square discriminant,
  symbolic `sqrt` otherwise, complex roots via symbolic `i` when
  `allow_complex`), and inverse-function isolation with a small exact-value
  table (`sin(x)=1/2` → `pi/6`). `allow_complex` is a parameter, not a read
  of the global number mode, keeping the CAS engine decoupled from the UI.
  Tests 133.
- **2d factor**: rational-root theorem + synthetic division over degree
  3–4, content/lowest-power extraction; irreducible inputs return the
  expanded original. Tests 140.
- **2e integrate**: table-based with linearity, linear substitution, and
  one-level integration-by-parts (LIATE pick); `definite_integrate` uses
  the symbolic antiderivative when found, else falls back to the Phase 4B
  Gauss-Kronrod quadrature. Tests 153. (Also: portable `kPi` constant —
  newlib/arm doesn't define `M_PI`.)

**Stage 3 — UI integration (4D.4/4D.20/4D.21, `5984d2e`..`2eb0f16`, this
session).**
- **3a** (`cas_eval.{hpp,cpp}`): `math::cas::evaluate_home` recognizes a
  single inline call — `simplify()`/`expand()`/`factor()`/`diff()`/
  `integ()`/`solve()` — and dispatches to the Stage 0–2 engine; returns
  `kNone` for everything else, including `solve()` carrying a numeric
  guess/bounds (>=3 args), so `math::solveexpr` (D28) keeps owning the
  numeric-solver shape (the P5-4 shape split). Tests 196.
- **3b**: wired into `HomeScreen::evaluate_input` as the first dispatch.
  CAS is display-only — no `Ans`/store/variable commit (P5-1/P5-2), same
  precedent as complex/`MatAns`. `Entry` gained a `ResultKind`
  (plain/error/symbolic) replacing the old bool error flag; symbolic
  results typeset via `serialize` → `render::build_layout` (2D fractions/
  superscripts/`√`/`π`) in an accent color. **D42**: reuses `build_layout` on
  the serialized string instead of a dedicated `expr_to_layout`
  tree-walker (spec 4D.4) — identical visual output, no duplicate layout
  code; the one case a tree-walker could win (a big radical spanning its
  argument) is explicitly KIV.
- **3c** (`cas_menu.{hpp,cpp}`): a 6-row CAS menu (Simplify/Expand/Factor/
  d/dx/Integral/Solve) on F6 (home-screen slot 6, previously empty) and the
  typed `cas` command; selecting a row inserts the call opener into the
  input line (same insert-back pattern as `const_screen`).

**Device-testing fixes, same session (`08ebfff`, `6c1c6c2`).** Flashed to
the Pico 2 and interactively tested; three issues found and fixed in place:
1. Decimal coefficients were showing as `0.25*x^4` instead of exact
   fractions — `serialize.cpp` now renders tight rational coefficients via
   `math::frac` and splits `MUL` into numerator/denominator so
   `integ(x^3)` prints `x^4 / 4`, typeset as a real stacked fraction by the
   existing layout builder.
2. Symbolic results now right-align like numeric results (were
   left-anchored at x=4, reading as another input line).
3. Accent color changed teal → warm amber (`255,190,40`) — teal read too
   close to the gray input line.
4. Sum terms now display highest-degree-first (TI convention: `expand
   ((x+1)^3)` → `x^3 + 3x^2 + 3x + 1`, was ascending) — new
   `term_degree`/`sum_term_less` in `simplify.cpp`; only the sum-term sort
   changed, equals-based tests unaffected.
5. Long results (plain or symbolic) that overflow the line now render as a
   one-line pannable window (leading/trailing ellipses, LEFT/RIGHT scroll,
   shared `draw_result_window` helper) instead of clipping/overflowing.

Host tests: `test_cas` grew 153 → **199 checks**, 0 failures; full host
suite green. Both boards build clean; `scripts/lint.sh`/`scripts/format.sh`
clean. Pico 1 static RAM **201,096 bytes** (~67 KB headroom, essentially
unchanged from the pre-Phase-5 arena baseline — the CAS pool overlays the
shared scratch arena; the pool becoming reachable in Stage 3a only cost
+388 B). Flashed onto the Pico 2 (RP2350) and confirmed working
interactively on-device: inline CAS ops, F6 menu, fraction display,
descending sum order, amber accent, and scrollable long results all
reported "looks good."

Known limitations / deferred (per `phase5-spec.md` §11):
- **Stage 4 — exact-form display (4D.23/4D.24) not started**: `sqrt(2)`
  still shows as a decimal on the home screen, not `√2`.
- **Stage 5 — hardening (4D.22) not started**: no stress/edge-case pass
  yet, no pool-capacity abort guard (spec Risk 2, >80% capacity), Risk-1
  cycle set only exercised at the unit-test level, not at scale. Pico 1
  device flash/verification for this branch's CAS work is also still
  pending (only the Pico 2 has been flashed and tested so far).
- `PICOCALC_PHASE` in `CMakeLists.txt` stays `"4D"` — not bumped to `"5"`
  yet; that's a Stage 5 close-out task.

Decisions: D41 (ExprPool placement), D42 (result-rendering reuse) — both in
`decisions.md`. No phase/sub-phase status flip in README/ti-parity this
session (Phase 5 is in progress, not closed).

## 2026-08-02 — CI fix: red Lint/Validate-docs jobs, pinned clang-format, first release (v0.1.0)

Infra/CI session, no firmware source changed, no decision number consumed.
The GitHub Actions "Build" workflow (`.github/workflows/build.yml`) had been
failing on every push in two of its four jobs — the pico/pico2 build jobs
themselves always passed. Root-caused and fixed both, plus published the
project's first tagged release. Landed via PR #1 (squash-merged to main,
branch deleted); merge commit `e4b53ab`.

- **Lint (clang-format) — pinned tool version.** CI installed Ubuntu apt's
  `clang-format 18`; local dev uses Homebrew's `22`, so the two disagreed on
  `src/math/seq_expr.cpp:115` and `:263` (no actual source reformatting
  needed — style was already consistent with the local tool). Fix: pinned
  **`clang-format==22.1.8`** in `requirements-dev.txt` (available on PyPI,
  matches local exactly) and changed the CI Lint job to `pip install` that
  pinned version, sourced from `requirements-dev.txt` via `grep` so there's
  one place to bump it. Also changed `scripts/lint.sh` and
  `scripts/format.sh` to prefer `.venv/bin/clang-format` when present, so
  local and CI stay byte-for-byte identical even if Homebrew drifts ahead.
- **Validate docs — loose unicode math.** `docs/notes/pre-phase5-review.md`
  used `×` (U+00D7) outside math mode in 6 dimension multipliers;
  `scripts/validate_md.py` rejects that (house style: wrap math symbols in
  backticks or reword). Fixed by replacing those 6 `×` with ASCII `x`.
- **Workflow modernization + release job (not a lint/docs fix, bundled in
  the same PR):** bumped all actions to current majors (`checkout@v7`,
  `setup-python@v7`, `cache@v6`, `upload-artifact@v7`,
  `download-artifact@v8`), clearing the Node 20 deprecation warnings. Build
  now stages board-named UF2s (`picocalc_graphcalc-pico.uf2`,
  `-pico2.uf2`); a new `release` job (gated on `v*` tags, `needs: [build,
  lint, validate-docs]`, `permissions: contents: write`) downloads both and
  publishes a GitHub Release via `gh release create --generate-notes`.

**Verification:** CI fully green on the PR and on the tag run — Build
(pico), Build (pico2), Lint, and Validate docs all passing. **v0.1.0
published** (first tagged release):
<https://github.com/moodoki/graphite_picocalc_gc/releases/tag/v0.1.0>, both
UF2 assets attached (pico 826,880 bytes; pico2 788,480 bytes). This sits
during the pre-Phase-5 window (Phase 4D closed; Phase 5 CAS is next) and
doesn't change either phase's status.

## 2026-08-02 — Pre-Phase-5 review pass: shared scratch arena (−21.8 KB SRAM) + near-zero matrix chop, HW-verified

Opened the pre-Phase-5 code-review/size-optimization pass (next-session.md
"The next job" #1). Full write-up: `docs/notes/pre-phase5-review.md`.
Three commits landed:

- **`1073f4f` docs** — de-staled `AGENTS.md` (claimed "Phase 0 → Phase 1";
  now Phase 4 complete / Phase 5 next) and added "frozen, don't trust as
  current" pointers to worklog.md's two era-frozen top blocks.
- **`5f76851` perf(size): shared math scratch arena** — the headline. A
  per-symbol SRAM audit (new `scripts/size-report.sh`) showed the rough map
  under-counted: alongside ArrayStore's 57.5 KB, ~40 KB sat in per-module
  256-element PSRAM-streaming scratch buffers, one private set per module,
  never simultaneously live (single-threaded on core 0). Collapsed the
  verified-mutually-exclusive ones onto one arena (`src/math/scratch.{hpp,
  cpp}`), two disjoint regions: **kCompute** (list_expr | stats | infer |
  matops — none calls another) and **kListops** (listops, disjoint because
  list_expr calls it). Rebound by reference-aliasing so all call sites are
  unchanged; matops' RowBufs via placement-new. **Pico 1 bss 222,528 →
  200,704 (−21,824 B; headroom ~46 → ~68 KB)**; Pico 2 same. Host 1627
  green, both boards clean, lint/format clean, device-verified on Pico 2.
- **`4edba81` fix(matrix): near-zero chop** — found during the arena
  device spot-check. `[A]^-1*[A]` showed FP roundoff (2.22e-16 off-diagonals)
  as scientific noise instead of a clean identity. NOT an arithmetic bug and
  NOT the arena (all matrix ops compute correctly on-device). `format_matrix`
  now snaps a cell >~12 orders below the matrix's largest cell to `0`
  (relative, real+complex). test_matrix +6 (381), device-verified.

**Measurements banked (no code change):**
- **`-Os`/MinSizeRel: not a lever for this pass.** Pico 1 text 419,680 →
  293,488 (−126 KB, −30% flash) but **bss flat** (−32 B) — this pass targets
  SRAM, so `-Os` buys nothing here; the win is flash, which isn't
  constrained. Hot-path host bench (2M expr evals) `-O3`≈`-Os` within noise
  (dominated by double soft-float/libm). **Keep `-O3`.**
- **Phase 6 MicroPython budget re-verified — the arena makes Phase 6 fit on
  Pico 1.** Free SRAM: pre-arena 46.7 KB < the 56 KB lazy heap needs (Phase 6
  was infeasible on Pico 1); post-arena 68 KB → fits with ~12 KB spare. That
  ~12 KB must still absorb Phase 5 CAS + 6A framework static growth.

**Levers still on the table** (documented in `pre-phase5-review.md`, all
deferred — Phase 6 already fits so none is urgent): (a) reduce the MicroPython
heap 48→40 KB if the ~12 KB spare gets eaten (spec Risk 6); (b) ArrayStore
slab cut — ~12-16 KB safe (needs a device high-water-mark measure first),
~24-32 KB if a **PSRAM-fallback-on-slab-exhaustion** prerequisite lands (today
`slab_alloc` hard-fails, no fallback); (c) fold the persistence `g_chunk`s
(~6 KB, minor); (d) the arena's debug owner-guard (deferred — placement risk
> value given host coverage). No decision number consumed (measurement/trim,
not a design call).

## 2026-08-02 — UI-friction polish: matrix precision/`>Frac`, constants-picker relayout + description scroll, HW-verified

Addressed the two UI-friction feature requests logged in the 2026-07-27 eval
(matrix decimal precision, constants-picker readability) and two follow-ups
raised during this session's on-device testing (`>Frac` on matrices, a
constants-picker description-scroll gap left by the relayout). All four
HW-verified on the Pico 2 (flashed `build/pico2` at each step; clean
sustained boots, no faults). Both boards build clean; host suite green;
`clang-format`/`clang-tidy` clean. **No decision number consumed** — these
are UI polish inside the already-closed Phase 4D, not new design calls
(tracked as "The next job" #1's two feature requests, not a phase item).

**1. Matrix result precision (home-screen inline).** `format_matrix`
(`src/math/mat_expr.cpp`) now formats cells with the compact formatter —
real cells via `format_number_compact` (4 sig figs; integers/sci
unchanged), complex cells via a new `format_complex_compact`
(`src/math/format.cpp`/`.hpp`). `format_complex`/`format_complex_compact`
share one `format_complex_impl` taking a per-component formatter, so the
layout logic isn't duplicated. `[[1.4142135624,...]]` now shows
`[[1.414,...]]`.

**2. Constants picker readability (`src/apps/const_screen.cpp`).** The old
two-column layout (value + right-aligned summary) overprinted for long
values like `hbar`. Replaced with four fixed non-overlapping columns:
symbol (green) | engine id (the ENTER-insert token) | short value |
summary (truncated with `math::kEllipsisGlyph`). Added a local
`format_value_short` (~5 sig figs incl. the sci range, e.g. `1.0546e-34` —
`format_number_compact` wasn't usable here because it keeps full precision
in the sci range, exactly where the picker's longest values live) and a
`fit_text` truncate-to-width helper. Surfaced the previously-unused
`symbol` field from `ConstDescriptor` (`math::catalog`).

**3. Follow-up: `>Frac` now works on matrices.** In `src/apps/home_screen.cpp`
the `>frac`/`>dec` suffix detection was hoisted into a `to_frac` flag so the
matrix path formats cells as fractions via a new
`math::matexpr::format_matrix_frac` (`mat_expr.cpp`/`.hpp`) when set; the
scalar `>frac` handling was relocated after the list path (fires only when
the expr wasn't matrix/list syntax). `format_matrix` was refactored into a
shared `format_matrix_impl` taking per-cell formatters (compact vs.
fraction). Real matrix cells become `p/q` (den `<= 10000`) with a
compact-decimal fallback; complex cells keep the compact form. `mat_expr.cpp`
now includes `math/frac.hpp`; `scripts/host-tests.sh` gained
`src/math/frac.cpp` on the `test_matrix` link line.

**4. Follow-up: constants picker left/right description scroll.** Added a
`desc_scroll_` field (`const_screen.hpp`) so LEFT/RIGHT horizontally scroll
the selected row's (truncated) summary; resets on up/down and
`on_activate`. Right-scroll stops as soon as the remaining tail fits fully
in the column (`font.text_width` vs. the column's pixel width — not "until
the last char is left-aligned"). Hint bar updated to
`ENTER:INSERT <>:DESC ESC:BACK`.

**Tests**: host suite green — `test_math` now **230 checks** (added
`format_number_compact` cases), `test_matrix` now **375 checks** (added
compact real+complex cell checks and a `format_matrix_frac` check), 0
failures across all 12 suites.

**Build**: both boards build clean; Pico 1 bss **222,528 bytes** (was
222,520 baseline — +8 from the `desc_scroll_` field, alignment included).
`scripts/lint.sh` and `scripts/format.sh` both clean.

Files touched: `src/math/format.cpp`, `src/math/format.hpp`,
`src/math/mat_expr.cpp`, `src/math/mat_expr.hpp`,
`src/apps/home_screen.cpp`, `src/apps/const_screen.cpp`,
`src/apps/const_screen.hpp`, `scripts/host-tests.sh`,
`tests/host/test_math.cpp`, `tests/host/test_matrix.cpp`.

---

## 2026-08-02 — Phase 4D CLOSED: F sequencing + idea H disposition, ti-parity/README flip (D40)

Docs-only session, no source changes. Resolved the three-item Phase 4D
close checklist carried in `next-session.md`:

1. **F-evaluator follow-on check (D37): trigger fired.** Idea B (complex
   variables/Ans, 4D.15), C (complex lists, 4D.24), D (complex matrices,
   4D.25), E (vector ops, 4D.22 + the list↔matrix bridge 4D.12), and G
   (eigenvectors, 4D.23) have all shipped and are HW-verified within Phase
   4D. F itself remains committed per D37, but its sequencing is now
   decided (**D40**): F happens **after** Phase 5 (CAS) — pre-Phase-5
   code-review/size-optimization pass → Phase 5 (CAS) → F.
2. **Idea H (polymorphic variables): deferred again (D40).** TI's three
   separate namespaces (`A`-`Z` scalars, `[A]`-`[J]` matrices,
   `l1`-`l6`/named lists) stay as-is; H remains unscheduled, revisit only
   if real usage demands it, re-checkpointing after F.
3. **`ti-parity.md` and `README.md` flipped to reflect Phase 4D as shipped
   and HW-verified.** All nine 4D batches are HW-verified on the Pico 1
   (2026-07-26/27); the Pico 2 leg is closed as a formality (board-
   independent logic, same precedent as the earlier Phase 3/4A-4C rows).
   Flipped the rows previously marked "🟡 planned: Phase 4D" (scientific
   constants 4D.17, unit conversions 4D.18, `▶Frac` 4D.2, sequence graphing
   4D.6-8, list↔matrix conversion 4D.12, home-screen matrix literals
   4D.14, complex-valued variables 4D.15) plus several additional stale
   rows found during the sweep: ENG display mode (4D.1), zoom
   ZBox/ZDecimal/ZSquare (4D.9-10), curve/band shading (4D.11), named
   lists (4D.13), complex-valued lists (4D.24), complex-valued matrices
   (4D.25), matrix eigenvectors (4D.23), and the xyLine/normal-probability
   stat plots row (already shipped pre-4D per D38's zero-work 4D.16
   closure, but the ti-parity row itself hadn't been updated until now).
   README's Status blurb, the Phase 4 Features bullet, and the
   Project-status table row all now describe 4D as complete and
   HW-verified rather than "code-complete, evals pending".

**Phase 4D is now CLOSED.** The forward path: (1) a pre-Phase-5 code-review
+ size-optimization pass (`size-optimization-ideas.md`), (2) Phase 5 (CAS)
per D32/D33, (3) F (unified evaluator) after CAS, (4) idea H
revisit-after-F. Full detail: `decisions.md` D40 (cross-refs D37,
`design-departures-matrix-complex.md` §H).

---

## 2026-08-02 — D10 leg A: dual-core display pipeline extended to the Pico 2, HW-verified

Closes the non-blocking D10 follow-up item "extend the display pipeline to
Pico 2" (open since the 2026-07-25 D10 root-cause/fix session). Only
`src/gfx/framebuffer.cpp` touched.

**What changed**: `start_display_service()` now launches the core-1
display service on both boards — was gated Pico-1-only via
`if constexpr (!config::kUseFullFramebuffer)`; the existing
`service_running` latch still prevents a double launch. The Pico 2
full-framebuffer path in `render_frame()` now hands its band push to core
1 **asynchronously** through the existing `submit`/`drain_acks` machinery
instead of a blocking synchronous `push_rect` on core 0. `frame_buf` is a
single buffer, so each frame calls `drain_acks()` first to wait for the
previous frame's push to finish before reusing it; a synchronous
`push_rect` fallback remains for the pre-service boot window
(`service_running == false`). No new static state (bss unchanged);
`push_rect_dma` already chunks internally (4 scanlines/staging buffer), so
a full 320x320 band needs no large allocation, and core 1 was otherwise
idle on both boards.

**Why**: on the Pico 2 the ~146 ms full-frame SPI push was blocking core
0; routing it to core 1 lets core 0 return to the event loop, so input
polling and the *next* frame's compute overlap the push — the same win
the Pico 1 strip pipeline already had, plus the Pico 1 pipeline can't hide
a full-frame compute-bound redraw the way this can when compute overlaps
the previous push.

**HW verification (RP2350, on top of `e5f2a10-dev`)**: the D10 root cause
this leg carried over from the Pico 1 fix — core 1 hard-faulting from XIP
while core 0's USB stack is active (chip wedge, USB drop) — had never
been exercised on the RP2350 before this session. Flashed and confirmed
sustained boot with USB enumerated the whole window, steady core-0
heartbeats, and `graph recompute:`-triggered core-1 pushes with no
wedge/fault/USB-drop. Developer then did an interactive pass — rapid
screen navigation, fast typing, graph pan/zoom under key-repeat — and
reported it clean: no tearing, no corruption, no freeze.

**Tests / build**: both `build/pico` and `build/pico2` build clean; full
host test suite green (`./scripts/host-tests.sh`, 12 suites, 0 failures —
the multicore TU isn't in the host build, so host-side behavior is
unchanged). `scripts/lint.sh`/`scripts/format.sh` clean.

**Known limitation, explicitly deferred**: D10 leg B — parallelizing
`GraphScreen::recompute_function` onto a second engine/vars context for
compute-bound screens (render > ~146 ms push budget) — remains open; this
session only addressed the push-offload leg (A). See `decisions.md` D10
for the updated leg A/B breakdown.

Files touched: `src/gfx/framebuffer.cpp`.

## 2026-08-02 — feature follow-on: MatAns now persists across a power cycle (D39), HW-verified

Same-day follow-on to the bugfix session below. That session's stale-doc
pass reclassified "MatAns doesn't survive a power cycle" as by-design (a
transient global, never written to SD). The developer then decided that
stance should be reversed: MatAns should persist like the named matrices
`[A]..[J]` do. Implemented and HW-verified same day.

**What changed**: `matrices_persist.cpp`'s single-matrix save/load was
refactored from index-specific `save_matrix`/`load_matrix` (file-static,
`namespace {}`) into path-based `save_matrix_file`/`load_matrix_file`
(declared in `matrix.hpp`, shared by `MatrixStore` and MatAns — no format
duplication). `math::matexpr::save_ans`/`load_ans` (declared in
`mat_expr.hpp`, defined in the firmware-only `matrices_persist.cpp` so the
host build stays storage-free) write/read MatAns to its own
`/picocalc/matans.dat`, same PCM2 header/element format as `[A]..[J]`. A
new `mat_ans_mutable()` accessor (`mat_expr.cpp`) gives the persistence TU
write access to the `g_mresult` global that `mat_ans()` exposes read-only
elsewhere. `HomeScreen::evaluate_input` (`home_screen.cpp`) calls
`save_ans` after any matrix-result commit; `main.cpp` calls `load_ans` at
boot and retries it in the late-init loop (same D14 cold-boot contract as
the named-matrix load — `printf("late-init: matans loaded ...")` on a
delayed success). A `g_ans_loaded` latch (mirrors `MatrixStore::loaded_`)
stops a late-init retry from clobbering an in-session result. Decision
**D39** records the rationale (parity with `[A]..[J]`/scalar `ans`) and the
one-file-per-store mechanism.

**Tests / build**: full host suite green, `test_matrix` unchanged at **369
checks** — no new host-side coverage was added for this feature (the
persistence path is firmware-only, gated behind `platform::Storage`, same
as `MatrixStore`'s own save/load, which is likewise HW-verified rather than
host-tested). Both `build/pico` and `build/pico2` link clean; Pico 1 bss
unchanged at 222,520 bytes. `clang-format` clean.

**HW verification**: cold-boot verified on the Pico 2 (`e5f2a10-dev` plus
this change) — a matrix result set into MatAns survived a physical
power-off/on and was restored from SD on the next boot.

**Doc reconciliation**: the "MatAns is by-design transient, not a bug"
language added by the bugfix session below (and the HW-PENDING Batch 5 row
above) is now superseded — amended in place there rather than rewritten,
since it was accurate when written; `next-session.md`'s forward-looking
MatAns text is updated to describe the new persisted behavior.

Files touched: `src/math/matrices_persist.cpp`, `src/math/matrix.hpp`,
`src/math/mat_expr.{hpp,cpp}`, `src/apps/home_screen.cpp`, `src/main.cpp`.

## 2026-08-02 — bugfix session: SEQ trace float noise + SEQ editor recursive-row color, both HW-verified; stale-doc corrections

Fixed the two minor bugs found during the 2026-07-27 eval (SEQ-mode trace,
sequence editor color swatch), flashed to the Pico 2, and confirmed both
fixed on-device (build `e5f2a10-dev`). Also corrected three stale claims in
`next-session.md` left over from earlier sessions (below). No decision
number consumed — these are bug fixes inside the already-code-complete
Phase 4D, not new design calls.

**Fix 1 — SEQ-mode trace (F4) float noise.** The trace readout
(`GraphScreen::draw_trace`, `src/apps/graph_screen.cpp`) read x/y back from
the pixel-quantized point cache, so sequence values showed float noise
(e.g. `4.9999997`) instead of the exact integers the table (F5) shows for
the same points. Fix: read exact values straight from
`math::seqexpr::value()` instead, with a NaN guard that falls back to the
cached readout if the evaluator isn't primed. Covers **both** seq plot
styles: TIME (`(n, u(n))`) and WEB cobweb vertices (`(u(k-1), u(k))` /
`(u(k), u(k))` — two cache points per step `k`). Notable in-session
discovery: the first cut only handled TIME and looked done on a quick
desk-check, but the actual test board's seq plot style was WEB — which
persists in `GraphState`/PCG5 across the format-version resets that have
happened since, so it silently carried over from whatever it was last set
to. The readout stayed noisy on-device until the WEB branch was added.
Lesson for future seq work: plot style is sticky across sessions/reboots —
test both styles explicitly, don't assume the default.

**Fix 2 — SEQ editor drew every recursive row red.** `SlotEditorScreen`
(`src/apps/slot_editor.{hpp,cpp}`) validated row text by compiling it
against the plain engine to decide white-vs-red; the plain engine can't
resolve `u(n-1)`-style self-references, so *every* recurrence rendered red
("broken") and only explicit forms (computed directly from `n`, no
self-ref) showed white — this was the "color swatch tracks
recursive-vs-explicit form" symptom from the 2026-07-27 eval notes; the
graph itself always plotted correctly regardless of the swatch color. Fix:
added a stateless `math::seqexpr::compiles(const char*)`
(`src/math/seq_expr.{hpp,cpp}`) that runs the same lag-rewrite + engine
compile `begin()` does, but with **no** iterator/compile-state side
effects (compiles into a scratch `SeqState`, frees the handle immediately)
— so the editor can validate a row without disturbing a live sweep. Added
a `SlotEditorScreen::field_valid(int, const char*)` virtual hook (default:
plain-engine compile, i.e. unchanged behavior for Y=/param/polar editors)
and had `render()` call it instead of the old free function directly.
`SeqEditorScreen` (`src/apps/seq_editor.{hpp,cpp}`) overrides the hook to
call `seqexpr::compiles` for the `u/v/w(n)=` expression rows; the
`nMin`/seed rows (re-formatted numerics, not expressions) are always
valid.

**Stale-doc corrections** (already applied to `next-session.md`; recorded
here for the record, not new findings this session): the home-screen
`MatAns` token was listed in an old watch-item as an open Pico 1 gap, but
actually shipped as **4D.14** (`matans` expression token,
`mat_expr.cpp:591`) — corrected in both `next-session.md` and the "Open
design threads" backlog paragraph above. "fnInt shading should follow
curve color" was similarly listed as an open feature request but shipped
as **4D.11** (`function_color_dim(s.slot)`, `graph_screen.cpp:1146`). The
`MatAns` "doesn't survive a power cycle" bug (2026-07-27) and its Pico 2
board-to-board discrepancy (2026-08-02) are reclassified as **by-design**:
`mat_ans()` is a transient global (`g_mresult`, `mat_expr.cpp:27`) never
written to SD, so the Pico 1's empty-after-boot reading is correct and the
Pico 2's "persisted" reading was warm-reset RAM retention, not a source
bug — see the HW-PENDING table's Batch 5 row above, now amended to match.

**Tests**: 12 new checks in `tests/host/test_seq.cpp`
(`test_compiles_validator`) — recursive/lag/cross-ref forms valid, circular
(`u(n)`), malformed lag (`u(n-3)`), unknown-function, and garbage forms
invalid, empty text valid, and confirmation the stateless check doesn't
disturb a live compiled sweep. Full host suite green, `test_seq` now **63
checks** (was 51); no other suite's count changed.

**Build**: both `build/pico` and `build/pico2` rebuilt clean;
`clang-format` clean (no reformatting needed). Pico 1 bss unchanged at
222,520 bytes — no new static state (the validator is a stack-only scratch
compile).

Files touched: `src/apps/graph_screen.cpp`, `src/math/seq_expr.{hpp,cpp}`,
`src/apps/slot_editor.{hpp,cpp}`, `src/apps/seq_editor.{hpp,cpp}`,
`tests/host/test_seq.cpp`.

## 2026-07-27 — on-device eval: 4D Batches 2-4 all PASS — Phase 4D on-device eval backlog now clear

Observations-only test-drive session on the Pico 1 (current Phase 4D build; no
code changes). Ran the full checklist for the three remaining open HW-PENDING
rows: **Batch 2** (complex matrices, 4D.25), **Batch 3** (sequence graphing,
4D.6-8), **Batch 4** (zoom + shading, 4D.9-11). All passed; Batch 3 turned up
two new minor bugs (below). Also re-confirmed the Batch 7 `π` tick-label fix
renders correctly (real glyph, not blank) on this build.

**Batch 2 (complex matrices)**: no findings. Editor a+bi/polar migration,
REAL-mode "Non-real result" guard, `det`/`rref`/`ref`/`[A]^-1`/`rank`/
`transpose`/`augment`/`[A]^2`, scalar `i*[B]` multiplication, mixed-matrix
add, element read, complex scalar store, and power-cycle persistence all
checked out. (A `ref([A])` result that looked wrong mid-session was a
data-entry mistake on the interviewer's side, not a device bug.)

**Batch 3 (sequence graphing)**: overall pass — MODE cycling, basic ramp,
Fibonacci table, cross-referenced sequences, window/ZoomFit, WEB-mode cobweb
convergence, bad-form error cases, F6 CALC no-op all confirmed — but two new
bugs, neither root-caused or fixed yet:

1. **SEQ-mode trace doesn't snap to exact values.** F4 trace on a sequence
   (e.g. the basic ramp `u(n)=u(n-1)+1`) shows x/y with float noise instead
   of the exact integers the F5 table shows for the same points.
2. **Sequence editor color swatch tracks recursive-vs-explicit form, not the
   assigned plot color.** In the sequence editor list, a recursive definition
   (references `u(n-1)`/`u(n-2)`) always shows its swatch red; an explicit
   one (computed directly from `n`, no self-reference) always shows white —
   regardless of the color actually assigned. The graph itself still plots
   in the correct color either way.

**Batch 5 (data & catalog glue, spot-checked in passing)**: one bug found —
`MatAns` does not survive a power cycle, unlike named matrix variables
(`[A]` etc.), which Batch 2 confirmed persist correctly.

**Batch 4 (zoom + shading)**: no findings. ZDecimal, ZSquare, ZBox
(crosshair/rubber-band/Alt-move/ESC-cancel), per-function shade toggle with
persistence, two-curve `H` band shading, and fnInt shaded-region color all
checked out.

Two UI-friction feature requests were also raised (not bugs, no fix yet):
matrix results with many decimal places are hard to read (cap displayed
digits?); multi-character constant names are hard to read in the constants
picker (flagged "kiv" for future design thought). Two previously-logged open
items were re-checked and remain unchanged (not new): 2-Var stats `Sx`/`Sy`
still not subscripted; the `const` screen's numeric-value/description text
overlap.

Full verbatim report: `docs/notes/testdrive-2026-07-27-observations.md`
(committed `a8fed7b`).

**Net effect on the Phase 4D on-device eval backlog**: with Batches 2-4
cleared today (joining Batch 1 and Batches 5-9, all cleared 2026-07-26), **all
nine D38 batches are now hardware-verified on the Pico 1** — see the updated
table rows above. Phase 4D itself isn't closed yet: the close-out follow-ons
(F-evaluator check per D37, idea H revisit, `ti-parity.md` + README status
update) are still outstanding — see `next-session.md`.

## 2026-07-26 — eval fix: diag-screen phase label was stale ("Phase 4C")

Second eval observation: the diag screen still said "Phase 4C". The phase
can't be derived from git like the hash/dev suffix, so it's now a single
source of truth next to the build-id block: `set(PICOCALC_PHASE "4D")` in
CMakeLists.txt, injected as a define and printed as "Phase %s [%s]" in
main.cpp. The session-wrapup agent's README status pass now also bumps it
when a phase/sub-phase goes code-complete, so it shouldn't drift again.

## 2026-07-26 — eval fix: `π` tick labels were blank (small font had no `π` glyph)

First Batch 7 eval observation: ZTrig tick labels showed `/2` instead of
`π/2`. Root cause: tick labels draw with `gfx::small_font()` (Spleen 5x8,
ASCII 32..126 only) but `kGlyphPi` is slot 127 — one past the end, so
`draw_char` silently drew nothing. The 8x16 mathglyphs pipeline never
covered the small font ("`π` has always been 8x16-only" per gen-fonts.sh).

Fix (two rounds, same day): first a hand-drawn 5x8 `π` via `--extra`; then
reworked per developer preference to a **copied glyph** like the 8x16
fonts use — new `bdf_to_utft.py --donor FILE` (fallback BDF for --map
codepoints the primary lacks, cell size must match) sourcing `π` (U+03C0)
from Markus Kuhn's public-domain X11 fixed 5x8, vendored at
`drivers/ucs-fixed/5x8.bdf` (vendored, not fetched, so the default-font
regen stays offline; both fonts are 5x8/ascent-7 so it bakes unshifted).
gen-fonts.sh 5x8 line: `--last 127 --map 127:960 --donor ...`; the
hand-drawn mathglyphs-5x8.txt is gone. spleen5x8.h regenerated (8x16
byte-identical). Host suite + lint green; flashed to the Pico 1, boot
verified. Re-check the `π` ticks in the Batch 7 eval row with this build.

## 2026-07-26 — 4D Batch 9 shipped: device polish (4D.19-20) — **Phase 4D CODE-COMPLETE**

Ninth and final D38 batch, same day. **All of Phase 4D is now
code-complete** (25 tasks across 9 batches; on-device evals pending —
see the HW-PENDING table, Batches 2-9). Host suite green (15 binaries,
1740 checks); lint + format clean; both boards build; **flashed to the
Pico 1** and boot-verified. Pico 1 bss **222,520** (+36).

1. **Soft-sleep APD (4D.19)**: new `platform/power.{hpp,cpp}` —
   inactivity timer fed by the main loop's key drain
   (`power::note_key`), timeout dims LCD + keyboard backlights to 0;
   any key wakes and is **swallowed** (the wake key must not type).
   No deep sleep in v1 (core-1 display service + tinyusb risk, D38);
   DISPOFF also deferred — backlight-only sleep. **All STM32 writes go
   through a paced queue in `power::tick()`**: one register write per
   main-loop pass, `≥250 ms` apart, only while `Keyboard::bus_idle()` —
   the battery_poll discipline (a wedged STM32 needs a physical power
   cycle).
2. **Brightness settings (4D.20)**: new `apps/settings_screen` (typed
   `settings`/`setup`): LCD brightness (STM32 reg 0x05, the Phase-1
   verified path), keyboard backlight (vendored `set_kbd_backlight`,
   reg 0x0A — first use), APD timeout OFF/1/2/5/10/30 min. Every change
   applies through the paced queue and persists.
3. **Persistence**: `/picocalc/settings.dat`, magic **PCS1** (lcd, kbd,
   apd). A missing/old file keeps the STM32's own boot defaults
   untouched (no surprise brightness jump on first boot); load hooks
   boot + the D14 late-init retry (`late-init: settings loaded`).

Phase 4D closing follow-ups (per the D38 plan): the F-evaluator
follow-on check (D37) and the idea-H (polymorphic variables) revisit
are now due; Phase 5 (CAS) follows per D32/D33.

## 2026-07-26 — 4D Batch 8 shipped: matrix eigenvectors (4D.23)

Eighth D38 batch, same day. Host suite green — 15 binaries, **1740
checks** (test_matrix 339→369); lint + format clean; both boards build;
**flashed to the Pico 1** and boot-verified. Pico 1 bss **222,484**
(unchanged).

`matops::eigenvectors` (D38/P4-13): real eigenvalues from the existing
QR core, then per eigenvalue the **rref nullspace of `A − λI`** — a
looser 1e-8-relative pivot epsilon absorbs the QR error in `λ`
(`rref_t` now takes the epsilon; `rref()` passes its usual
1e-12-relative one). Exactly one free column expected; unit-normalized
with a deterministic sign (largest-magnitude component positive),
columns in the eigenvalues' descending order. **Distinct real spectra
only**: repeated eigenvalues error "No unique eigenvector" (whole
eigenspace or defective — either way no canonical answer), complex
pairs error "Complex eigenvalues", complex matrices "Non-real matrix"
(D36/D37). Exposed as composable `eigenvec([A])` in matexpr + catalog.
Tests: diagonal/symmetric known vectors, a `3×3` verified via `A·v = λ·v`,
Jordan-block + identity + rotation refusals, matexpr composition.

## 2026-07-26 — 4D Batch 7 shipped: display & formatting (4D.1-5)

Seventh D38 batch, same day. Host suite green — 15 binaries, **1709
checks** (test_math 204→221, test_layout 46→47); lint + format clean;
both boards build; **flashed to the Pico 1** and boot-verified. Pico 1
bss **222,484** (unchanged — all flash/text). All five font headers
regenerated (slot 141 added).

1. **ENG display mode (4D.1)**: `DisplayMode::kEng` — exponent snapped
   to multiples of 3, mantissa in [1,1000) via log10 with edge
   renormalization; MODE Display row cycles 4 modes. Persists through
   the existing GraphState display field (same width, no PCG bump).
2. **▶Frac (4D.2)**: new `math/frac.{hpp,cpp}` — bounded
   continued-fraction `decimal_to_fraction` (`den ≤ 10000`, ~1e-9
   relative tolerance so irrationals refuse), `format_fraction`,
   `pi_multiple`. Home-screen `>frac` postfix evaluates the scalar and
   shows `p/q` (decimal fallback when no tight fraction); `>dec`
   strips to the default display. Ans commits as usual.
3. **`π` tick labels (4D.3)**: `tick_label` detects small rational
   multiples of `π` (`|p| ≤ 12`, `q ≤ 6`) and renders `π/2`-style labels
   with the real `π` glyph — ZTrig's grid finally reads properly
   (the 2026-07-18 KIV).
4. **Subscript-x glyph (4D.4)**: slot 141 (U+2093) baked into all five
   fonts — hand-drawn for Spleen (mathglyphs-8x16.txt), Unifont-sourced
   for Terminus/Unifont, TTF-rendered for JuliaMono/Iosevka; all gen
   scripts updated to `--last 141` and **all five headers regenerated**
   (the four non-Spleen scripts fetch their font sources).
   `gfx::kGlyphSubX`; the stats 1-Var output now shows `Sₓ`/`σₓ` (2-Var
   keeps plain x/y pairs — Unicode has no subscript-y). The optional
   `NodeType::kSubscript` pretty-math node stays future work.
5. **Fraction centering (4D.5)**: `make_fraction` now places the bar
   half a char above the node baseline, so it centers on the midline of
   baseline-aligned text siblings instead of sitting on the text's
   bottom row (the whole stack hung too low). HBox ascent/descent
   bookkeeping absorbs the raised baseline; test_layout locks it.

## 2026-07-26 — 4D Batch 6 shipped: named lists (4D.13, P4-10 full integration)

Sixth D38 batch, same day. Host suite green — 15 binaries, **1691
checks** (test_lists 211→239); lint + format clean; both boards build;
**flashed to the Pico 1** and boot-verified. Pico 1 bss **222,484**
(+2,052 — the 20-slot registry, its persist index, editor state),
~48 KB headroom.

1. **Registry** (`math/named_lists.{hpp,cpp}`): up to 20 named lists
   (2-5 chars, lowercase letter-first; `valid_name` rejects catalog
   functions, constants, reserved identifiers — `ans`, `matans`, lift
   operand names, seq lag placeholders — and the `l<digit>` namespace).
   Slots are Arrays (complex dtype rides the 4D.24 tier); unified
   **refs** (0-5 = l1-l6, 6+k = named slot k) via `list_by_ref` /
   `list_ref_name`.
2. **Expression integration** (`list_expr.cpp`): named tokens ride the
   existing operand-extraction seam (a named token extracts into a lift
   operand slot like a brace literal — the whole lift/complex-lift
   machinery works unchanged, `≤4` list terms per expression as before);
   bare recall, reductions, dot/cross/norm, in-place sorts all resolve
   named refs; **store-to-name creates the list at commit time**
   (`{1,2}->costs`; deferred so a failing expression leaves no stray),
   full registry errors "Too many named lists".
   `Result` gains `lists_mask` (every ref written) + `names_modified` —
   which also **fixes a D35 bug**: plain `sort_asc(l1)` set
   `lists_modified` with `stored_list = -1`, so the home screen called
   `lists().save(storage, -1)` — an out-of-bounds `lists_[-1]` read
   writing a garbage file, and the actual sort never persisted.
3. **List editor**: columns extend past l1-l6 into the named lists
   (headers show names); Alt+N/Alt+R/Alt+X create/rename/delete with a
   name prompt on the entry line; saves route by ref.
4. **Stats + plots**: stats List/X/Y/Freq selection cycles named lists
   (labels + result headers show names); `StatPlotConfig.x_list/y_list`
   now store refs (uint8 values 6+ = named — no GraphState layout
   change, so **no PCG bump**; stale refs render empty, out-of-range
   clamps); `stat_plot::list_of` resolves refs.
5. **Persistence** (`named_lists_persist.cpp`): `/picocalc/listdir.dat`
   index (magic **PCN1**: used flags + names) + `/picocalc/nlist<idx>.dat`
   per slot (PCL2-shaped header, complex payloads supported); files are
   keyed by slot so renames only rewrite the index; boot + late-init
   retry hooks in main.cpp (`late-init: named lists loaded`).
6. Help gains a NAMED LISTS section; matexpr's list interop
   (`->lk` targets, list2mat/mat2list) stays l1-l6-only in v1.

## 2026-07-26 — 4D Batch 5 shipped: data & catalog glue (4D.12/14/17/18/22 + MatAns)

Fifth D38 batch, same day. Host suite green — **15 binaries** (new
`test_units`, 32 checks), **1635 checks** (test_matrix 306→339,
test_lists 196→211, test_math 198→204 via the catalog rows); lint +
format clean; both boards build; **flashed to the Pico 1** and
boot-verified. Pico 1 bss **220,432** (+1,060 — const picker, matexpr
literal scratch, listexpr vector temps), ~50 KB headroom.

1. **Matrix literals + MatAns (4D.14 + D38 scope add)**: matexpr parses
   `[[1,2][3,4]]` (rows of full scalar expressions, ragged rows error,
   complex elements make a complex matrix, 64-element cap) into an
   expression temp, and `matans` as a typed token for the last matrix
   result — both compose (`2*matans`, `det([[...]])`, stores). The
   dispatch gate widened accordingly (`[[`, `matans`, the new calls).
2. **List↔matrix bridge (4D.12)**: `list2mat(l1,...)` packs lists into
   matrix columns (zero-pads to the longest; composes as a matrix
   value), `mat2list([A], l1, ...)` unpacks columns into list targets
   (whole-expression, side-effecting; new `Result.lists_mask` tells the
   home screen which lists to persist; "Done (n lists)" text result).
3. **Vector ops (4D.22)**: listexpr whole-expression `dot(A,B)`,
   `cross(A,B)` (3-element, storable list result), `norm(A)` over any
   list-valued expressions (real-only v1, "Non-real list"); matexpr
   `norm([A])` = Frobenius via new `matops::norm_f` (complex OK — the
   norm is real). Dispatch keeps them apart: `norm([A])` hits matexpr,
   `norm(l1)` falls through to listexpr.
4. **Scientific constants (4D.17)**: 16-entry `ConstDescriptor` table
   in the catalog TU (CODATA values; multi-char names — `clight`,
   `navo`, `boltz`, `qe`, `eps0`, … — so a-z user variables can't be
   shadowed), engine-bound as TE_VARIABLEs in `build_lookup`
   (kLookupCount grew by kMaxConstants); new `apps/const_screen`
   picker (typed `const`) that inserts the identifier into the home
   input via the new `HomeScreen::insert_text`.
5. **Unit conversions (4D.18, D38: typed only)**: new `math/units.*` —
   ~60-unit table across 11 families (length/mass/time/speed/area/
   volume/temp/energy/power/pressure/force, offset-aware temperature),
   `convert(value, from, to)` with optional quotes and case-insensitive
   unit names, spliced to a numeric literal pre-engine
   (`unitexpr::substitute`, solveexpr's pattern) so it composes and
   stores like any literal.
6. Catalog gains 6 help-only rows (dot/cross/norm/convert/list2mat/
   mat2list; 68 of 72 entries used). New `tests/host/test_units.cpp`.

## 2026-07-26 — 4D Batch 4 shipped: zoom presets + shading (4D.9-11)

Fourth D38 batch, same day. Host suite green (14 binaries, 1429 checks
— no new host-testable math; zoom/shading is view-layer); lint + format
clean; both boards build; **flashed to the Pico 1** and boot-verified.
Pico 1 bss **219,372** (+40 — the ZBox/shade session members). No
persistence bump: shade_mode[7] was reserved in Batch 3's PCG6 (D38).

1. **ZDecimal/ZSquare (4D.10)**: `zoom_decimal(w,h)` (0.1 units/pixel,
   origin-centered, matched to Viewport's x-over-width-1 / y-over-height
   mapping) and `zoom_square(w,h)` (y refit about its center so units
   are square) in graph_model; graph keys 'D'/'Q', pane-aware.
2. **ZBox (4D.9)**: `ZBoxSession` on GraphScreen (AnalysisSession-shaped
   modal state): free 2-D crosshair (arrows, Alt = 10 px), first ENTER
   commits corner 1 (rubber-band rect), second ENTER maps both corners
   through the viewport into the window (min 4 px box, inversion-proof);
   ESC cancels; prompt rides the readout strip. Pure state — render only
   draws (§8).
3. **Inequality shading (4D.11)**: per-Y-slot `shade_mode` (0/above/
   below) persisted in the reserved PCG6 fields; cycled with **'S' in
   the Y= editor** (new SlotEditorScreen `field_key`/`field_marker`/
   `softkey_text` hooks; '^'/'v' marker by the checkbox). `draw_shades`
   column-fills from the plot_y_ cache beneath the curves in the slot's
   dimmed palette color (`function_color_dim`, new ~40% palette).
4. **Shade(lower,upper) (4D.11)**: 'H' on the graph starts a two-pick
   flow (UP/DOWN cycles active curves — candidate drawn thick via the
   existing PlotStyle.thick — ENTER commits each); the band between the
   two column caches fills in the lower curve's dim color; transient
   (TI ClrDraw-style), 'H' toggles off, mode switch drops it.
5. **fnInt shading in curve color** (scope add, D38): the CALC-integral
   fill uses `function_color_dim(slot)` instead of fixed blue.
6. Slot-editor label buffer widened 8→12 (the seq editor's "u(nMin)="
   label was silently truncated — caught during this batch, Batch 3
   bugfix). Help gains the D/Q/B/H/'S' rows.

## 2026-07-26 — 4D Batch 3 shipped: sequence graphing (4D.6-8), PCG6

Third D38 batch, same day. Host suite green across **14 binaries** (new
`test_seq`, 51 checks — **1429 total**); lint + format clean; both
boards build; **flashed to the Pico 1** and boot-verified over serial.
Pico 1 bss **219,332** (+1,176 over Batch 2 — SeqFunctions in
GraphState, doubled by the persistence image mirror), ~51 KB headroom.

1. **`math::seqexpr`** (new `math/seq_expr.{hpp,cpp}`): u/v/w
   recurrences per D38/P4-12 — full cross-reference at n-1/n-2 lags.
   Lag references can't ride tinyexpr, so `begin()` **rewrites**
   `u(n-1)` → placeholder variables (`u1`…`w2`) bound to memo storage
   via the existing `Engine::compile_with` extras seam, and compiles
   each sequence once (textual/seed/nMin match = no-op, so per-row
   table calls stay cheap). `value(s, n)` advances **all three
   sequences in lockstep** from nMin with a rolling two-deep memo —
   forward sweeps are O(1)/step, backward jumps restart from nMin,
   `kMaxN = 10000` bounds the iteration (§9). Seeds: seed1 = u(nMin);
   seed2 = u(nMin+1), consumed only by (n-2)-referencing sequences;
   no-lag expressions evaluate directly at any n. Malformed refs
   (`u(n)` circular, `u(n-3)`, `u(3)`) fail compile → slot undefined.
   The engine's `n` variable is the sweep slot (complex-check skipped,
   callers save/restore like x/t/theta).
2. **Graph integration** (4D.7): `Mode::kSeq` + descriptor;
   `graph/seq_points.{hpp,cpp}` (SeqSource time-series PointSource +
   `make_seq_def`); GraphScreen `recompute_seq` fills the existing
   parameter-step point caches (time series), and **web/cobweb mode**
   reuses the (free in seq mode) function-mode column cache for the map
   curve f(x), the point cache for the cobweb stair, and draws the y=x
   diagonal from viewport math — all strip-safe (§8: render reads
   caches only). Web mode requires a pure own-lag-1 recurrence
   (`lag1_only`); others skip. Trace rides the caches (`u  n=…` readout;
   web points pair up per step); ZoomFit sweeps the time series; **F6
   CALC is blocked in seq mode** (continuous-curve constructs, v1).
3. **Editor + screens** (4D.8): `apps/seq_editor.{hpp,cpp}`
   (SlotEditorScreen subclass: nMin row, per-seq expr row with checkbox
   + seed row accepting `1` or `{1,2}`); WINDOW gains
   nMin/nMax/PlotStart/PlotStep rows in seq mode (10 rows, tightened
   row height); MODE Graph-mode cycles 4 modes + new **"Seq plot"
   TIME/WEB row**; table shows an integer-n column (`n` label, u/v/w
   value columns, non-integer rows NaN; the memo makes a top-to-bottom
   row sweep incremental); nav routes F1 to the seq editor; help
   updated.
4. **Persistence: PCG5→PCG6** — GraphState gains `SeqFunctions` (exprs,
   enables, two seeds), the n-range quartet, `seq_style`, **plus the
   reserved Batch 4 shading fields** (`shade_mode[7]` + 8 spare bytes)
   so zoom/shading lands without a second reset (D38). One-time
   graph-state reset on first boot, established precedent.
5. **Tests**: new `tests/host/test_seq.cpp` (51 checks: ramp/geometric/
   Fibonacci-lag-2/cross-ref/mutual/three-seq, explicit formulas, bad
   forms, nMin offsets, kMaxN cap, memo-across-begin, refresh() picking
   up variable edits, web eligibility + map_value). `test_graph` link
   list gains seq_expr/seq_points (table_model dependency).

## 2026-07-26 — 4D Batch 2 shipped: complex matrices (4D.25)

Second D38 batch, same day. Host suite green across all 12 binaries —
**1378 checks** (test_matrix 233→306); lint + format clean; both boards
build; **flashed to the Pico 1** and boot-verified over serial (temp +
psram-bulk heartbeats healthy). Pico 1 bss **218,156** (+4,824 over
Batch 1's 213,332 — exactly the union row-buffer widening below), ~52 KB
headroom; D28 watch continues.

1. **`matops` generalized to Complex** (`src/math/matrix.cpp` rewrite):
   the row-streaming kernels are now templated over an element-type
   policy (`RealOps`/`CplxOps`) — det/inverse/rref/ref/rank/add/sub/
   mul/scalar_mul/transpose/augment/reshape/power/copy all take both
   dtypes; pivoting compares **moduli**; mixed real/complex operands
   promote to a complex result (`CplxOps::read` widens real rows
   back-to-front in place). The three 200-elem row buffers became a
   `calc_t`/`Complex` **union** (one buffer set serves both tiers;
   +4.8 KB bss instead of +9.6). Output temps retype per expression
   (`retype()` — recycled `g_temp`s may carry a stale dtype).
   `determinant` gained a Complex overload (the calc_t entry point now
   errors **"Non-real matrix"** on complex input, D37); `scalar_mul`
   gained a `Complex k` overload; new `matops::make_complex(Array&)`
   (2-D in-place dtype migration, mirrors `listops::make_complex`).
   `eigen_core` **stays real-input** and errors "Non-real matrix" (D37).
2. **matexpr scalars ride Complex** (`src/math/mat_expr.cpp`): the
   parser's scalar `Value` is now a `Complex`; scalar spans fall back to
   `complexexpr::evaluate` when `eval_field` refuses (`i`, `2i`
   shorthand — the digit scanner folds a trailing `i` — and
   complex-valued variables); `det`/element access return complex
   scalars; `^` uses `c_pow` off the real fast path; complex scalar
   results commit **complex Ans/var stores** (`Result.scalar_complex` +
   `cvalue`, mirroring listexpr's Batch 1 shape; home screen formats
   via `format_complex`). REAL-mode gating is strict (Batch 1 rule): a
   complex `[X]` operand errors "Non-real result" at the token, and a
   complex result (e.g. `i*[B]` over a real `[B]`) errors at commit.
   `format_matrix` formats complex elements mode-aware.
3. **Matrix editor complex entry/display** (`src/apps/matrix_editor.cpp`):
   cell entry falls back to `complexexpr` exactly like the list editor
   (REAL mode → "Non-real result"; complex value → `make_complex`
   migration with a "Complex max 5000 cells" cap message); cells render
   short mode-aware complex forms (`format_cell_c`, same 11-char
   fallback shape as the list editor); the entry line shows the full
   `format_complex` form (`cur_val_` widened 24→48); F8 clear reverts
   the slot to the real dtype.
4. **Persistence** (`src/math/matrices_persist.cpp`): complex payloads
   under the unchanged **PCM2** header (the dtype byte was already
   there) — 16 B/elem chunks through the same 2 KB buffer
   (`kChunkComplex = 128`), complex load requires PSRAM (late-init
   retry, D14) and caps at `kMaxComplexElements`; older firmware treats
   a complex image as corrupt and skips it (established precedent).
5. **Tests**: `test_matrix` +73 checks (complex det/inverse round-trip
   `A*A^-1=I`/rref/rank/power/transpose/augment/mixed promotion/
   make_complex/reshape-dtype/identity-retype; matexpr complex layer
   incl. REAL-mode gates, dtype-preserving stores, complex Ans, format
   glyph). `scripts/host-tests.sh`: test_matrix links complex_expr.cpp.

No persistence-format bump needed this batch (PCM2 already carried
dtype). HW-PENDING row added (checklist above in the table).

## 2026-07-26 — Phase 4D STARTED: D38 batch plan; Batch 1 shipped — complex variables/Ans (4D.15) + complex lists (4D.24)

Phase 4D implementation begins. Both boards build; lint + format clean
(including two pre-existing lint failures fixed, block 4 below); host suite
green across all 12 binaries — **1305 checks** (test_lists 134→196,
test_complex_expr 44→74). Pico 1 flashed and boot-verified over serial
(`psram-bulk: OK`, battery + temp heartbeats, no crashes). Flash-path note:
the BOOTSEL-volume `cp` failed with "Permission denied" this time (a new
failure mode vs. the old xattr complaint), so `picotool load -f` remains the
reliable path; also, `picotool info` **segfaults** in picotool v2.3.0 —
`load`/`reboot` work fine, just don't lean on `info`.

**1. Phase 4D planning pass (D38).** A full plan for 4D was drawn up
(code exploration of engine/storage, the graph subsystem, and the
render/platform HAL) and the developer resolved every remaining open
question — recorded as **`decisions.md` D38** (full detail there).
Highlights: **4D.16 (xyLine/normprob plots) discovered ALREADY SHIPPED**
in Phase 3D (Session 15, D27) and closed with zero work; 4D.21 (build-id
diag label) closed — shipped 2026-07-25 (`f444db9`); **P4-12** = full
u/v/w + cross-reference in v1; **P4-10** = named user lists, full
integration (4D.13 re-estimated 4 → ~15 h); **P4-13** = rref-nullspace of
`(A−λI)`; units = typed `convert()` only; APD = soft-sleep v1 +
`/picocalc/settings.dat` (magic PCS1); two scope adds accepted
(home-screen `MatAns` token → 4D.14, fnInt shading in the curve's color →
4D.11); **sequencing = risk-first batches** (complex storage → complex
matrices → sequence graphing → zoom/shading → catalog glue → named lists
→ display/formatting → eigenvectors → device polish).
`phase4-spec.md` §7.3/§8/§11 and the summary table updated accordingly
(4D subtotal ~165 h, Phase 4 total ~300 h).

**2. Batch 1, first half: 4D.15 complex Variables/Ans.**
- `Variables` widened with a parallel `imag[28]` array (`engine.hpp`);
  `vars[]` stays the flat real view because tinyexpr binds raw slot
  addresses. New helpers `is_complex`/`set_real`/`set_complex`.
- New `math::refs_complex_var(expr, skip_slot)` (`engine.cpp`) detects
  references to complex-valued slots (`a`-`z`/`ans`/`theta` tokens).
- **Real-only consumers now ERROR, never truncate** (P4-11/D37):
  `Engine::eval_internal` ("Non-real variable"), `eval_field` (checks
  before rebinding x), and `compile`/`compile_with` — which grew a
  **`sweep_slot` param** excluding the caller's sweep variable, so a stale
  complex `x` can't block graphing `Y1=sin(x)`; `Engine::kNoComplexCheck`
  serves the slot editors' syntax-validity-only compiles. All
  graph/table/solver/seq compile sites pass their sweep slot.
- `complexexpr` resolves complex-valued variables itself
  (`parse_scalar_span` intercepts bare var tokens; opaque spans like
  `fac(a)` give a pointed "Non-real variable" error). The 4C "Complex
  results can't be stored" restriction is **removed** — `2i->a` now
  stores.
- `home_screen`: dispatch forces the complex path on `refs_complex_var`;
  complex Ans/store commit via `set_complex`; complex store display
  `3+2i→a`; REAL mode gives "Non-real result".
- Real writes clear the imag part everywhere (engine evaluate, `mat_expr`
  scalar store, `solver_screen`, graph trace/analysis stores);
  `evaluate_at` saves/restores both parts.
- **`variables.dat` format bump: new magic PCV1** (header + vars + imag,
  `home_screen.cpp`). The old raw 224-byte file is ignored → **one-time
  variables reset on first boot** under this firmware (expected,
  precedented — same "old files ignored" pattern as PCL2/PCM2).

**3. Batch 1, second half: 4D.24 complex lists.**
- `Dtype::kComplex` (interleaved re/im, 16 B/elem); complex arrays are
  **PSRAM-only regardless of size** (D37) and cap at **5000 elements**
  (`kMaxComplexElements`) so one 80 KB region holds a full array. `Array`
  gained `cget`/`cset`/`read_range_c`/`write_range_c`/`set_dtype`
  (empty-only retype); real accessors return NaN / no-op on complex
  arrays (loud failure per D37); byte-based zero-fill.
- `listexpr`: complex brace literals (`{1+i, 2-i}`); a narrow **complex
  vector lift** (v1 scope exactly per D37: `+`/`−` of terms, scalar `*`
  and `/`, one list per term; everything else gives pointed errors like
  "Complex lists support only +, -, scalar * and /"); `sum`/`mean` of a
  complex list supported **standalone only** (new
  `Result::scalar_complex`/`cvalue`; home_screen commits complex Ans);
  `stdev`/`median`/`prod`/`sort`/`cumsum`/`delta` error "Non-real list";
  complex list results error "Non-real result" in REAL Number mode
  **before** any store; `format_list` formats complex elements via
  `format_complex`.
- `listops`: dtype-aware copy; `csum`; `copy_complex`; `make_complex`
  (in-place real→complex migration); dtype guards on
  sum/prod/sort/cumsum/delta; `seq` retypes its output real.
- List editor: complex cells display (`format_complex` with a short
  fallback); entering a complex value into a real list migrates it via
  `make_complex` (REAL mode rejects); delete-row handles complex; sort
  gives "Non-real list"; F8 clear reverts the list to the real tier.
- Stats `one_var`/`two_var`/regressions error "Non-real list" on complex
  input; stat plots skip complex lists (the slot renders empty).
- **Lists persistence: PCL2 header unchanged** — the dtype byte was
  already there; complex payloads (16 B/elem) now save/load. Old firmware
  treats a complex list file as corrupt (graceful skip).
- `scripts/host-tests.sh`: test_lists now links `complex_expr.cpp`.

**4. Two PRE-EXISTING lint failures fixed** (HEAD from 2026-07-25 didn't
pass `./scripts/lint.sh`): the `framebuffer.cpp` int-to-ptr cast in
`display_service_main` (NOLINT with rationale — the inter-core FIFO is a
32-bit mailbox by design) and a dead store in `main.cpp` diag rendering.

**5. Pico 1 state.** Flashed with this build: bss **213,332** bytes
(+1,148 over 212,184 — the imag array + PCV1 image + editor scratch;
~57 KB headroom), boots healthy (psram-bulk OK, battery + temp
heartbeats, idle die temp 28-31 C). **Expected one-time variables reset
(PCV1)** on first boot; lists/matrices/graphstate formats unchanged
(PCL2/PCM2/PCG5). Batch 1's hands-on eval is queued in the HW-PENDING
table above and `next-session.md`.

Known limitations / deferred (all per the D37/D38 v1 scope, not
oversights): the complex vector lift covers only `+`/`−`/scalar `*`,`/`;
`sum`/`mean` on complex lists are standalone-only; stat plots skip complex
lists rather than plotting real parts; `eigen_core` stays real-input;
complex arrays always pay the PSRAM tier and cap at 5000 elements.
**Next per the D38 batch plan: Batch 2 — complex matrices (4D.25)**,
~22 h est: generalize `matops` (det/inverse/rref/ref/rank/augment/
reshape/identity/power/transpose/solve_linear) to `Complex` via the
static-row-buffer kernels; magnitude-based pivoting; matrix editor
complex entry/display reusing 4D.24's storage tier.

**Addendum (same day, after `c0d7321`): Batch 1 eval PASSED; polar-mode
display fix.** The developer ran the full Batch 1 checklist on the
Pico 1 — all passed (HW-PENDING row cleared above). One observation:
complex display in the list editor was weird under **polar** number
mode — complex elements showed a+bi form while real-valued elements
showed `r∠0`. Two root causes, both fixed:
- `math::format_complex` only short-circuited real values in the
  rectangular branch, so the polar branch rendered a plain `5` as
  `5∠0`. The `is_real()` early-return now precedes the mode branch —
  a real value displays as a plain number in every mode.
- The list editor's 11-char cell fallback (`format_cell_c`) was
  hardcoded to a short a+bi form, and polar strings almost always
  overflow 11 chars — so polar-mode complex cells nearly always
  regressed to a+bi. The fallback is now mode-aware
  (`%.3g∠%.3g`, degree-aware theta via `math::fn::deg`).
New host regression check ("polar mode real value stays plain",
test_complex_expr 75/75). Host suite 12/12 green, lint clean, both
boards rebuilt, Pico 1 reflashed — **developer confirmed the display
now works as intended.** Also (harness only, no firmware impact): the
three `.claude/agents/*.md` definitions pinned to `model: sonnet` now
pin `model: claude-sonnet-5` — the bare alias resolved to
claude-sonnet-4-5, unavailable on this foundry deployment (note: the
deployment currently rejects sonnet-5 too, so those agents error until
it's enabled; the pin is forward-correct).

## 2026-07-25 — Bugfix session: `!` factorial on the complex path fixed; D10 stall isolated to the multicore FIFO (DMA cleared)

Bug-fixing session on the Pico 1 (connected throughout, flashed via
`picotool load -f` — the `cp` xattr complaint recurred and aborted the
BOOTSEL-volume copy, so picotool was the reliable path all session).

**1. `!` factorial bug FIXED (the last open Pico 1 finding from 3D.14).**
Root cause (root-caused 2026-07-22 by code scan, worklog that date):
`math::complexexpr::evaluate` has no postfix-`!` rule of its own, and
`math::engine`'s `!`→`fac()` rewrite lived in an anonymous-namespace
`preprocess()` it couldn't reach — so `5!` failed as a syntax error
whenever input routed through the complex path (any non-REAL Number mode,
or an `i`-bearing expression). Note `fac(5)` already worked there (it
falls through `parse_scalar_span`→`eval_field`), only the postfix form
broke. **Fix**: exposed `math::preprocess_factorial()` (a thin public
wrapper over the existing anonymous `preprocess`) in `engine.hpp`, and
called it in `complexexpr::evaluate` on the trimmed body before parsing;
`fac()` itself still resolves through `eval_field`'s real engine like any
other scalar span. Added 4 host checks to `test_complex_expr`
(`5!`→120, `3!+2`→8, `(2+1)!`→6, `2*4!`→48) — all green (host suite
54/54). **HW-verified**: `5!`→120 and `4!`→24 on the Pico 1 in complex
mode. bss unchanged at 201896 (no new static data).

**2. D10 dual-core stall — DIAGNOSED: it was never the DMA, it's the
multicore FIFO handshake.** Item 1b from `next-session.md`, the queued
first step ("diagnose the FIFO stall — timing/handshake bug vs. real HW
constraint — before rebuilding"). Hypothesis going in: the D10
(2026-07-10) "core-1 FIFO stall on frame 1" was really a **DMA-to-SPI
hang** misattributed to the FIFO, because the dead core-1 service's job
handler called `push_rect_dma` (the DMA path, which has *never* run on
hardware — the shipped blocking path uses `spi_write_fast`), and if that
DMA never completed the service never acked, so core 0 blocked on
`drain_acks()` forever — indistinguishable from a FIFO stall. The same
D10 session had also found the vendored *bulk-PSRAM DMA* hung, making a
shared DMA fault plausible. **Test**: routed strip-mode pushes through
`push_rect_dma` synchronously on core 0 (no multicore), with per-render
timing prints, flashed, observed. **Result: DMA-to-SPI is healthy.** The
home screen renders correctly via DMA; full-frame ~160-172 ms (20 strips),
partial band ~15 ms. Confirmed on **warm reboot** *and* a **genuine cold
power-on** (correct home screen at frame 1 + live `dma-push:` heartbeat
while rails settle) — so the display DMA has **no** dependency on the
D14 PSRAM/SD rail settle: it sources pixels from the SRAM `strip_buf`,
on its own `dma_claim_unused_channel` channel, on the SPI peripheral —
nothing to do with the PSRAM QSPI/PIO or the D10 bulk-PSRAM-PIO fix.
Bonus: DMA (~160 ms) beats the blocking path (~200 ms, D10) even without
dual-core, since chunk conversion overlaps transfer.
**Conclusion**: the D10 stall lives in the **multicore FIFO handshake**,
not DMA — which de-risks reviving D10's compute/core-0 + DMA-push/core-1
display pipeline (the highest-value dual-core target per the 2026-07-24
scoping: SPI push dominates compute, so the win is freeing core 0 during
the ~160 ms push, not a single-frame latency cut). The diagnostic was a
temporary `framebuffer.cpp` edit, fully reverted; the Pico 1 is back on
the clean production build (blocking `push_rect`, boots healthy, `psram-bulk:
OK` + battery heartbeats).

**3. D10 ROOT-CAUSED AND FIXED (same session): XIP flash contention, not
the FIFO.** Continued the diagnosis by rebuilding the core-1 service
incrementally with instrumentation (a timeout-guarded one-shot run from
the main loop so it lands after USB enumeration and can't wedge the
device):
- **Step 1 — raw FIFO echo** (core 1 pops a value, pushes it back +100):
  **works flawlessly**, round-trips in a few us with USB connected. So the
  multicore FIFO handshake was *never* the problem.
- **Step 2 — core 1 runs the actual `push_rect_dma` + ack** (the exact
  pattern the dead D10 service used): **hard-wedged the whole chip** — USB
  dropped entirely (a hard fault, not a soft hang; a hang would have hit
  core 0's ack timeout and kept printing). Needed a BOOTSEL-catcher poller
  + power cycle to recover.
- **Developer observation that cracked it**: the crashing firmware boots
  fine with the **USB cable unplugged**. So the fault needs *both* core-1
  display access *and* an active USB stack on core 0.
- **Static read of `lcdspi`**: no IRQ / no DMA / no lock of its own —
  nothing in its logic to hard-fault. But `spi_write_fast`/`spi_finish`
  were already deliberately marked `__not_in_flash_func` (RAM-resident),
  a strong hint the SPI hot path can't safely execute from flash. The
  rest of the core-1 path (`push_rect_dma`, `dma_push`, `convert_565_666`,
  `define_region_spi`, `hw_send_spi`, CS toggles) ran from flash.
- **Hypothesis → fix → verified**: the RP2040 XIP cache is shared; core
  0's tinyusb/`stdio_usb` servicing (active only with USB plugged) churns
  XIP while core 1 executes the display path from flash → hard fault.
  Marking that whole path `__not_in_flash_func` (RAM-resident) removes the
  contention. **Confirmed on HW**: core 1 now runs `push_rect_dma` and
  acks reliably with USB connected — pushes 1..11+ all `ack=1` (~2 ms for
  a 4-row band), `psram-bulk: OK`/battery heartbeats keep flowing, no
  crash, and the developer confirmed the color-cycling test band renders
  correctly at the bottom of the screen. This also explains why the same
  `push_rect_dma` ran fine on **core 0** (only one core hitting XIP at a
  time). Fix committed as the RAM-residency change to `display.cpp` +
  `lcdspi.c` (the `mc-diag` scaffolding in `main.cpp` was reverted).
  **Next: wire the production dual-core pipeline** into `framebuffer.cpp`
  (core 0 renders the next dirty strip while core 1 DMAs the current one),
  now that the blocker is gone — see D10 addendum in `decisions.md`.

**4. Production dual-core display pipeline WIRED (same session,
HW-validated).** Revived `gfx::display_service_main` as the real core-1
service (pop job → `push_rect_dma` → ack) and rebuilt strip-mode
`render_frame` as a two-buffer pipeline: core 0 renders strip N+1 into one
ping-pong buffer while core 1 DMAs strip N from the other; a buffer is
reused only after its push is acked (`outstanding < 2`), and `render_frame`
drains all acks before returning so callers still see a completed frame.
`start_display_service()` launches core 1 once at boot (strip mode / Pico 1
only — the Pico 2 full-framebuffer path stays synchronous on core 0 and is
untouched, since that board isn't in hand to test). A `service_running`
guard keeps a synchronous core-0 fallback if any render ever runs before
the launch. Concurrency is clean: the only `push_rect` callers are in
`framebuffer.cpp`, and `render_frame` is only ever called from core 0
(main loop + stats "Computing…"), so core 1 is the sole SPI driver — no
cross-core bus contention. Cost: +10 KB bss for the second strip buffer
(212184 total, ~57 KB headroom). **HW-validated on the Pico 1**: developer
exercised home screen, expression entry, history scroll, screen
transitions — renders correctly (no torn/stale strips), no hang, feels at
least as responsive as before. This closes the D10 dual-core display leg
end to end (root cause → fix → pipeline).

**5. Perf measured (A/B on the Pico 1, temporary `render_frame`
instrumentation, reverted after).** Synchronous blocking `push_rect` (the
pre-session shipped path) vs. the DMA pipeline, matched by render load,
full-frame redraw (20 strips, rows 0-320):
- light render (~13 ms compute): **173.6 → 146.5 ms, −27 ms (~16%)**
- medium render (~28 ms compute): **190.3 → 147.8 ms, −42 ms (~22%)**
- status band (2 strips, the common dirty-band case): **15.4 → 13.5 ms,
  −2 ms (~12%)**

Headline finding: **pipeline frame time is flat at ~146-148 ms regardless
of render load** (up to the ~28 ms tested) because the render compute now
*fully hides* under the push, whereas synchronous time grows linearly with
compute — so the benefit grows with screen complexity. Decomposing the
~27 ms light-frame win: ~14 ms from DMA vs blocking push (the push itself,
~160 → ~146 ms, conversion overlaps transfer) + ~13 ms from render/push
overlap. **Push floor ≈ 146 ms** (~7.3 ms/strip = SPI wire time,
irreducible without a faster SPI clock). **Compute-bound screens get ~0
benefit** — one graph redraw showed `total=1,177,314 render=1,169,899`
(the render callback alone was 1.17 s); the pipeline only hides compute up
to the ~146 ms push budget, so heavier screens need the *secondary* D10
candidate (parallelize `recompute_function` onto a second engine) or plot
optimization, not this pipeline. Also noted: this design still blocks
core 0 in `drain_acks` (frame completes before `render_frame` returns), so
the win is frame-time, not "core 0 free for input during the push" —
that would need a deeper async redesign, unnecessary for the 16-22%
already gained.

**6. Grid usage-feedback fix (same session, HW-confirmed).** Acted on the
wishlist item logged earlier this session: a tiny `Xscl`/`Yscl` relative to
the axis range made `GraphScreen::draw_axes` draw thousands of merged grid
lines (slow + an illegible wash). Added a `thin_factor(per_px, min_px)`
helper and coarsen the grid step to the smallest multiple of `scl` spaced
>= 4 px — the "largest meaningful grid" — bounding line count to ~80/axis;
no change at normal scales (factor 1). Tick labels now snap to the
coarsened grid step (`grid_every * thin_factor(...)`) so they stay on grid
lines (a no-op when the grid isn't coarsened). HW-confirmed on the Pico 1:
dense case renders fast and legible, normal case unchanged. Wishlist item
moved to Completed/Closed (no phase/D-number).

**7. Die-temperature read + diag-screen cleanup (same session, HW-confirmed).**
Added `platform::die_temp_c()` reading the RP2040/RP2350 on-chip sensor
(ADC input 4, no external pin; `hardware_adc` linked). Shown on the diag
screen and on a 30 s `temp:` serial heartbeat — useful for watching the
Pico 1 overclock + core-1 pipeline thermals (idle reads ~28-31 C, i.e. the
overclock/dual-core cost is negligible). The read is cached with a 500 ms
min-refresh so it's stable within a frame: `render()` runs once per strip
(~20x/frame) and must be idempotent — a live per-strip re-read made a digit
on a strip boundary show two values (half-character glitch). Also closed
the stale-diag-screen backlog item: fixed the "Milestone 1" header comment;
added a **build id** (`git rev-parse --short HEAD`, `-dev` when the tree is
dirty) captured in CMake as `PICOCALC_BUILD_ID` and shown right-aligned on
the diag title line as `Phase 4C [<hash>-dev]`; and **removed the leftover
`Frame:` counter** — a Phase-1 bring-up artifact that did `frame_++` inside
`render()`, so it incremented per strip and its glyph straddled a strip
boundary (same class of glitch). HW-confirmed on the Pico 1: build id, die
temp, and the tightened layout all render cleanly, no glitch.

## 2026-07-24 — Docs/planning: D10 dual-core scoping, matrix/complex design departures closed (D36, D37), idea H raised

Docs/planning-only session — no application source touched, only
`docs/notes/*.md` and `docs/phases/phase4-spec.md`. No board reflashed;
both boards remain in their 2026-07-22 (D35) flash state. Four blocks:

1. **D10 dual-core display stall — scoped for the next bug-fixing
   session, not this one.** Revisited why core 1 was never adopted for
   the display service: D10 (2026-07-10) found that routing the strip
   push through a core-1 service over the multicore FIFO stalled on the
   very first frame; the synchronous-core-0 workaround shipped and the
   FIFO stall itself was never root-caused (`display_service_main`/
   `push_rect_dma` are still in the tree, unused). Surveyed the rest of
   the codebase for other parallelization candidates — matrix ops
   (`inverse`, `eigen_core`), iterative regression fits (`lm_fit`,
   `poly_fit`), and `GraphScreen::recompute_function`'s per-`Y=`-slot
   sweep. Conclusion: reviving D10's original compute/core-0 +
   DMA-push/core-1 pipeline is the highest-value target, since profiling
   since D11/D35 has consistently shown SPI push time dominating over
   compute (recompute ~15-17 ms vs. ~200 ms full-frame push pre-D13) —
   this is a pipelining problem, not a data-splitting one.
   `recompute_function` is a secondary, harder candidate (its shared
   `math::engine()` instance mutates a shared `X` var, so a naive split
   needs a second engine/vars context); matrix/regression ops were ruled
   out as inherently sequential (row-reduction/iteration). Added as item
   **1b** in `next-session.md`'s "The next job", queued into the next
   bug-fixing session alongside the already-known `!` factorial fix (item
   1) — not a Phase 4D item.
2. **Soak/feedback session** (testdrive-observations skill, both boards:
   Pico 1 current HEAD, Pico 2 Session 19 build) — its own artifact,
   `docs/notes/testdrive-2026-07-24-observations.md`, was already written
   and committed separately (`0c2cdae`) mid-session; not re-logged here.
   Key outcomes: MatAns and fnInt-shading feature requests folded into
   4D scope (already tracked in `next-session.md`'s "The next job" #2,
   from the 2026-07-22 Phase 4A-4C pass); "Non-real result" phrasing,
   font/glyphs, and D16 trace-sync judged fine as-is, no action; F3
   MODE/ZOOM and the Session 13 caps not tested this session, still open
   watch-items; new feedback logged but **not yet scoped into any
   phase** — fnInt/trace numeric limit entry (typed values, not just
   cursor-drag); and vector-ops/matrix feedback that fed directly into
   block 3 below.
3. **D36: pulled vector ops (E) and eigenvectors (G, new idea) into
   Phase 4D.** The soak session's vector-ops/matrix feedback overlapped
   directly with `design-departures-matrix-complex.md` idea E — the
   list↔matrix bridge half already shipped as 4D.12, but the vector-ops
   half (`dot`/`cross`/`norm`) never made it into 4D's task list — and
   raised a new idea, G (eigenvectors): 4A/4C ship eigen*values* only,
   no way to recover the corresponding eigenvectors. Rather than wait for
   the post-4D scoping pass `next-session.md` had planned, pulled both
   forward now: **4D.22** (`dot`/`cross`/`norm` on vectors, plus matrix
   Frobenius `norm`) and **4D.23** (matrix eigenvectors, real-only for
   v1, mirroring D28's real-only-eigenvalues precedent before D30 added
   the complex spectrum). Added open question **P4-13**
   (`phase4-spec.md` §11) for 4D.23's algorithm choice (nullspace of
   `A - λI` via existing `rref` vs. inverse iteration) and
   repeated/defective-eigenvalue handling. C/D (complex lists/matrices)
   and F (unified evaluator) were left deferred at this point, per their
   own gating reasons. 4D's subtotal grew ~109 → ~124 hrs (Phase 4 total
   ~244 → ~259 hrs).
4. **D37: closed out the remaining matrix/complex design departures (C,
   D, F) instead of leaving them for a post-4D pass.** Immediately after
   D36, worked through the rest of `design-departures-matrix-complex.md`
   via a user interview:
   - **P4-11 resolved: error, not silent truncation.** Real-only
     consumers of a complex value — matexpr scalar subterms, listexpr
     reductions, the graphing/table hot path — error rather than
     silently drop the imaginary part. Generalizes the rule from
     "complex variables" (the original P4-11 scope) to complex
     list/matrix elements too.
   - **C (complex lists): go, as 4D.24.** Complex-valued lists route
     **exclusively through the PSRAM region tier**, never the fixed
     28-slab/56 KB SRAM pool (`ArrayStore::kSlabCount * kSlabBytes`,
     ~67 KB headroom left as of D35) — zero bss growth, even for a
     3-element complex list, trading away the SRAM fast path small real
     lists get. v1 scope: storage, display, elementwise ops
     (add/sub/scalar-mul), `sum`/`mean`. `stdev`, regression, `sort`
     error on complex input in v1.
   - **D (complex matrices): go, as 4D.25**, reusing 4D.24's PSRAM-only
     storage answer. **Full complex linear algebra in v1** (the more
     ambitious of two scoping options offered, chosen by the user over
     "storage + elementwise only"): det (complex LU), inverse (complex
     Gauss-Jordan, magnitude-based pivoting), rref/ref/rank,
     augment/reshape/identity/power/transpose, and the solver's
     `solve_linear` path all generalize to `Complex`. Explicitly
     excludes eigenvalues/eigenvectors *of* a complex-valued matrix (a
     complex Hessenberg+QR core — materially bigger than generalizing
     arithmetic to `Complex`, and distinct from 4C's existing
     complex-eigenvalues-*from-a-real-matrix* feature, D30) — a future
     gap, not covered by 4D.23 or 4D.25.
   - **F (unified evaluator): committed as a real follow-on once 4D
     ships**, not indefinite parking — its own trigger ("2+ of B-E ship
     and duplication becomes visible") is expected to fire once 4D
     closes.
   4D's subtotal grew ~124 → ~160 hrs (Phase 4 total ~259 → ~295 hrs).
   `docs/phases/phase4-spec.md` (§1, §7.3 prose, §8 task table with new
   rows 4D.24/4D.25, §11 P4-11 resolution),
   `docs/notes/design-departures-matrix-complex.md` (A-G marked closed,
   header status updated), and `docs/notes/next-session.md` (item 4
   rewritten as "CLOSED 2026-07-24 (D37)") all updated to match.
5. **Idea H raised, explicitly NOT decided.** Follow-up question: how
   much work to make variables fully polymorphic — any `A`-`Z` holds
   real/complex/list/matrix, MATLAB-style, collapsing the three separate
   namespaces (`A`-`Z`/`[A]`-`[J]`/`l1`-`l6`) themselves, not just
   unifying the evaluators the way idea F does. Scoped as new idea **H**
   in `design-departures-matrix-complex.md`, grounded in real code:
   `Variables::vars` is a flat `calc_t[28]` with no Array-ownership
   capability; `ArrayStore`'s 28-slab/56 KB SRAM pool; `evaluate_input()`'s
   token-shape-gated dispatch cascade (`src/apps/home_screen.cpp`). Rough
   size 100+ hrs, deliberately not pinned tighter — flagged as needing
   its own design pass (tagged-`Value` representation, array-backed-
   variable capacity policy, the `[A]`/`A` namespace-collapse product
   question, persistence format). **Not given a D-number, not scoped
   into any phase task table** — the final call was to revisit after 4D
   ships, same checkpoint as F.

No code changes; host test count and both-boards build status unchanged
from the 2026-07-22 D35 flash. See `decisions.md` D36/D37 for full
rationale/tradeoffs/revisit-triggers, and `next-session.md`'s "The next
job" for the current handoff state.

## 2026-07-22 — Pico 1 perf fixes: stat-plot point cache, list-editor dirty bands, per-list/matrix persistence (D35)

Fourth block of work today, following 3D.14 and the Phase 4A-4C eval above. Where
those two blocks were docs/investigation-only, this one is code: root-caused and
fixed both non-blocking perf findings from 3D.14 (5000-point scatter plot slow to
render, list editor feels sluggish), plus a related bug the developer's own
retest surfaced mid-session. Full technical detail in **D35**
(`docs/notes/decisions.md`); this entry summarizes.

1. **Bucketed pixel-space point cache for stat plots** (`src/graph/stat_plot.cpp`,
   `src/graph/stat_plot.hpp`, call sites in `src/apps/graph_screen.cpp`).
   Root cause: `draw_stat_plots()` streamed a plot's *entire* list from its Array
   (PSRAM-tier above ~256 elements, D21) on every `render()` call — and
   `render()` runs once per `config::kStripHeight`-line strip (~20x/frame) on the
   Pico 1 vs. once for the whole screen on the Pico 2 (§8), so the same code paid
   a ~20x hidden multiplier on this board. Fix: `recompute_stat_plots()` now takes
   the viewport and builds a capped (800 points), decimated `PointCache` once per
   actual data/window change; scatter and normprob (order-independent) get
   counting-sorted into strip-height buckets so `render()` only visits the
   bucket(s) overlapping `fb.clip_y0()/clip_y1()`; xy-line keeps insertion order
   (segments depend on it) but still benefits from the small SRAM cache instead
   of re-streaming PSRAM every strip; box-plot outliers get a one-line guard
   (skip the fence-scan unless the slot's fixed row is in the current strip).
   Built clean both boards, lint clean, all 1219 host checks pass (stat_plot has
   no host coverage per the phase3-spec §8 strip-safety note — unchanged).
   Pico 1 bss +~8.4 KB (**201896 bytes** total, was ~193.5 KB) — comfortably
   inside the ~76 KB headroom watched since D28. Flashed and developer-confirmed:
   scatter "much faster," decimated output "looks right," xy-line/box-plot/
   normprob all still correct.
2. **List-editor dirty-band narrowing** (`src/apps/list_editor.cpp`,
   `src/apps/list_editor.hpp`). The screen already used the D13 dirty-band
   mechanism but only ever invalidated the whole 13-row grid on any change.
   Added `invalidate_row(int)` and `invalidate_header()`; `kUp`/`kDown`
   navigation and `commit_edit()` now narrow to just the affected row(s) (plus
   the header band on commits, since appending changes the "l1:N" count) when
   the visible window didn't scroll; delete/sort/clear keep the full-grid
   invalidate (they touch every row). Built clean, tests/lint pass. Flashed and
   developer-confirmed: "held key is definitely better" — but the developer also
   clarified the *actual* originally-reported sluggishness was specifically about
   "entering values when there are already large lists," which pointed at fix 3.
3. **One-file-per-list persistence** (`src/math/lists.hpp`,
   `src/math/lists_persist.cpp`, call sites in `src/apps/list_editor.cpp` and
   `src/apps/home_screen.cpp`). Real bottleneck, found by reading
   `ListStore::save()` after the developer's retest pointed at data entry, not
   rendering: it re-serialized **all six lists'** full contents to SD on every
   single commit — an I/O cost proportional to total stored data, not the edit
   size, and the dominant cost behind "large lists feel sluggish." Replaced the
   single concatenated `/picocalc/lists.dat` with one file per list
   (`/picocalc/list1.dat`..`list6.dat`, magic bumped **PCL1→PCL2** — old file
   simply isn't read under the new paths, same "old files ignored" precedent as
   prior format bumps); `save()` now takes the index that changed (every call
   site already only ever mutates one list per operation, verified by reading
   each one, not assumed). `load()` keeps the old all-or-nothing-per-item
   contract for the PSRAM-tier D14 retry case, but now tracks a per-list
   `loaded_[]` latch so a pending retry never re-reads (and clobbers an
   in-session edit to) a list that already succeeded. Built clean, tests/lint
   pass (one nit fixed: `save_lists()` made `const`). Flashed — developer
   confirmed the expected one-time reset (old lists showed empty, as designed),
   confirmed entry perf "much faster," and confirmed append/edit/delete/sort/
   clear and the home-screen list-expression store paths all still persist
   correctly.
4. **One-file-per-matrix persistence** (`src/math/matrix.hpp`,
   `src/math/matrices_persist.cpp`, call sites in `src/apps/matrix_editor.cpp`
   and `src/apps/home_screen.cpp`). Developer asked mid-interview whether
   matrices had the identical bug — confirmed yes (`MatrixStore::save()` had the
   exact same concatenated-file pattern) — and the identical fix shape was
   applied: `/picocalc/matrix1.dat`..`matrix10.dat` (magic **PCM1→PCM2**),
   index-aware `save()`, per-matrix `loaded_[]` latch. Built clean, tests/lint
   pass. Flashed — developer confirmed the expected one-time reset (matrices
   showed empty), confirmed entry perf on a large matrix "much faster," and
   confirmed edit/DIM-reshape/clear and the home-screen matrix-expression store
   paths all still persist correctly.

Host test count is **unchanged at 1219 checks** (198 math + 46 layout + 72 graph
+ 134 lists + 71 dist + 91 infer + 225 matrix + 27 solve + 117 analysis + 122
stats + 66 complex + 50 complex_expr) — no new host tests were added or needed,
since stat_plot and the persistence layers have no host coverage per existing
project convention (strip-safety and SD I/O both need real hardware/framebuffer
to exercise meaningfully). Both boards build clean; lint clean.

**Known limitations / deferred, explicitly**: `kMaxCachedPoints = 800` means very
dense scatters (checked with 5000 points) are visually decimated rather than
literally rendering every point — accepted as a non-issue at 320px screen width,
confirmed by the developer rather than assumed. The per-file persistence change
is a real, precedented format break — any lists/matrices saved before this
session were lost on first boot under the new firmware (expected, not a
surprise — confirmed with the developer during the interview). Pico 2 was not
reflashed this session; these fixes are board-generic (same code path, no
`#ifdef`s) so it should already benefit, but that is **not separately
verified** — carried to `next-session.md`. The `!` factorial bug root-caused
earlier today is unrelated to this session's work and remains unfixed (see
"The next job" in `next-session.md`).

## 2026-07-22 — Task 3D.14: Pico 1 combined pass — Phase 3 CLOSED (D18)

The Pico 1 (RP2040) board was swapped back into the PicoCalc unit, having sat on
Session 7 firmware for three days (Phase 2's Pico 1 verification pass was
deliberately deferred per D18, to be combined with Phase 3's own both-boards
pass, task 3D.14). Rebuilt `build/pico/picocalc_graphcalc.uf2` from current HEAD
(f9dbfb6, the Session 19 font/glyph build — no code changes were needed) and
flashed via BOOTSEL (`RPI-RP2`). Serial capture across three windows confirmed a
healthy boot: PSRAM OK, battery telemetry sane, graph recompute running, no
crashes or hangs.

Ran the full task 3D.14 checklist against this board: the Pico-1-specific
strip-renderer risks (split-pane clipping, graph status-bar clip, render
idempotency), the entire Session 8+9 fix list (screen-stack leak, held-key
scroll overrun, status-bar overdraw/staleness, charging-bit decode, DEG/RAD
persistence, wording fixes, ZStandard, typed commands, F-key remap,
case-sensitivity, DEL/SPACE, polar-gap fix, split-trace activation), the Phase 2
acceptance checklist (function/parametric/polar/tables/SD-PSRAM health), and the
Phase 3 acceptance checklist (list editor 3A, stats 3B, distributions 3C,
inference 3D, StatPlot layer — box-plot-outlier and normal-vs-skewed contrast
tests). Everything passed except two non-blocking findings, logged verbatim to
`session3D14-pico1-observations-verbatim.md`:

- `!` (factorial) throws a syntax error on this board — not investigated at the
  time; **root-caused later the same day by code scan, see the next entry
  below** (a genuine, board-independent bug, not a keyboard quirk). Factorial
  predates Phase 3, so this is not a Phase 3 regression.
- The list editor and a 5000-point scatter plot both feel sluggish on the
  Pico 1 — not profiled; could be strip-renderer overhead, RP2040 clock speed,
  or list/plot-specific cost.

This closes task 3D.14 and resolves **D18** (deferred Pico 1 pass). With 3D.14
done, **Phase 3 is declared closed** — retro written to `phase3-retro.md`
(what shipped / went well / was hard / carried into Phase 4, following the
`phase1-retro.md`/`phase2-retro.md` convention). The two findings above carry
forward as non-blocking backlog items, not blockers. No code changed this
session (docs/observations only); host test count and both-boards build status
are unchanged from Session 19.

## 2026-07-22 — Phase 4A-4C Pico 1 eval + `!` factorial root-cause (code scan)

Same session as 3D.14 above, same board/build (no reflash needed — Phase
4A-4C and Session 19 fonts were already on the binary flashed for 3D.14).
Ran the Phase 4A (matrices + solver), 4B (CALC menu / graph analysis), and 4C
(complex numbers) checklists against the Pico 1 — the first hands-on
functional walkthrough of any of these three sub-phases on **either** board
(Pico 2 was flashed at Session 19 but never walked through for 4A/4B/4C
specifically). Full verbatim record:
`phase4abc-pico1-observations-verbatim.md`.

All acceptance checks passed: matrix editor/ops/stores/persistence, solver
(form + inline), big-matrix PSRAM-tier perf (fine — unlike the list/scatter
sluggishness from 3D.14); CALC menu across function/parametric/polar and
both angle modes, including the tangent-line and shaded-fnInt strip-render
risks; complex-number mode cycling, arithmetic, store rules, polar glyph,
eigenvalue text display. Two reports turned out to be intentional per
`decisions.md` (matrix+scalar addition dim mismatch — D28, only
scalar-multiply is defined; case-insensitive matrix names — D28's deliberate
exception to D19), not bugs. One real UX gap (`MatAns` has no home-screen
token — editor-only, unlike scalar `ans`) and one feature request (fnInt
shading should follow curve color) logged for later.

**Closed the Session 16/17/18 Pico 2 HW-PENDING rows as a formality**: the
matrix/CALC-menu/complex-number logic is 100% board-independent (no
board-conditional code in any of these paths), and the harder rendering case
(Pico 1's strip renderer) just passed the identical checklist — so a
dedicated Pico 2 hands-on pass would be repeating the same checklist against
the same code, not exercising anything Pico 2-specific. Also fixed two
stale "NOT flashed" notes on the Session 17/18 rows — Session 19's flash
(2026-07-21) put current HEAD (including 4B/4C) on the Pico 2, that just
never got a hands-on walkthrough. **Genuinely still open, not covered by
today's pass**: Pico 2 perf feel for any Phase 3/4 feature has never been
re-measured against current code (only the pre-Phase-3 2.25 baseline
exists) — perf doesn't transfer between boards the way correctness does.

**`!` (factorial) syntax error — root-caused by code scan, no hardware
needed:** `math::complexexpr::evaluate` (`src/math/complex_expr.cpp`) has no
equivalent of `math::engine`'s `preprocess()` postfix-`!`→`fac()` rewrite. In
`kReal` Number mode (default), `HomeScreen::evaluate_input` only uses
`complexexpr` as a discardable probe and falls through to `math::engine()`
(which has the rewrite) on failure, so `5!` works. In `kRectangular`/`kPolar`
mode, `complexexpr`'s own "Syntax error" becomes the final, displayed result
— `math::engine()` is never reached. Reproduces in any non-REAL Number mode
(MODE screen, F3, "Number" row), on any board — **not** a per-unit
keyboard/hardware quirk as originally speculated. Fix candidate: give
`complexexpr` the same `!`-postfix rewrite (or share `engine::preprocess`).
Not yet fixed — added to the backlog (see `next-session.md`).

No code changed this session (docs/investigation only); host test count and
both-boards build status unchanged from Session 19.

**Same day, third block: closed the remaining HW-PENDING formalities.**
Applied the same board-independent-logic reasoning to the still-open Session
11 (lists), 12 (stats), and 15 3D (inference/plots) rows — closed their Pico
2 legs as a formality, same as Session 16-18. Session 10 round 3 (bulk
PSRAM) was already fully resolved on both boards and got marked closed
outright. Session 15's storage-health row was corrected to **fully
closed** per developer confirmation: hot-plug/retry-forever was checked on
both boards (not just inferred from Pico 1), and the Y=-editor truncation
detail was directly observed incidentally during other on-device testing.

Then ran a short interview for the one item that had no supporting evidence
either way — **Session 10 round 2**, closed 2026-07-22: `L` axis-label
toggle survives a reboot (confirmed — note the original PCG3 one-time-reset
transition itself is long past, both boards are now on PCG5, so this
confirms steady-state persistence, not the historical migration); `rand()`
shows a sensible, varying value each call (confirmed); ZTrig gives short
tick labels (`1.571`-style, confirmed); `F` ZoomFit auto-fits the y-range
correctly (confirmed). The HW-PENDING table is now clear except the
deliberately-deferred Pico 2 perf re-baseline and the still-informal
Session 19 font/glyph sweep.

`.claude/skills/testdrive-observations/` (the interview skill used across
today's sessions to gather and log all of this) is now tracked in git —
it had been sitting untracked all session.

## 2026-07-21 — Docs/tooling: wishlist restructure, validator fix, GitHub math-rendering pass

Documentation- and tooling-only session — no C++ source changes, no
hardware flashing, no code review; nothing here required a new
decision number. Prompted by a walkthrough of the D30 complex-number
storage rationale, which surfaced that the wishlist's provenance
trail stopped at "Graduated" (no record of what actually shipped).

- **`wishlist.md` restructured**: added a **Completed / Closed**
  section (after Active / Graduated). Moved both Graduated items there
  now that they've shipped — Complex numbers -> **D30** and TI-84
  CALC-menu graph analysis -> **D29** — each with an as-built summary
  and a decisions.md cross-reference. Corrected both dates to
  **2026-07-20** while doing so (initially assumed 07-21; verified
  against `decisions.md`, which dates both D29 and D30 07-20). Also
  moved the JuliaMono font item out of Active (where it had been
  inline-marked "Closed") into Completed / Closed, pointing at D31's
  general font selector. Graduated is now empty with a pointer note.
  Added three new Active items: vertical centering for fraction
  expressions (currently top-aligned to the line instead of centered
  on the bar), auto power-off/standby after idle, and remembering
  screen brightness/keypad backlight across power cycles (flagged as
  needing a feasibility check first — unclear whether the PicoCalc's
  backlight control is even readable back or write-only).
- **`scripts/validate_md.py` fix**: the unicode-math-outside-code-span
  check wasn't stripping inline code spans (`` `...` ``) before
  scanning, unlike the `$`-balance check next to it, which already
  did. A literal glyph reference like `` `√(x)` `` — documenting the
  D31 font glyph table, not math prose — was still flagged as a
  violation. Added the same backtick-stripping step used by the
  `$`-balance check.
- **Glyph references backtick-wrapped** across `decisions.md`,
  `worklog.md`, `next-session.md`, `wishlist.md`: these are literal
  on-device font glyphs/substitutions (D31), not math notation, so they
  read as code/verbatim rather than tripping the math-prose convention.
  Iterated twice — the first pass wrapped individual symbols; a second
  pass fixed cases where only the bare unicode char got backticked
  instead of the whole token it belonged to (e.g. the polar-slope
  formula's individual `θ`s -> the whole `r·cos(θ)/r·sin(θ)` expression
  in one span; individual `σ` chars -> the whole `σx`/`σy` tokens).
- **Genuine math-in-prose violations fixed** (loose `±`/`×`/`≥`, not
  glyph refs) in `worklog.md`, `testdrive-phase2-observations.md`,
  `phase3-spec.md` — converted to the project's existing `$\pm$`/
  `$\times$`/`$\geq$` LaTeX-math convention (already used in
  `dependencies.md`/`hardware.md`/etc).
- **GitHub math-rendering fix**: GitHub's `$...$` inline math doesn't
  render when a digit touches the `$` delimiter directly (ambiguous
  with a currency amount). Fixed 65 instances across 11 files
  (`hardware.md`, `worklog.md`, `feasibility.md`,
  `testdrive-phase2-observations.md`, `phase1-spec.md`,
  `phase2-spec.md`, `phase3-spec.md`, `phase1-plan.md`,
  `phase4-spec.md`, `AGENTS.md`, `README.md`) — e.g.
  `2$\times$` -> `$2\times$`, `320$\times$320` -> `$320\times320$`,
  `$\pm$1000` -> `$\pm1000$`.
- `python3 scripts/validate_md.py docs/ AGENTS.md README.md`: **24
  files validated, 0 issues** (final state, after the fixes above).
- **Identified, not fixed**: a stale diag-screen label
  (`src/main.cpp:213`) still hardcodes `"[milestone 1]"`, a leftover
  from the Phase 1 bootstrap plan (closed 2026-07-08) that was never
  updated through milestones 2-5 or phases 2-4. Fix design recorded in
  `next-session.md`'s Backlog line this session: swap it for the
  current phase name plus a build identifier (git short hash on a
  clean tree, `dev` on a dirty one), which needs CMake to capture
  `git rev-parse --short HEAD` + `git status --porcelain` and pass it
  through as a compile definition; the stale `main.cpp:6-8` header
  comment should go too. Not implemented this session — src/ untouched.

No test suite change (no source touched); no board builds run; no
flashing. `docs/phases/*-spec.md` edits in this session were the
mechanical GitHub math-rendering fix only, not content/scope changes.

## 2026-07-21 — Session 19: Font system + real math glyphs, `eig` alias, list UX polish (D31)

Large multi-part UI-polish + bugfix + font session, spanning 2026-07-20
into 2026-07-21. Started from a testdrive papercut (ASCII `<` for the
polar angle, plain `i`) and widened into a full pass: a build-time
swappable 8x16 main font with a shared math-glyph slot map, real-glyph
substitutions across every screen that had been using ASCII stand-ins,
an `eig` alias for `eigenvals`, and a list-history UX fix. All recorded
as **D31** in `decisions.md` (see there for the full as-built detail and
the on-device font comparison notes) — this entry summarizes what
landed and doesn't re-derive D31. Host suite grew **1206 -> 1219
checks** (`test_math` 197->198, `test_layout` 41->46, `test_lists`
133->134, `test_matrix` 219->225; all other suites unchanged), 0
failures; lint clean; format clean; both boards build with no new
warnings. Pico 1 text 362004 -> 364068 (+2064 B), bss 188684 -> 188820
(+136 B, ~188.8 KB of 264 KB — still the D28/D29/D30 watch item,
essentially flat). Pico 2 text 349068 -> 351164 (+2096 B), bss 382604
-> 382740 (+136 B). **Flashed to the Pico 2 (Terminus default build);
boots healthy, telemetry clean over serial.**

- **`eig` alias for `eigenvals`** (`src/math/mat_expr.cpp`): added to
  `kMatFns` and the whole-expression `eigenvals(...)`/`dim(...)` parse
  path (`eig([A])` now resolves identically to `eigenvals([A])`); the
  "must stand alone" rejection inside a larger expression covers it too.
  `catalog.cpp` gained the help entry; `test_matrix.cpp` covers the
  alias.
- **List UX (testdrive 2026-07-20 follow-up)**: `format_number_compact`
  (`math/format.{hpp,cpp}`) — a 4-significant-figure, ~5-character
  variant used by `format_list` so more list elements fit on one line;
  falls back to the full formatter for integers, FIX/SCI modes, and the
  scientific-notation range. Home screen (`home_screen.{hpp,cpp}`) gained
  **LEFT/RIGHT horizontal scroll of the newest result** when the input
  line is empty and the view is pinned to newest (`result_full_` keeps
  the untruncated string, `result_scroll_` is the pan offset, windowed
  by `result_max_scroll()`); otherwise LEFT/RIGHT still move the input
  cursor as before. A separate full-precision detail screen was
  considered and left KIV.
- **Swappable 8x16 main font + real math glyphs — the big one (D31)**:
  build flag `-DPICOCALC_FONT=spleen|juliamono|iosevka|unifont|terminus`,
  **default terminus** (`CMakeLists.txt`, `gfx/font.cpp`); the 5x8 small
  font stays Spleen always. All five fonts carry the same 32..140 slot
  map, including new high slots 127..140: `π`, ∠, `θ`, `σ`, Σ, χ, `μ`,
  imaginary `i`, store-arrow ⇒, `λ`, `≠`, …, ², `√`. New tooling:
  `scripts/ttf_to_utft.py`
  (freetype raster) and `scripts/hex_to_utft.py` (native Unifont .hex);
  `bdf_to_utft.py` gained `--extra`/`--hexfont`/`--hexmap`/`--hexshift`;
  per-font `scripts/gen-{fonts,juliamono,iosevka,unifont,terminus}.sh`;
  `scripts/mathglyphs-8x16.txt`; vendored OFL/dual licenses under
  `drivers/{juliamono,iosevka,unifont,terminus}/` (README + license only
  — font sources are fetched on demand, not committed); five committed
  headers in `src/gfx/fonts/`; `requirements-dev.txt` pins `freetype-py`.
- **Real-glyph substitutions across the UI**: `format_complex` polar
  `<`->∠ and `i`->the imaginary-unit glyph; MODE Number row shows
  `a+bi`/`r∠θ`; `render/layout_builder.cpp` gained a `preprocess_glyphs`
  pass that rewrites `pi`/`i`/`theta` and the `->` store op up front, so
  the substitution reaches the plain-text fallback too (e.g. `3+2i`
  renders correctly even when it doesn't build a full layout tree);
  `sqrt` deliberately stays a real function identifier (needed for the
  fraction/call structure, e.g. `1/sqrt(2)`) but its rendered name is
  `√`, so it prints inline as `√(x)` (a true radical vinculum over the
  argument is KIV). Home-screen result store indicator `>`->⇒;
  truncation `...`->… in `list_expr.cpp`, `mat_expr.cpp`, and
  `slot_editor.cpp`; graph-trace + table polar label `th`->`θ`
  (`graph_screen.cpp`, `table_model.cpp`); stats results `σx`/`σy`,
  `Σx`/`Σx²`/`Σy`/`Σy²`/`Σxy`, `r²` (`stats_screen.cpp`); inference `!=`->`≠` and
  `mu`/`sigma`->`μ`/`σ` (`infer_screen.cpp`); distribution `mu`/`lambda`->
  `μ`/`λ` (`dist_screen.cpp`). New display-byte constants: `gfx::kGlyph*`
  (`gfx/font.hpp`) and `math::kAngleGlyph`/`kImagUnitGlyph`/
  `kEllipsisGlyph` (`math/format.hpp`). Tests updated: `test_layout`,
  `test_lists`, `test_matrix`, `test_graph`, `test_complex_expr`.
- **Non-bug**: a reported "2-Var stats doesn't scroll" turned out not to
  be one — all 17 result rows fit on screen, so no scroll is needed.
  `kResVisible` was briefly changed then reverted to 17 (net: no change
  there).

Decisions recorded as **D31** (see `decisions.md`): the font-as-build-flag
design, the shared 32..140 slot map and its glyph sourcing (Unifont for
spleen/terminus/unifont, native TTF glyphs for JuliaMono/Iosevka), the
full substitution list, and the on-device font comparison verdict
(Terminus default; Unifont good; Spleen best-if-thicker; JuliaMono
worst; Iosevka a bit unbalanced from rastering).

Known limitations / deferred:
- No true radical vinculum — `√` is inline-only (`√(x)`), not drawn over
  the argument.
- No true subscripts (`Sₓ`, `σₓ`) in stats/inference displays — still text,
  just with real Greek letters now instead of `mu`/`sigma` spelled out.
- Rasterized fonts (JuliaMono, Iosevka) read worse than the bitmap fonts
  at 8px 1bpp with no antialiasing; antialiasing / a higher-res panel /
  a desktop emulator build would likely help — all unplanned, tracked
  in `wishlist.md`.
- Four non-default font build dirs (`build/pico2-jm|io|uni|term`) are
  now stale relative to this session's other (non-font) changes; a
  rebuild picks up everything. `build/pico2` is the canonical default
  (Terminus) and is what's flashed.
- Pruning the non-default fonts (if the selector is ever judged
  unnecessary) is left as a future call, not made this session.

Still HW-PENDING (unchanged from Session 18, plus this session's own
glyph-correctness sweep, see the table row above): the Session 11/12/
15/16/17/18 batches (lists, stats, storage health, inference/stat
plots, matrices/solver, CALC menu, complex numbers) still await their
on-device pass; then **3D.14** (the combined Pico 1 pass, D18); then
**Phase 4D** (CAS engine, `phase4-spec.md` §6).

## 2026-07-20 — Session 18: Phase 4C — complex numbers (D30)

All of 4C.1-4C.9 plus P4-7 (complex matrix eigenvalues, user pick — the
spec's own note said "likely defer") in one pass. Two user decisions taken
upfront: **P4-9 default number mode = REAL**, **P4-7 = add conjugate-pair
eigenvalues now**, both recorded with the rest of the as-built calls as
**D30**. No hardware was connected this session — **nothing was flashed**;
the Pico 2 stays on the Session 16 (4A) build. Host suite grew **1070 ->
1206 checks** (two new binaries: `test_complex` 66, `test_complex_expr`
50; `test_matrix` 199 -> 219), 0 failures; lint clean (two real
clang-tidy findings fixed: a De Morgan simplification, three
`modernize-return-braced-init-list`, one dead store); format applied; both
boards build clean. Pico 1 text 354036 -> 362004 (+7968 B), bss 188616 ->
188684 (+68 B, ~188.7 KB of 264 KB — unchanged watch item from D28/D29).
Pico 2 text 341420 -> 349068, bss 382536 -> 382604.

- **`Complex` type + math** (`src/math/complex.{hpp,cpp}`): arithmetic
  (Smith's algorithm for division — stable across extreme magnitudes),
  modulus/argument/from_polar, and the elementary set from the spec:
  `c_sqrt` (Numerical Recipes' cancellation-safe form), `c_exp`, `c_ln`
  (principal branch), `c_pow`, `c_sin/cos/tan`, `c_asin/acos/atan`
  (log-form identities), `c_abs/arg/conj/real/imag`. Fully host-tested
  against known values (Euler's identity, sqrt(3+4i)=2+i, round-trips
  through the inverse trig identities).
- **`NumberMode`** (`math/types.hpp`): `{kReal, kRectangular, kPolar}`,
  storage/persistence/MODE-screen row all mirror the existing `AngleMode`
  pattern exactly (D30 item 1) — new "Number" row on the MODE screen,
  `GraphState` shadow field, persisted GraphState bumps to **PCG5** (a
  one-time reset on first boot after this build, like PCG3/PCG4 before
  it).
- **`math::complexexpr`** (`src/math/complex_expr.{hpp,cpp}`): the
  home-screen complex evaluator, a recursive-descent parser over
  `Complex` mirroring `matexpr`'s shape (D28) since `Complex` can't flow
  through tinyexpr either. Special-cases only `i` (+ the `2i` literal
  shorthand) and the complex-aware function set; everything else —
  `pi`/`e`/`theta`/`ans`, bare variables, and the rest of the real
  catalog — reuses `eval_field` as an opaque real span (the same
  technique `matexpr::parse_scalar_span` already uses), so switching
  to a+bi/polar mode doesn't lose access to `ncr`/`round`/the
  distributions/etc. as long as their own arguments stay real.
  Deliberately **side-effect-free** (unlike `matexpr`/`listexpr`) so the
  home screen can use it as a cheap probe in plain REAL mode: a would-be
  `NaN` (`sqrt(-4)`) upgrades to a proper "Non-real result" domain error
  without double-committing Ans/a store (D30 item 4). `i` is now a
  globally reserved identifier (D30 item 2), same tier as `e` — blocked
  as a store target in the real engine, `matexpr`, `listexpr`'s `seq`,
  and `solve_expr`'s solve-variable parsing, all with pointed error text.
- **`format_complex`** (`math/format.{hpp,cpp}`): rectangular ("3 + 2i",
  pure-real/imag, unit-coefficient elision) and polar. No angle glyph was
  baked — the vendored Spleen BDF has no true U+2220 (checked; nearest
  are angle *quotation marks*, wrong shape) — polar uses ASCII `<`
  (`"2<60"`, the common EE phasor-notation convention) as a stand-in,
  flagged to revisit if it reads badly on hardware.
- **P4-7: matrix eigenvalues, full spectrum** (`src/math/matrix.cpp`):
  the D28 Hessenberg+Wilkinson-QR core is now `eigen_core`, filling
  `Complex` and never erroring on a conjugate pair (that's a legitimate
  result now, not a domain error). Two public entry points:
  `eigenvalues(Array&)` keeps its **exact old contract** (still errors
  "Complex eigenvalues" on any complex pair — existing tests/callers
  untouched) and the new `eigenvalues_complex(Complex*, int*)`.
  `mat_expr.cpp`'s `eigenvals([A])` now calls the complex-capable one: an
  all-real spectrum is still `Kind::kList` (storable into l1..l6,
  unchanged); a spectrum with a conjugate pair is a new `Kind::kText`
  (`matexpr::Result` gained a `text` field) — an unstorable formatted
  string like `{i,-i}`.
- **Home-screen dispatch** (`apps/home_screen.cpp`): the final scalar
  step in `evaluate_input` now branches on `number_mode()` /
  `complexexpr::mentions_i()` — non-REAL mode or a literal `i` routes
  through `complexexpr` (applying its own Ans/store once); plain REAL
  mode still hits the unchanged real engine, with the one-shot probe
  described above layered in front for the NaN-upgrade case.
- New host suites: `tests/host/test_complex.cpp` (66 checks, the
  `Complex` type standalone) and `tests/host/test_complex_expr.cpp` (50
  checks — the parser, `mentions_i`, store rules, `format_complex`,
  `NumberMode` plumbing); `test_matrix.cpp` grew 20 checks for
  `eigenvalues_complex` and the `eigenvals()` `Kind::kText` path.

Decisions recorded as **D30** (see `decisions.md`) — full detail there;
summary: REAL default (P4-9), `i` globally reserved, complex results
can't be stored (Variables/Ans stay real-only — widening them is out of
scope), no baked angle glyph (ASCII `<`), `eigenvalues()`'s old contract
frozen while `eigenvals([A])` gains the richer text form (P4-7).

Known limitations / deferred:
- No complex-valued variable storage — `5->a` works if real, `2i->a`
  errors. Would need widening `Variables::vars` past `calc_t`.
- No baked ∠ glyph; polar display uses ASCII `<` (`"2<60"`).
- `eigenvals([A])`'s complex form can't be stored into l1..l6 or nest
  inside a larger matrix expression (same limitation `dim`/`eigenvals`
  already had for lists).
- The REAL-mode NaN-upgrade probe re-parses every plain REAL home-screen
  expression once through `complexexpr` on Enter — home-screen-only cost
  (Enter-rate, not frame-rate; graphing never touches this path).

Still HW-PENDING: the full 4C on-device batch (MODE Number row cycle,
non-real domain errors reading clearly, a+bi/polar display and store
rules, the `<` polar stand-in, `eigenvals()` complex text) — plus the
older Session 11/12/15/16/17 batches; then **3D.14** (the combined Pico 1
pass, D18); then **Phase 4D** (CAS engine, `phase4-spec.md` §6 — weeks
32-36), which per the spec's own §5.5 hook uses this session's `Complex`
type as its numeric backing for complex roots.

## 2026-07-20 — Session 17: Phase 4B — graph analysis / CALC menu (D29)

All of 4B in one pass, on top of the Session 16 (4A) close. No hardware was
connected this session (`/dev/cu.usbmodem*` empty) — **nothing was flashed**;
the Pico 2 stays on the Session 16 build. Host suite grew **953 -> 1070
checks**, 0 failures; lint clean; both boards build clean (a format pass fixed
two `-Wformat-truncation` warnings by growing two `line[64]` -> `line[96]`
buffers in `graph_screen.cpp`). Pico 1 text 354036, bss 188616 of 264 KB
(**unchanged from the 4A baseline** — the Gauss-Kronrod tables are
compile-time constexpr arrays in flash/text, not bss). Pico 2 text 341420,
bss 382536. Two open spec questions resolved as **D29**.

- **Numeric calculus primitives** (`src/math/numeric_solve.{hpp,cpp}`, the 4A
  solver file per the spec's file plan): `numeric_extremum`/`_fn` (Brent's
  method: golden section + parabolic interpolation), `numeric_derivative`/
  `_fn` (central difference + one Richardson step, O(h^4)),
  `numeric_integral`/`_fn` (adaptive Gauss-Kronrod G7-K15, depth-capped at
  12 so pathological integrands can't hang; Kronrod nodes skip panel
  endpoints so endpoint singularities like `1/sqrt(x)` integrate cleanly).
  Each has a callback core (`EvalFn`) plus an expr-string wrapper — the
  callback form exists because parametric/polar integrands (`Y(t)*X'(t)`)
  aren't a single expression string.
- **Graph analysis engine** (`src/graph/analysis.{hpp,cpp}`): `AnalysisOp`
  (Value/Zero/Minimum/Maximum/Intersect/Derivative/Integral) +
  `AnalysisResult`, and `analyze_value/zero/extremum/intersect/derivative/
  integral`, mode-aware across function/parametric/polar — parametric slope
  = (dy/dt)/(dx/dt); polar slope differentiates the Cartesian forms
  r*cos(theta)/r*sin(theta) (correct in degree mode); parametric fnInt =
  integral of Y(t)*X'(t) dt; polar fnInt = area only, (1/2) integral of
  r^2 d(theta), always in radians internally regardless of angle mode
  (**D29 resolves P4-8**). Intersect solves same-independent-variable
  crossings only — documented limitation for parametric/polar.
- **Interactive session state machine** (`src/graph/analysis_cursor.{hpp,cpp}`):
  `AnalysisSession`, modeled on the existing `TraceCursor`, drives the
  TI-84 multi-step flow (e.g. zero: "Left Bound?" -> "Right Bound?" ->
  "Guess?"). **D29 resolves P4-6**: intersect curve picking is cursor-cycle
  (Up/Down + ENTER through "First curve?"/"Second curve?"), not a list
  picker; picking the same curve twice is refused. Fully host-testable.
- **UI integration**: new `src/apps/calc_menu.{hpp,cpp}` (`CalcMenuScreen`,
  form-list, 7 ops) pushed from a new **F6 "CALC" softkey** on the graph
  screen or typed `calc`/`analyze` (home screen); picking an op pops back to
  the graph and starts the session there, riding the existing `trace_`
  cursor machinery. `graph_screen.{hpp,cpp}` gained the analysis draw path:
  result marker, tangent line for dy/dx, shaded region under the curve for
  fnInt (function mode only, column-based fill off the cached plot-y array
  — new, since no Phase 3 shaded-region primitive existed despite the spec
  assuming one; §8 strip-safe), and a prompt/readout strip. Results store
  TI-style: root/location -> the mode's independent variable (x/t/theta),
  headline value -> Ans. Help screen gained the command + F6 CALC entries.
- New host suite: `tests/host/test_analysis.cpp` (117 checks) — the three
  numeric primitives directly, `analyze_*` in all three modes (spec
  acceptance cases: max of `4-x^2` at 0, intersect of `x`/`x^2` at 0 and 1,
  dy/dx of `x^2` at 3 = 6, integral of sin 0..pi = 2, parametric slope on
  the unit circle, polar area of a circle in both radian and degree mode),
  and the `AnalysisSession` state machine incl. the intersect
  same-curve-refusal case.

Decisions recorded as **D29** (see `decisions.md`): P4-6 (intersect =
cursor-cycle) and P4-8 (polar fnInt = area only) resolved; min/max keep the
TI "Guess?" step in the UI but Brent's method only uses the bracket,
ignoring the guess value — a judgment call to revisit on hardware.

Known limitations / deferred:
- Intersect can't find parametric/polar curves that cross at different
  parameter values (same-independent-variable crossings only).
- Min/max "Guess?" step is UI-only, not fed to the solver's bracket.
- No arc-length option for polar fnInt (area only, per D29/P4-8).
- Phase4-spec.md §4.3/4.5's assumed Phase-3 shaded-region primitive doesn't
  exist; the fnInt shading in `graph_screen.cpp` is new, function-mode-only.

Still HW-PENDING: the full 4B on-device batch — F6 CALC menu on the physical
keyboard, cursor feel riding curves in all three modes, shaded fnInt region
and tangent-line rendering, result-store-to-variable behavior, and the
min/max Guess-step judgment call — plus the older Session 11/12/15/16
batches; then **3D.14** (the combined Pico 1 pass, D18); then **Phase 4C**
(complex numbers, phase4-spec.md §5).

## 2026-07-20 — Session 16: Phase 4A — matrices + numeric solver (D28)

All of 4A.1-4A.9 in one pass. Three user calls taken upfront via a question
form (TI-style `[A]`-`[J]` matrix syntax; eigenvalues error on complex pairs
rather than a partial answer, since complex support is 4C; solver ships as
both a form screen and inline `solve()`), recorded as **D28**. Host suite grew
**716 -> 953 checks**, 0 failures; lint clean (five real clang-tidy findings
fixed: branch-clone, an analyzer null-path in `augment`, `std::max`, a dead
store, a missing `const`); format applied; both boards build with no new
warnings (pre-existing `home_screen` strncpy warnings remain); flashed to the
Pico 2, warm boot verified over serial.

- **`matops` + `MatrixStore`** (`src/math/matrix.{hpp,cpp}`): free functions
  over 2-D `Array`s (listops-style streaming — the spec's reference-returning
  `Matrix` class sketch doesn't fit the get/set PSRAM `Array`): add/sub/mul/
  scalar, transpose, LU determinant (direct for <=3x3), Gauss-Jordan inverse,
  rref/ref/rank, powers (`^-1`/`^0`/`^n<=100`), reshape (row/col-overlap
  preserving, for the editor's DIM), QR eigenvalues (Givens Hessenberg +
  Wilkinson shift, n<=10, real-only — a complex conjugate pair is the
  "Complex eigenvalues" error per the user's D28 call), descending 1-D list
  output. `MatrixStore` holds [A]-[J].
- **Persistence** (`src/math/matrices_persist.cpp`): `/picocalc/matrices.dat`,
  magic **PCM1**, the `lists_persist` pattern (all-or-nothing PSRAM load);
  wired into `main.cpp` boot + late-init retry.
- **`[A]` expression layer** (`src/math/mat_expr.{hpp,cpp}`): a recursive-
  descent evaluator (matrix ops aren't element-wise, so this isn't a
  listexpr-style lift) covering `[A]*[B]`, `2*[A]`, `[A]/3`, `[A]^-1`,
  `[A]^T`, `[A]^n`, `[A](r,c)` element access, `det`/`rank` inline scalars,
  `inverse`/`transpose`/`rref`/`ref`/`augment`/`identity`, `dim`/`eigenvals`
  whole-form list results, `-> [C]`/`-> lk`/`-> a` stores, a persistent
  MatAns buffer. Routed first in `home_screen`'s `evaluate_input`. Verified
  `[`/`]` are typeable (`kPrintable` + pass-through).
- **`matrix`/`mat` editor** (`src/apps/matrix_editor.{hpp,cpp}`): grid
  editor, TAB cycles [A]-[J] + Ans (read-only), F7 DIM, F8 clear, strip-safe
  cached render (§8).
- **Numeric solver** (`src/math/numeric_solve.{hpp,cpp}`): bisection to a
  tight bracket + Newton polish, Newton-from-midpoint fallback when there's
  no sign change, `lo == hi` = explicit-guess form, solve variable saved/
  restored. Reused by 4B's zero/intersect later.
- **`solve`/`solver` form screen** (`src/apps/solver_screen.{hpp,cpp}`):
  equation (optional top-level `=`), variable cycle, Lower/Upper/optional
  Guess; root -> variable + Ans; residual + iterations shown.
- **Inline solve** (`src/math/solve_expr.{hpp,cpp}`): `solve(f,x,lo,hi)` /
  `solve(f,x,guess)` / `solve(lhs=rhs,...)` substituted to numeric literals
  pre-evaluation (innermost-first, like the list reductions) — composes in
  any expression.
- **Pools grown** for the matrix population: `ArrayStore::kSlabCount` 12 ->
  28 (+32 KB bss), `kMaxPsramRegions` 12 -> 24; `kMaxCatalogEntries` 56 -> 72
  (12 help-only rows: matrix functions + `solve`).
- New host suite: `tests/host/test_matrix.cpp` (199 checks, incl. the
  `mat_expr` layer + the PSRAM tier) and `tests/host/test_solve.cpp` (27
  checks).

Known limitations / deferred:
- **Pico 1 bss is now ~188 KB of 264 KB** (~76 KB stack/heap headroom) —
  flagged for the 3D.14 Pico 1 pass; the knob is `ArrayStore::kSlabCount`.
- No matrix literals on the home screen — the editor is the only entry path
  for populating [A]-[J] (matches D28's syntax call; watch on device).
- `dim`/`eigenvals` are whole-form results (list-valued) and can't nest
  inside a larger expression.
- Element stores (`5 -> [A](2,3)`) are editor-only, not available inline.
- A pre-existing unicode `×` in `decisions.md` was fixed in passing (found
  by the markdown validation pass).

Still HW-PENDING: the full 4A on-device batch (bracket typing feel, matrix
editor, `[A]*[B]->[C]` round-trip, det/inverse/eigenvals spot-checks,
`matrices.dat` first-save + power cycle, solver screen + inline `solve()`,
>16x16 PSRAM-tier timing) — see the HW-PENDING table above — plus the older
Session 11/12/15 batches; then **3D.14** (the combined Pico 1 pass, D18);
then Phase 4B.

## 2026-07-20 — Session 15 (part 2): Phase 3 sub-phase 3D — inference + stat plots (D27)

All of 3D.1-3D.13 in one pass (3D.14, the combined Pico 1 pass, remains —
it needs the physical board swap). Host suite now **716 checks** (new
`test_infer`, 91), lint clean, both boards build (Pico 1 text ~305 KB,
bss ~147 KB/264 KB), flashed to the Pico 2. Conventions recorded as
**D27** (resolves P3-5 + P3-6).

- **`math::stats` inference** (`src/math/infer.{hpp,cpp}`): z (1/2-samp,
  summary), t (1-samp/2-samp/paired, Data or summary, pooled or Welch
  with fractional df), 1/2-prop z, chi-square GOF + 2-way (columns =
  l1..lk), one-way ANOVA (groups = l1..lk), linreg slope t-test; the six
  interval families. `Alt` (!=, <, >) on mean/prop/slope tests;
  p-values via new one-sided `dist` survival functions (`normal_sf`,
  `t_sf`, `chisq_sf`, `f_sf` — far-tail precise). Lightweight streaming
  `mean_sd` (no quartile selection) for the t machinery.
- **`test` command** (alias `infer`) → 15-kind form screen with
  Data/Stats source toggle, list pickers, InputLine numeric fields,
  H1/pooled/count cycles; results as cached lines (§8).
- **StatPlot layer** (`src/graph/stat_plot.{hpp,cpp}`): scatter,
  xy-line, histogram, modified box plot (1.5-IQR outlier marks, fixed
  bands), normal probability plot (Blom). Cache/draw split for strip
  safety: recompute caches bins/five-numbers/sorted+quantile Arrays;
  render only streams and draws. Plot1-3 config via the `plot` command
  (persisted — **PCG4**, one-time graph-state reset on first boot);
  graph screen draws plots under curves; **`Z` = ZoomStat**.
- **Reference vectors**: `tests/host/gen_infer_vectors.py` (mpmath).

Remaining in Phase 3: on-device eval (Sessions 11/12/15 HW-PENDING) and
**3D.14** — the combined Pico 1 pass (D18): reflash `build/pico/…uf2`,
Phase 2 sweep + Sessions 8/9 fixes + Phase 3 acceptance + strip-render
idempotency of every §8 screen, map-file re-check.

## 2026-07-19 — Session 15 (part 1): Session 14 observation fixes — storage health (D26)

Implemented the logged Session 14 batch (`session14-observations-verbatim.md`):

- **Retry-forever** (D26): the D14 30 s late-init window is now only the
  fast phase; unhealthy SD/PSRAM retry on a 10 s heartbeat indefinitely.
  `run_self_tests()` skips green subsystems (the PSRAM word test
  bump-allocates per run — must not repeat forever).
- **SD hot-plug**: DET pin polled ~1 s in the main loop. Eject →
  `Storage::on_card_removed()` (f_unmount + `sd::invalidate()`),
  `g_sd_test = kNoCard`; insert → immediate retry. Persisted state
  loads exactly once per power-on (re-insert never clobbers the
  session).
- **Red `SD` / `PSRAM` status-bar indicators** while down
  (`ui::set_health_flags` + chrome), clearing on recovery;
  band-invalidated like the battery refresh.
- **Y=/PAR/POL editor**: long expressions truncate with `...` before
  the enable checkbox (stored regression models ran beneath it).

Both boards build, lint clean. HW checks queued in the Session 15 row.

## 2026-07-19 — Session 14: Phase 3 sub-phase 3C — probability distributions (D25)

Session 13's batch was **developer-verified on device** at the start of this
session (HW-PENDING row cleared above), then 3C.2-3C.8 were completed in one
pass. Host suite now **625 checks** (new `test_dist`, 71 checks + parser-path
checks in test_math), lint clean, both boards build, **flashed to the Pico 2**
(boot verified over serial). Conventions recorded as **D25** (resolves P3-4).

- **`math::dist`** (`src/math/dist.{hpp,cpp}`): normal/t/chisq/F
  pdf+cdf+inv and binomial/poisson/geometric pmf+cdf on the 3C.1 cephes
  primitives (`ndtr`/`ndtri`, `incbet`/`incbi`, `igam`/`igamc`/`igami`)
  + `std::lgamma` closed forms. Two-sided CDFs (P3-4 → D25), lower-tail
  inverses, real-valued df, NaN on domain errors, far-tail saturation
  guards (`ndtr` overflows at 1e99), TI integer rule for discrete args.
- **Link fix**: cephes had compiled but never linked into a binary;
  `lgam` pulls an `isfinite()` *function* that neither newlib nor macOS
  libm exports → `src/math/cephes_support.c` shim (in the cephes CMake
  target + host-tests), documented in `drivers/cephes/README.md`.
- **3C.7 registration**: catalog fp3/fp4 casts, 18 rows (47 total,
  `kMaxCatalogEntries` 32 → 56 — `kLookupCount` follows), help FUNC
  summary column now yields to long signatures. The test_math catalog
  check now builds a numeric call at the declared arity (signatures use
  descriptive parameter names, which are not variables).
- **3C.8 `dist` command** → guided form (Distribution/Function cycles,
  InputLine parameter fields with shared named slots, Calculate shows
  the equivalent call + result and updates Ans). Help COMMANDS/KEYS/
  SYNTAX updated.
- **Test infra**: `tests/host/gen_dist_vectors.py` (mpmath, 50-digit
  reference values); new `.venv` + `requirements-dev.txt` policy for
  Python dev-deps (developer rule this session).

Remaining in Phase 3: on-device eval (Session 11/12/14 HW-PENDING rows),
then sub-phase 3D (inference + stat plots), then the combined Pico 1
pass (3D.14, D18).

## 2026-07-19 — Session 13: on-device observation batch — bug fixes + usability (D24)

Worked the verbatim Session 11/12 usage-notes list
(`phase3A-3B-observations-verbatim.md`): fixed the two reported bugs, implemented
the direct requests, parked the design questions on the wishlist. Dispositions
recorded as **D24** (the 3C naming calls move to D25). Host suite now **508
checks** (was 473), lint clean, both boards build (Pico 1 bss ~135 KB/264 KB
after +9 KB of lift-operand buffers), **flashed to the Pico 2** (BOOTSEL cp
path, boot verified over serial — battery + `psram-bulk: OK` heartbeats).

Bugs fixed:

- **Brace-literal broadcast** (`{1,2,3}+2` → "Expected a list"; `{1,2,3}+{2,2,2}`
  → "Bad list element"): the whole-literal test was first/last-char only (so
  `{..}+{..}` misparsed as one literal) and the vector lift knew only l1..l6.
  Fix is general **lift operands** (D24.1): top-level literals and wrapper calls
  evaluate into 4 side arrays bound as extra engine vars — wrapper results now
  compose in expressions too (`cumsum(range(1,4))+1`).
- **Home screen unreachable after graph/HOME interplay**: `switch_to()` at
  depth 1 `replace()`d the stack root (trigger: F4 trace from home), leaving no
  home screen beneath. `switch_to` now never displaces the root (pushes instead).
- **List editor negative numbers** drew in placeholder gray (leading-'-' test);
  placeholder detection is now exact-match on "_"/"---".

Implemented from the notes:

- **`range(lo, hi[, step])`** — inclusive, default step +/-1 toward hi,
  seq-backed, catalog-registered. The quick large-list generator (item 5/7).
- **`mean` / `median` / `stdev` (+`std`) home reductions** via `stats::one_var`,
  and **reduction args generalized to any list expression** (innermost-first) —
  `sum(range(1,10000))`, `mean(l1*2)` (lifts the D22 bare-arg limitation).
- **Aliases**: `?` → help, `list` → lists, `stat` → stats.
- **Stats "Computing..." indicator**: one forced frame before the synchronous
  Calculate (D23 revisit closed).
- **Pi glyph**: `bdf_to_utft.py --map` bakes U+03C0 at 0x7F in the 8x16 font
  (byte-identical regeneration otherwise); layout builder renders identifier
  `pi` as the glyph (host-tested, incl. no substitution inside longer idents).
- Help updated (COMMANDS aliases, LISTS syntax incl. literals/range/reduction
  args, 4 new FUNC rows — catalog at 29/32 entries).

On-device eval of this batch queued in HW-PENDING (Session 13 row) alongside
the still-pending Session 11/12 sweeps.

## 2026-07-19 — Session 12: Phase 3 sub-phase 3B — descriptive stats + all ten regressions (D23)

Continued Phase 3 per next-session.md (the 3A on-device eval still needs the
developer at the keyboard; its checklist stays in HW-PENDING). All nine 3B
tasks (3B.1-3B.9) are code-complete, host-tested, lint-clean, built for both
boards, and **flashed to the Pico 2** (BOOTSEL-volume cp path; boot verified
over serial — `psram-bulk: OK`, battery heartbeat).

- **`math::stats`** (`src/math/stats.{hpp,cpp}`, host-testable through the
  D22 psram_backend seam): `OneVarStats` (plain + freq-weighted),
  `TwoVarStats`, `regress()` for all ten models, `eval_model`,
  `format_model`, display metadata. Spec structs + the project error
  convention (`ok` + static error string).
- **Quartiles without sorting**: weighted rank selection by binary search
  over the order-preserving uint64 image of the doubles — one streaming
  counting pass per bit, all ranks batched into the same passes (<= 64
  total), optional x-range filter. Serves plain quartiles, freq-weighted
  quartiles (integer freq >= 0, freq 0 excludes), and the median-median
  group medians with one code path and zero allocation (D23).
- **Regressions**: polynomial (1-4) via normal equations on
  center+scaled x with binomial expansion back (quartic on year-scale x
  stays conditioned — host-tested); ln/exp/pwr linearized with TI-style
  r/r² from the linearized fit; **logistic + sinusoidal via LM (P3-3
  resolved → D23)** with logit-linearization / frequency-scan seeding;
  median-median via filtered selection (x-boundary ties group by value).
  `r` = NaN where TI doesn't define it; `r²` = 1 - SSE/SST elsewhere.
- **3B.8**: `format_model` emits an engine-parseable model in x
  (`%.10g` coefficients, parenthesized exponents); the stats screen
  writes it to the chosen Y slot, enables it, saves graph state. SinReg
  b/c convert to degrees when the global mode is DEGREE (spec §10).
- **3B.9**: typed **`stats`** command (D20) → `StatsScreen`: form
  (Analysis / lists / Freq / Store / Calculate) + scrollable cached-text
  results; strip-safe (compute in on_key, render draws only). Help KEYS
  + SYNTAX updated.
- **Tests**: `tests/host/test_stats.cpp`, 122 checks — known-answer
  stats (incl. weighted + a 1000-element PSRAM-tier list), all ten
  fits (exact-data recovery for LM models), error paths, and an
  engine-compile + eval cross-check of every generated model string.
  Suite now **473 checks**.
- Deviations from spec sketch: stats structs gained `ok`/`error`; r/r²
  conventions and `converged` semantics per D23; `one_var` returns
  quartiles NaN for n=1.

Same session, after the 3B commit: **task 3C.1 (cephes vendoring) —
done** (see the status addendum above; commit `deps: ...`). The
`TE_FUNCTION0 + arity` registration in `build_lookup` already
generalizes to arity 3-4, so 3C.7 only needs catalog-side fp3/fp4
helpers + headroom.

Next: on-device eval (Session 11 + 12 rows in HW-PENDING), then the
rest of 3C: `math::dist` wrappers (P3-4 naming call at 3C.2), catalog
registration, `dist` helper screen.

## 2026-07-19 — Session 11: Phase 3 sub-phase 3A — Array, lists, list editor (D22)

Started Phase 3 per next-session.md. All five 3A tasks (3A.1-3A.5)
code-complete in one session; both boards build, lint clean, 106 new host
checks (suite now 351), Pico 2 flashed and boot-verified over serial.

**Design departure recorded as D22** (the load-bearing discovery of the
session): the spec's §2.1 `Array` sketch assumed pointer access, but the
PSRAM is SPI-attached and **not memory-mapped** — `platform::Psram` deals
in addresses via `read()`/`write()`, and its allocator is bump-only. The
as-built API is therefore `get`/`set` + `read_range`/`write_range`
(which is also exactly the shape D21's dtype-tag rule wants), and
`ArrayStore` recycles fixed-size storage: 12 x 2 KB SRAM slabs, up to
12 x 80 KB PSRAM regions on a free-list (bounded, fragmentation-free).
The spec §2 got an "as built" note.

What shipped:

- **`math/array.{hpp,cpp}`** — dtype-tagged (D21) 1-D/2-D `Array`,
  tier migration at 256 doubles (slab→region and back), zero-filled
  growth, cap 10000; `math::psram_backend` seam so host tests run the
  identical code against a malloc shim (`tests/host/host_psram_backend`).
- **`math/lists.{hpp,cpp}` + `lists_persist.cpp`** — `ListStore` l1-l6;
  `/picocalc/lists.dat` (magic `PCL1`, per-list dtype+count header,
  elements streamed in 2 KB chunks both ways — a full file can be 480 KB,
  far beyond any SRAM buffer, hence new **`Storage::read_file_range`**
  (f_lseek) in the platform layer). Load is all-or-nothing and returns
  false until PSRAM is up when large lists exist (D14 cold boot); main's
  late-init loop retries it (`late-init: lists loaded`). Saves are
  on-mutation (editor commits, home-screen list stores/sorts).
- **`math/list_ops.{hpp,cpp}`** — sum/prod, in-place sorts (NaN-safe
  total order; PSRAM tier uses an **external merge sort**: 256-element
  sorted runs into a temp region, streaming merge passes ping-ponging
  region<->region, ~6 passes at 10000), cumsum, delta_list, seq (engine
  compile-once, var slot saved/restored), copy — all chunked/streaming.
- **`math/list_expr.{hpp,cpp}`** — the home-screen syntax layer (D22):
  literals, l-refs, `->lk` store, bare-arg reductions substituted as
  numeric literals, wrapper functions, and the **vector lift** — any
  engine expression over l1..l6 compiles once via the new
  `Engine::compile_with(extras)` (l1..l6 bound as per-element variables)
  and evaluates in 256-element chunks. `sort_asc(l1)` bare-arg form
  sorts in place per spec; compound args are by-value.
- **`apps/list_editor.{hpp,cpp}`** — grid editor (3 of 6 lists visible,
  horizontal scroll, append row, type-to-edit via `eval_field`, DEL
  row-delete, F6/F7 sort, F8 clear, global F1-F5 intact), reached by the
  typed **`lists`** command. Strip-safe: visible cells cached as text on
  change; render only draws. Dirty-band tracked.
- **Home screen**: list expressions get first crack in evaluate_input
  (kNone falls through to the scalar engine untouched); `Entry.result`
  widened 24→48 for `{...}>l1` results; scalar-result formatting
  factored. **Catalog** gained help-only rows (fn == nullptr, skipped by
  build_lookup) for the eight list functions; help FUNC column widened
  (kSummaryCol 13→19), KEYS/SYNTAX got LIST EDITOR / LISTS sections.

Testing: `tests/host/test_lists.cpp` (106 checks) covers Array basics +
tier boundaries + recycling, all ops (incl. 5000-element external sort,
NaN ordering, seq edge cases), and ~40 list_expr cases (grammar, stores,
errors, empty lists, formatting truncation, PSRAM-tier lift). test_math's
catalog check was taught about help-only rows (arity check now
registration-only).

Flash: BOOTSEL cp path; note **`cp` exits 1 on macOS with "could not
copy extended attributes"** — harmless, the UF2 lands and the board
reboots (verified: volume unmounted, app re-enumerated, `psram-bulk: OK`
heartbeat + battery line on serial). On-device functional eval is queued
in HW-PENDING (Session 11 row).

Deferred/notes:

- Reductions take bare list names only; list literals can't sit inside
  element-wise arithmetic (both documented in D22 — revisit if 3B wants
  a real tagged-value evaluator).
- List results don't set Ans (scalar Ans preserved — D22).
- The editor's F8 clear is immediate (no confirm) — watch in eval.
- ArrayStore worst case: 24 KB static SRAM (slabs) + 960 KB PSRAM.

## 2026-07-18 — Session 10: pre-Phase-3 deferred-item batch (D9 fonts, rand seed, ZoomFit, axis labels)

Reviewed the deferred-items backlog before Phase 3; the user picked four to
clear first (D10/D14 stay next in line): D9 was easy, ZoomFit is a good
feature, axis tick labels need on-device evaluation (and needed D9's small
font), and rand() needed seeding before statistics work begins.

**D9 font upgrade — done (also D17 permissive-path step 3):**

- Vendored **Spleen 2.2.0** (BSD-2-Clause) under `drivers/spleen/` (8x16 +
  5x8 BDFs + LICENSE + README with regen commands).
- New `scripts/bdf_to_utft.py` converts BDF → the UTFT header layout
  `gfx::Font` already reads (packed row-major bitstream; glyphs composed via
  BBX/ascent). Verified by decoding glyphs back to ASCII art.
- Generated `src/gfx/fonts/spleen8x16.h` / `spleen5x8.h` (ASCII 32–126,
  ~1.5 KB + ~0.5 KB); `gfx::main_font()` is now the 8x16, new
  `gfx::small_font()` is the 5x8. Coyote `font1` is **no longer compiled
  in** — NOTICE.md updated (GPL surface now lcdspi/i2ckbd/pwm_sound only).
- Layout: Spleen's cell has built-in leading (caps ink rows 2–11), so
  16px rows pack tight. `kRowH 14→16` (table, files), `kLineH 14→16` +
  `kVisibleLines 19→16` (help), help scroll indicator moved into the title
  bar. Everything else already scaled off `font.height()` or had roomy rows;
  bar heights and text y-offsets unchanged (caps ink fits).

**rand() seeded** (closed the lint-era backlog item): `math::fn::rand01()`
is now a **xorshift64\*** PRNG (top-53-bits → [0,1) double) with
`seed_rand(uint64)`; firmware `main()` seeds from the SDK entropy source
(`get_rand_64()`, new `pico_rand` link dep). Host tests stay deterministic
under the default seed; 6 new checks (range, determinism, divergence) —
math suite 105→111.

**ZoomFit** (`F` on the graph screen, task 4.7 finally): function mode
refits y over the current x-range (TI behavior); parametric/polar refit
both axes to the curve extent. Sweeps world coordinates through the same
`PointSource`s recompute plots from (kMaxCurvePoints cap; engine vars
saved/restored like recompute). 5% margin, half-unit span floor for flat
curves; no active/plottable curve → no-op. Help KEYS updated.

**Numeric axis tick labels** (task 4.4, deferred since Phase 1 M4): drawn
in the small font at scl grid lines, thinned to >= ~48px (x) / ~24px (y)
apart, origin skipped, placed beside the axes (flipping/clamping at
edges; screen-edge fallback when an axis is off-screen). **`L` toggles
live** — this shipped explicitly as an *evaluation* feature: judge on
device whether it's too distracting; not persisted. Strip-safe (pure
draws in `draw_axes`' const path).

Lint clean (two float-loop-counter findings fixed by switching to the
integer-index grid idiom), both boards build, 216 host checks green.
**Pico 2 flashed same session** (BOOTSEL cp path, ~15 s mount wait
confirmed again); on-device eval queued in HW-PENDING.

**Round 3 (same day) — D10 root-caused, fixed, HW-verified; D14 scoped.**
The deferred-queue review moved to D10/D14:

- **D10 bulk-PSRAM hang: solved.** Reading the vendored driver against
  its PIO program found the mechanism — the PIO takes **8-bit transfer
  counts** (`out x, 8`/`out y, 8`; max 255 bits = 31 bytes/transaction),
  and `psram_write()`/`psram_read()` let the count byte wrap above 27/31
  data bytes, desyncing PIO from the DMA stream (count 0 underflows
  `jmp x--` into a ~2^32-bit shift loop) and wedging the blocking DMA
  wait. Fix: `Psram::read/write` chunk internally (27 B writes / 31 B
  reads, single DMA call + one mutex hold per chunk; also respects the
  chip's ~8 us tCEM). **Un-quarantined** — any length/alignment works.
- **Watchdog-guarded bulk self-test** in `run_self_tests()` (permanent):
  cap-straddling sizes, unaligned start, cross-chunk addressing, 1 KB
  timing. The historical failure is an infinite DMA wait, so the test
  arms a 2 s watchdog with a scratch-register marker — a regression
  reboots once and the next boot skips the test (no boot-loop). Verdict
  repeats on a 30 s serial heartbeat (`psram-bulk:`); diag screen shows
  `PSRAM: word OK, bulk OK`.
- **HW-verified on the Pico 2 same session**: `psram-bulk: OK (1KB
  write 150 us, read 156 us)` — ~6.8 MB/s, roughly 40x the word path.
  Unblocks the Array PSRAM tier (D21 keeps Phase 3 SRAM-only by choice,
  not necessity) and Phase 4 matrices.
- **Serial-capture gotcha found**: pico stdio_usb transmits only with
  DTR asserted — `screen` does that, plain `cat` does not (why captures
  looked dead). New `scripts/serial-capture.py` asserts DTR/RTS for
  non-interactive use: `serial-capture.py [seconds] [match-substring]`.
- **D14 (rail settle)**: software instrumentation is already sufficient
  (late-init timestamps + heartbeats); root cause needs bench time —
  scope plan written into next-session. Unchanged risk profile.

**Round 2 (same day) — eval verdict + fixes.** On-device: screens look
good; **axis labels are keepers**. Three fixes from the eval, flashed:

1. **`L` toggle now persists**: `axis_labels` moved into `GraphState`
   (default on), saved on toggle. Persistence magic bumped **PCG2→PCG3**
   — one-time state reset on first boot with this build (the loader
   falls back to defaults + Phase 1 legacy-file migration, so ancient
   yfuncs.txt content may resurface once; re-enter window/mode and
   resave).
2. **`rand()` rendered as `rand())`** in history: the layout parser's
   empty-arg-list path let `parse_expr` consume the `)` as a stray
   one-char text atom, and `make_paren` then drew its own. Fixed in
   `layout_builder.cpp` (empty args → empty text node); host layout
   test added (suite 33→37).
3. **Tick labels capped at 4 significant digits** (`%.4g`): ZTrig's
   pi/2 steps printed full double precision. **KIV (recorded in
   next-session)**: symbolic ticks (pi, pi/2) for irrational steps, and
   more broadly surd-form / fraction / pi-fraction *answer* display.

---

## 2026-07-18 — Session 9: offline-spin verdict, items 8+9 re-fixed & verified, D18 Pico 1 deferral

**Offline spin on 079a8b2: 8/10 Session 8 fixes verified on-device** — including
the split-F1 trace toggle (the one code inspection couldn't reproduce) and
MODE/graphstate persistence. The two failures were re-root-caused, fixed,
flashed, and **verified on-device the same session**:

- **Item 8 (held-key scroll overrun): the Session 8 drain was a no-op.**
  `Keyboard::poll()` is a two-phase machine — the first call only selects the
  FIFO register and returns kNone; the read lands $\geq10$ ms later. The drain loop
  broke on that first kNone, so it still consumed one event per frame. New
  `Keyboard::fifo_empty()` reports whether the last *completed* read found the
  FIFO empty; the main loop now drains through mid-phase kNones until a real
  empty read (event cap 16 + 250 ms budget as wedge guards). Verified: scroll
  stops at release.
- **Item 9 (battery staleness): the ~1 s target was never achievable** — the
  1 Hz main-loop check read a cache whose internal I2C refresh was still 30 s
  (worst case ~31 s; the observed 2-3 s was a lucky phase). Battery API split:
  `battery_status()` is now cache-only (render-safe, no I2C ever);
  new `battery_poll()` — sole call site, the main loop's 1 Hz check — owns the
  refresh. Cadence set to **5 s by developer call** (stability over a snappy
  charging indicator; an STM32 wedge needs a physical power cycle). Serial
  `battery:` prints on change + 30 s heartbeat instead of every read.
  Verified: status bar follows a charger plug within ~5-6 s — which also
  **confirms the charging-bit decode** (battery was at 84%).

**D17→D18: Pico 1 Phase 2 pass deferred to post-Phase 3** (board swap is
tedious; the board-conditional surface is 4 files; clip logic is shared and
Pico-2-exercised; RP2040 static RAM is 62.5 KB of 264 KB). Folds into Phase 3
task 3D.14. Guardrail added to phase3-spec §8: new `render()`s must be
strip-safe (idempotent, ~$20\times$/frame on Pico 1; no host coverage exists).

**Flash-path revision:** the RP2350 BOOTSEL volume mounts again and
cp-to-volume is preferred — `picotool load` hung for minutes at a black
screen. Gotcha recorded: BOOTSEL reboot needs `picotool reboot -f -u`
(plain `-f` reboots into the application).

**Usage-observation round + six improvements implemented (same session).**
Observations logged (`testdrive-phase2-observations.md` §"round 2"),
designs settled in a quiz with the developer, then implemented:

1. **Graph status bar + top-bleed clip**: GraphScreen now draws the
   status bar (title shows GRAPH FUNC/PARAM/POLAR) and confines plot
   drawing to the plot rows via a tightened pane clip (restored for
   chrome; split panes unaffected — Framebuffer gained pane rect
   getters).
2. **Square ZStandard**: default window y = $\pm8.75$ (= 10·280/320), so
   the standard window is square as displayed.
3. **Typed commands** on home: `cls` (session-level scrollback clear
   via display watermark — recall walk and history.txt untouched),
   `clrhist` (full wipe incl. history.txt), `help`, `diag`, `files`;
   grey right-aligned "type help" hint on the empty input line.
4. **Case-sensitive input** (D19): lowercase folds removed from
   preprocess + store op; `2->A` errors pointedly; `1E10` literals
   still fine (strtod). Stored-var echo now prints lowercase.
5. **DEL/SPACE semantics**: DEL clears (editor rows; WINDOW/table-setup
   fields edit-from-empty; ASK-table row delete), SPACE toggles slot
   enable in editors.
6. **F-key remap** (D20): global F1 editor / F2 window (table: setup) /
   F3 mode / F4 trace / F5 graph↔table, Alt+F5 split (HW-verified via
   diag key echo before implementing), `-`/`=` zoom, F6-F9 freed,
   global F6 diag toggle removed, FILES left the diag screen. New
   shared `apps/nav.{hpp,cpp}` (push_mode_editor, goto_graph_trace).
   Help KEYS/SYNTAX tabs rewritten for the new map.

Both boards build; lint clean; host tests 105+33+72 = 210 checks green
(new case-sensitivity coverage). Flashed to the Pico 2 (cp to BOOTSEL
volume). Next: on-device sweep of the remap + fixes, then close out
Phase 2 (2.24 done, judge 2.25, scope 2.22) → retro → Phase 3.

## 2026-07-17/18 — Session 8: THE Phase 2 test drive (2.24) + same-session fixes

**The full 2.24 checklist passed on the Pico 2** (Pico 1 deferred). Full record:
`docs/notes/testdrive-phase2-observations.md`; raw serial + 2.25 perf baseline in
`docs/notes/testdrive-serial-2026-07-18.txt`. Perf headline: recompute is nowhere
near the bottleneck — parametric pair ~1.0-1.4 ms, function set ~5 ms, polar
~2-5.3 ms, split ~4.1 ms vs the ~200 ms frame push.

**Bugs found on-device, all root-caused and fixed same session (079a8b2):**

- Stuck PSRAM/SD `FAIL` on the diag screen after a cold boot: the D14 late-init
  loop never re-ran self-tests that failed while rails were marginal. Now retries
  inside the 30 s window and prints late-init events over serial.
- Screen-stack leak: editor F4 *pushed* graph even when the editor sat on top of
  it; at kMaxDepth every push silently no-oped ("F4 stops working, ESC fixes
  it"). New `ScreenManager::switch_to` pops/replaces instead.
- Cardioid missing its final arc in degree mode: sweeps dropped the partial last
  step (up to one full step short of THmax). Sources now emit a final sample
  clamped to the range end; radian defaults had the same ~1.9° gap unnoticed.
- Held-key table scroll overran after release (event backlog): main loop now
  drains the key queue before each render.
- Home status bar: tall pretty-printed history entries drew over it (layout
  height checked only after drawing), and battery %/charging went stale under
  event-driven rendering. History now clips at the bar; battery cache polled
  ~1/s with a status-band repaint on change.
- Charging flag read bit 7 of the echoed register ID (always 0) — the Session 6
  "low byte assumption" was wrong. Now `raw & 0x8000` (value-byte bit 7) + a raw
  serial print for confirmation below 95%.
- DEG/RAD (and display mode / FIX digits) reset every boot: MODE-row settings now
  persist in graphstate.dat (**magic bumped to PCG2** — one-time state reset).
- Label lies: ESC/F4 pop to the *previous view* (behavior endorsed, wording
  fixed); table softkeys now standard divided cells; window footer fixed.

**Features from test-drive requests (same commit):** expression eval in all
numeric entry fields via `math::eval_field` (`2*pi`, `pi/180`; parse errors keep
the old value — strtod used to silently commit `2*pi` as 2.0); editor rows that
fail to compile render red; FILES screen (diag F6 → F5) lists /picocalc via the
existing `Storage::list_dir`; home F1 opens the mode-appropriate editor.

**Split-trace note:** "can't start trace inside split" was reported, but code
inspection says F1 is forwarded to the graph pane correctly and the split always
full-redraws. Softkey bar + help now advertise F1; verify the toggle on-device.

**Flash-path note (Pico 2):** macOS stopped mounting the `RP2350` BOOTSEL volume
this session; `picotool info` still saw the device, and `picotool load <uf2>` +
`picotool reboot` flashed fine. That is now the preferred path.

Both boards build; lint clean; 079a8b2 flashed to the Pico 2. Next: developer's
longer offline spin, then fix verification + the Pico 1 pass (see HW-PENDING).

## 2026-07-12 — Session 7: lint baseline clean + Phase 2 start (task 2.1 graph/ extraction)

Two commits: `lint: clang-tidy baseline clean`, `graph: extract Viewport + Plotter`.

**Lint cleanup (closed the "clang-tidy not installed" backlog item):**

- clang-tidy was installed via Homebrew `llvm` (keg-only → `lint.sh` now prepends
  `/opt/homebrew/opt/llvm/bin` when needed).
- Two silent problems found: (1) every TU failed to find newlib/libstdc++ headers
  (clang-tidy replays `arm-none-eabi-g++` commands but doesn't know GCC's built-in
  include paths — `lint.sh` now queries g++ for its search list and passes
  `--extra-arg=-isystem` per dir); (2) clang-tidy exits 0 on plain warnings, so lint
  "passed" while reporting them — `.clang-tidy` now sets `WarningsAsErrors: '*'`.
- Config was also backwards for this codebase (wanted `kmax_depth`-style constants,
  UPPER_CASE enum constants, no `#pragma once`); fixed to kCamelCase + prefix-k and
  dropped checks wrong for embedded (`.at()` needs exceptions; MMIO = fixed-address
  derefs; unchecked printf returns; cognitive-complexity on key dispatchers).
- ~50 mechanical fixes auto-applied (const-correctness, std::min/max, bool literals)
  — **caution**: `--fix` corrupted 4 `const char* arr[]` declarations into invalid
  `const char const*` and const-ified a written-through pointer in
  `Framebuffer::fill_rect`; all caught by rebuild + rerun. `misc-const-correctness`
  pointer analysis is now off (that FP class).
- Real code fixes: `atof/atoi` → `strtod/strtol`; factorial rewrite + `strip_zeros`
  use bounded copies (strcpy/strcat gone); grid-line loops use integer counters
  (float accumulation); `quiet_NaN()` replaces `0.0/0.0`; `Storage::read/write_string`
  const; `rand()` NOLINTed (seeding stays backlogged).

**Phase 2 task 2.1 — `graph/` extraction (per phase2-spec §2/§4):**

- New `src/graph/`: `viewport.{hpp,cpp}` (data↔pixel transform, Phase 1 formulas
  verbatim incl. the width-1 x-spacing), `plotter.{hpp,cpp}` (`PlotStyle`,
  `PointSource` interface, `Plotter` with the 140px discontinuity heuristic and
  the +/-1000 py clamp).
- Design call: GraphScreen **keeps its int16 column cache** (it's what makes trace
  redraws cheap) and replays it through `Plotter`'s pixel-space `begin()/point()`
  path; `Plotter::plot(PointSource&)` feeds the same path, so cached function mode
  and the future evaluate-on-plot modes (parametric/polar) share one segment logic.
- `value_to_py` and all duplicated transform math in recompute/draw_axes/draw_trace
  deleted in favor of `Viewport`.
- New host test `tests/host/test_graph.cpp` (18 checks) locks the transforms to the
  Phase 1 formulas — the refactor is provably behavior-preserving on the math side;
  a visual HW spot-check is queued above.
- **Deviation from spec §14**: trace was NOT generalized into `graph/trace.hpp` in
  2.1 — today's trace is a pixel-column walk over the function cache, and there's no
  second mode yet to generalize over. Folded into task 2.7 (parametric trace).

**Phase 2 task 2.2 — `graph::Mode` + mode-aware `GraphState`:**

- `graph/graph_mode.hpp`: `Mode` (kFunction/kParametric/kPolar — kCamelCase per
  codebase+lint convention, not the spec's UPPER_CASE) + `ModeDescriptor`/
  `descriptor_for()`. Slot counts 7/6/6 per §1/§9 (§3's "(function/polar)" comment
  saying 7 read as a typo). Polar descriptor's `independent_var = 0` = engine theta
  slot.
- `graph/graph_state.hpp`: `GraphState` = mode + Y/parametric/polar slots + shared
  x/y window + t/theta ranges (defaults §5.2/§6.2) + `TableConfig` (§7.1, lives in
  graph/ for layering). **Nested structs, not the spec's flat list**, so Phase 1
  screens keep their `GraphWindow&`/`YFunctions&`; save/load deferred to 2.23 with
  the migration (no dead persistence code).
- `apps::graph_model`: structs moved to graph/, apps keeps `using` aliases —
  zero churn in screens; globals are now references into `graph::state()`.
- Parametric/polar slots use `config::kMaxExprLen` (256) per §9; Y-slots stay 96
  until 2.23 (yfuncs.txt buffers sized to it).
- test_graph.cpp grew descriptor + defaults checks (host total now 144).

**Phase 2 tasks 2.3 + 2.4 — FunctionSource, engine sweep slot, ParametricSource:**

- `graph/function_source.{hpp,cpp}`: PointSource iterating viewport pixel columns
  through `eval_compiled` — Phase 1's recompute inner loop wrapped; GraphScreen now
  fills its cache from it (behavior identical).
- `Engine::eval_compiled(handle, var_slot, value)` overload — Phase 1 hardcoded the
  X write (§5.3/§14); 2-arg form delegates with the X slot. Parametric sweeps
  `'t'-'a'`; polar (2.8) will sweep `Variables::kTheta`.
- `graph/parametric_source.{hpp,cpp}`: t sweep with integer step counter +
  endpoint slack (1e-9) — `[0,2pi]` at `2pi/63` emits exactly 64 points; a point is
  defined only when both x(t) and y(t) are finite.
- Host tests: swept-slot checks (t/theta/X-compat), unit-circle sweep, degenerate
  zero-step case. Host total now **161 checks** (79 math + 33 layout + 49 graph).

**Phase 2 task 2.5 — SlotEditorScreen base + parametric editor (D15):**

- Editor architecture decided (D15): one `apps::SlotEditorScreen` base owning
  selection, InputLine editing, dirty-band row invalidation, key dispatch, and
  the render loop; per-mode subclasses provide labels/text/toggle/clear/checkbox
  + an `after_commit` hook. Chosen over both "one mode-aware Y= editor with
  if-branches" and "three duplicated screens" — the D13 invalidate footgun now
  lives in one file, and polar (2.9) should be ~50 lines.
- Commit 1 was a pure extraction (Y= behavior unchanged — same constants, same
  draw calls); commit 2 added `ParamEditorScreen`: 6 pairs as 12 rows (22px),
  pair checkbox on the X row, auto-enable when both halves non-empty, X-commit
  auto-focuses an empty partner (§5.1), F3 clears one field and drops the
  enable when the pair goes incomplete.
- **Not yet wired**: no navigation reaches the parametric editor until the mode
  selector (2.22), and its slots don't persist until the GraphState migration
  (2.23) — both deliberate, per spec task order.
- HW-PENDING: Y= editor visual/behavior spot-check after the extraction (queued
  with the 2.1 graph check).

**Phase 2 tasks 2.6 + 2.7 — mode-aware window screen; parametric plot + trace:**

- WindowScreen: fixed 6-field mapping → mode-driven field table; parametric
  prepends Tmin/Tmax/Tstep (§5.2 order) pointing into `graph::state()`; rows
  tighten to 28px when 9 fields; selection re-clamped on activate. Polar theta rows
  = 3 more table lines at 2.10.
- GraphScreen mode-branched: parametric recompute sweeps `ParametricSource` per
  complete enabled pair into a clamped (px,py)-per-step cache
  (`kMaxCurvePoints` = 340/pair ≈ 8 KB; default Tstep uses 64; tiny Tstep
  truncates — documented limitation). Undefined steps keep their index so the
  trace t-readout stays aligned.
- Trace generalized to `graph::TraceCursor` (index = pixel column in function
  mode, parameter step in parametric; clamped stepping host-tested). UP/DOWN
  cycles the active slots of the current mode; readout "P<n> t= x= y=".
- Graph F5 now pushes the parametric editor in parametric mode ("PAR" softkey).
- Host total **165 checks**. Week 11–12 subtotal (2.1–2.7) is code-complete;
  acceptance ("circle + Lissajous plot with trace") needs mode switching on
  device → argues for pulling a minimal 2.22 forward.

**Minimal 2.22 pull + built-in help (2.26–2.28, pulled from week 16):**

- MODE screen "Graph mode" row cycles FUNC<->PARAM (`graph::state().mode`);
  polar joins the cycle with 2.8–2.11. Mode unpersisted until 2.23.
- `math/catalog.{hpp,cpp}`: `FnDescriptor` table for all 17 parser functions;
  `build_lookup` registers functions by iterating it (variables stay inline).
  Host test asserts every catalog signature parses via the real engine — the
  drift guard §10 asks for. `kLookupCount` sized by `kMaxCatalogEntries` (32).
- `apps/help_screen`: FUNC tab (catalog-driven: signature + summary), KEYS tab
  (per-screen key map incl. PARAM-era keys), SYNTAX tab (store op, e/E rules,
  ans, factorial, angle mode, graph modes, history). Home F5 = "HELP"
  (previously unassigned). All content in flash — no SD dependency.
- **Content caveat**: KEYS reflects the current keymap; the F-key layout
  rethink (feedback 7) is still open — revise help strings if it lands.
- Host total **183 checks** (97 math + 33 layout + 53 graph).

**Polar week (2.8–2.11):**

- `graph/polar_source.{hpp,cpp}`: theta sweep (integer step counter + endpoint
  slack, same as parametric) writing `Variables::kTheta`; Cartesian conversion
  honors angle mode (§6.3) — in degree mode the range is degrees and
  `r*cos/sin` converts accordingly. Host tests: cardioid (radians, 64 points,
  starts (2,0)), r=1 circle in degree mode hitting the four axis points.
- `apps/polar_editor`: SlotEditorScreen subclass — r1..r6, palette label
  colors, auto-enable on non-empty, ~50 lines (D15 paying off as predicted).
- WindowScreen: polar prepends THmin/THmax/THstep; the name column now sizes
  to the longest field name (function/param layout unchanged at 5 chars).
- GraphScreen: `recompute_polar` shares the parametric (px,py)-per-step cache
  (only one parameter mode is active at a time — no extra 8 KB); helpers are
  now mode-switched (`param_style()` = parametric|polar); F5 routes to the
  polar editor ("POL" softkey); trace readout "r<n> th= x= y=" with theta in
  current angle-mode units. Theta var saved/restored around the sweep.
- MODE screen graph-mode row cycles FUNC/PARAM/POLAR; help KEYS/SYNTAX updated.
- Host total **188 checks** (97 math + 33 layout + 58 graph).
- Acceptance queued for HW: cardioid `1+cos(theta)`, rose `2*sin(3*theta)`,
  both angle modes (spec week-13 acceptance).

**Task 2.23 (pulled from week 16) — unified GraphState persistence:**

- `GraphState::save/load` → `/picocalc/graphstate.dat`: "PCG1" magic +
  size-guarded raw image (GraphState is static_assert'd trivially copyable).
  Layout change = bump magic → old image rejected, defaults/migration apply.
  Impl in `graph/graph_persist.cpp` so host-test links stay platform-free;
  6.5 KB image buffer is static (Pico stack is too small).
- `apps::load_graph_state`: unified image first; on miss, one-time migration
  from window.dat + yfuncs.txt, then writes unified. Old files ignored, not
  deleted. `save_functions`/`save_window` = thin wrappers over the full save;
  param/polar editors + MODE graph-mode row now persist their changes.
- Y-slots grew 96 → `config::kMaxExprLen` (256) per §9 — safe now that the
  yfuncs.txt writer is gone (its buffers were the 96 constraint).
- HW-PENDING: first boot after flash must migrate existing Y-funcs/window;
  parametric + polar curves and mode must survive a cold power cycle.

**Tasks 2.12–2.18 — table view:**

- `apps/table_model.{hpp,cpp}` (split out of table_screen for host-testability
  — not in the spec's file list): column mapping per mode (enabled slots in
  order, gaps skipped; parametric = two columns per pair, §7.3) and
  `evaluate_table_row` (per-slot compile/eval/free; syntax errors → NaN
  column). Per-row compile is the simple/spec-shaped path — if scroll feels
  slow on HW, the lever is compiling once per regenerate (note for 2.25).
- `TableSetupScreen`: Start/Step/Independent(AUTO/ASK); immediate-apply +
  unified save (deviation from the §7.1 mock's F1:SAVE/F2:CANCEL — matches
  the WINDOW screen convention; revisit if it feels wrong on-device).
- `TableScreen`: 17 visible rows evaluated into a cache once per change
  (dirty_ flag — same strip-render pattern as GraphScreen); auto mode
  scrolls infinitely both directions (row n can be negative); ask mode holds
  up to 32 entries (oldest dropped when full; ENTER adds via the detail-line
  InputLine, F5 deletes); LEFT/RIGHT shifts the 3 visible dependent columns
  with </> overflow markers; detail line = full-precision selected row.
- Entry point: **Graph F4 "TBL"** (was the free softkey slot); F3/ESC pops
  back to the graph. G-T split (F4 in the mock) comes with 2.19.
- Host total **202 checks** (97 math + 33 layout + 72 graph).

**Tasks 2.19–2.21 — split-screen graph|table (D16):**

- `gfx::Framebuffer::set_pane_clip/clear_pane_clip` (§8.1): a rect composing
  with the strip window, enforced in set_pixel + fill_rect (all primitives
  funnel through them — verified before touching this HW-verified code).
- **Horizontal split** (P2-1 → D16): graph pane rows 0–138 at full 320px
  width — column caches, trace x-mapping, and plot code untouched; only the
  viewport height shrinks. Table pane rows 142–298 (7 rows), divider between,
  white edge marks the focused pane.
- GraphScreen/TableScreen: geometry constants became members with
  `set_pane`/`reset_pane` (set_pane marks dirty — cached py depends on
  height). SplitScreen **reuses the singletons** — no duplicated caches, no
  forked trace/window state; on_deactivate resets panes so pushed screens
  (setup, editors) render full-screen and return cleanly.
- Trace sync (2.20, option c nearest-row): `GraphScreen::trace_value/
  sync_trace_to_value` + `TableScreen::selected_value/highlight_value` —
  works in all three modes (pixel column in function, nearest parameter step
  in parametric/polar). Option b (trace steps by table-step) KIV after the
  test drive.
- Keys (D16): F4 = switch graph↔table (full-screen) / pane focus (split);
  F9 (Shift+F4) toggles split from graph or table; ESC exits; table-focused
  F3 exits (its "GRAPH" meaning) while graph-focused F3 still zooms out.
- HW-PENDING: split rendering on the Pico 1 strip renderer (clip-rect
  compose), sync feel, pane sizes.

---

## 2026-07-11 — Session 6: STM32 fw v1.6 (battery works) + dirty-band partial rendering

**Developer hardware work**: updated the STM32 keyboard firmware to **v1.6** (not the
v1.2 the notes suggested; `PicoCalc_BIOS_v1.6.bin` is in the repo root). **The battery
indicator now works on-device** — the missing register 0x0B was indeed just old
firmware; no code change was needed, as predicted. Still to check under v1.6: charging
color/bit, phantom keys after a battery refresh, Shift-on-arrows, F10 (see the queue).

**Dirty-band partial rendering** (task 5.6 part 2, decision **D13**) — the last big
Phase 1 code item. The ~200 ms per-keypress latency is SPI push time, which scales
with pixel count, so:

- `ui::Screen` grows opt-in dirty-band tracking: `track_dirty()` + `invalidate(y0, y1)`
  (row band, full width); `take_dirty()` consumed per frame. Non-tracking screens
  keep full-frame redraws; `ScreenManager` fully invalidates any screen that surfaces
  to top of stack.
- `Framebuffer::render_frame()` takes the band and renders/pushes only those strips
  (both strip mode and the Pico 2 full-buffer path, which now treats its buffer as
  scratch). Empty band → render skipped entirely.
- Home screen: typing/recall/ESC = input band (~28 of 320 rows → ~20 ms expected);
  Enter = everything above the softkeys (also refreshes battery/mode status); history
  scroll = history band. Y= editor: per-row bands (~26 rows) for edit/select/toggle.
- Graph screen intentionally left full-frame (trace/zoom touch ~280 rows anyway — D13
  "revisit when").

Both boards build; 106 host checks green (host tests don't cover `ui/`).

**HW verification round 2 (same session, live with the developer over USB serial):**

- **Dirty-band rendering verified on Pico 1**: typing/recall/ESC instant, Enter,
  history scroll, Y= editor select/edit/toggle, all screen switches — no stale rows
  or seams anywhere. 5.6 closed.
- **Battery**: % displays correctly (100%). Found + fixed: on cold power-on the STM32
  is still booting when the first frame renders, so the first read failed and the
  10 s backoff pinned "--" in the status bar. `battery_status()` now has a 10 s boot
  grace (2 s retries that don't count toward the give-up cap). Verified after a
  power cycle: % appears at the first keypress ~2 s in.
- **STM32 v1.6 keyboard behavior unchanged from v1.2**: Shift+arrows still swallowed
  (diag serial shows the kShift press arrive with *no* arrow event), F10 still never
  emitted (Shift+F5 → F5). D12's Alt/Ctrl view-scroll stands. No phantom keys after
  a 40 s idle spanning a battery refresh.
- **Charging color inconclusive**: at 100% the charger is idle, so the charging bit
  being 0 proves nothing. Retest when the battery is below ~95% (queue row).

**HW verification round 3 (same session): rest of the queue cleared on Pico 1.**

- **Pretty math**: mostly right on first look, but `1/sqrt(2)` rendered inline —
  a function call parses to an HBox, which D2's is-simple check didn't accept.
  Fixed: calls (recognized structurally, `HBox[alpha-name, paren]`) and
  superscripts now count as simple operands, so `1/sqrt(2)` and `x^2/2` stack
  (D2 revision; 6 new layout tests, 33 total). Re-verified on-device.
- **Verified**: store op `2->A`/`A+1`→3 (`-` and `>` type fine), trace + S/T
  presets, mode toggles (status bar follows), reboot-to-bootloader (mounted
  RPI-RP2 — used it for the final reflash), SD card r/w self-test OK on the
  diag screen + history persistence. That completes the 5.7 exit test on Pico 1.

**Pico 2 (RP2350) bring-up (same session, continued): display path works;
cold-boot PSRAM/SD root-caused (D14).**

- **First flash ever on Pico 2.** The untested full-framebuffer display path
  **works**: home screen renders, diag shows "Pico 2 (RP2350)", color bars
  correct, keyboard + battery fine. (BOOTSEL volume is `RP2350`, not `RPI-RP2`.)
- **PSRAM + SD both failed on cold power-on, both fine on warm reboots.**
  Debugged over three instrumented flash cycles (buffered init trace dumped
  over USB serial after boot; timestamps). Measured: PSRAM reads zeros at
  0.5 s, near-correct data (bit-shifted — analog marginal, same at 75 MHz and
  18.75 MHz SCK) at 0.6-2.5 s, perfect at 7.5 s. SD answers CMD0/CMD8 cold but
  never completes ACMD41 until ~7.5 s, then inits instantly (R7 voltage echo
  clean). **The peripheral rail needs ~5-8 s to settle after cold power-on
  with the Pico 2 module**; Pico 1 doesn't show this. Matches community
  reports (RP2350 PSRAM cold-boot failures; fuzix SD failure on PicoCalc
  Pico 2, clockworkpi/PicoCalc#12).
- **Fix (D14): deferred late-init.** Boot stays instant; the main loop
  retries PSRAM (`Psram::reinit()` — chip reset via the existing PIO, no
  PIO/DMA re-allocation) and SD (`Storage::init()`) every 2 s for the first
  30 s. Late storage arrival re-runs self-tests + loads history/variables/
  graph state and refreshes the screen.
- Debug harness worth remembering: `stty -f /dev/cu.usbmodem* 1200` works on
  RP2350 for no-touch reflash; boot prints race USB enumeration, so buffer
  init logs and dump them once `stdio_usb_connected()` (or just late).
- **Verified on Pico 2 (cold power-on, 2026-07-12):** with the D14 late-init,
  a cold boot ends with the diag screen showing PSRAM OK + SD OK (came up
  before the developer even opened diag). Instant boot preserved.

Full flash-test-fix loop on Pico 1 with live USB-serial capture. Three flash cycles.

**Verified working** (removed from the HW-PENDING queue): evaluation/history, input
recall + view scroll, `e`/E-reserved, HOME key, ESC-exits-diag, graph colors, shifted
F-keys, graphing with zoom, and the `1e10` display fixes from this session.

**Graph profiling (5.6 part 1) DONE**: `graph recompute:` 15.3-17.1 ms for two enabled
functions (sin(x), x^2-3) on Pico 1 — well under the 50 ms target. Evaluation is not
the bottleneck; the ~200 ms full-frame display push is (dirty-rects = 5.6 part 2).

**New bugs found on-device and fixed** (HW-found, host tests added where possible):

1. **`1e10` displayed as "1" / result "10e9".** Two independent bugs: (a) the layout
   builder had no scientific-literal support and silently rendered a *truncated* tree —
   fixed by consuming `[eE][+-]?digits` after a number AND falling back to whole-string
   plain text whenever input is left unconsumed (also un-breaks `2->A` and multi-arg
   calls in history); (b) the Pico SDK's printf emits unnormalized `%e` mantissas at
   exact powers of ten ("10.000000000e+09") — host libcs don't, so host tests couldn't
   catch it; `normalize_mantissa()` fixes the output in both sci paths. +10 host checks.
2. **STM32 keyboard-controller forensics** (diag key echo over serial):
   - Shift on arrows is swallowed (STM32 emits shift-release, then a plain arrow) →
     view-scroll rebound to **Alt/Ctrl+UP/DOWN** (both pass flags through; D12 revised).
   - Shift+F1..F4 → scan codes 0x86-0x89 (F6-F9) confirmed. **Shift+F5 emits plain
     F5** — F10 does not exist in this STM32 firmware (decode keeps 0x8A for future fw).
   - **The battery register (0x0B) is unsupported by this unit's STM32 firmware**:
     reg 0x01 answers, 0x0B times out (100 ms) at select or read. Indicator shows "--";
     `battery_status()` stops after 5 consecutive failures until reboot.
   - **Hammering the STM32 wedges it** (the Session-4 build's broken backoff retried
     every strip): keyboard dead until a *physical power cycle* — the STM32 is not
     reset by USB reflash. Rule: pace all STM32 traffic, never poll aggressively.

Both boards build; 106 host checks (79 math + 27 layout). Battery debug instrumentation
removed after verification. Remaining for phase 1 close-out: SD/persistence (FAT32 card),
store-op typing, trace/S+T presets, mode/reboot 5.3, full exit test 5.7, Pico 2 bring-up.

---

## 2026-07-11 — Session 4: battery level indicator; first HW-test attempt stalled

HW verification of the polish fixes started: new firmware flashed to Pico 1 over the
1200-baud reset, serial capture attached. The diag-screen key tests were run **but the
serial log came back empty** — then the developer suspected a flat battery and paused.
Diagnose the capture path when the device returns (the key echo should print every
press; an empty log means the capture, power, or USB path failed — not the firmware).

That pause surfaced a gap: nothing displays battery level. `platform::read_battery_info()`
existed since M1 (STM32 register: percent + charging bit) but had no UI. Added:

- **`platform::battery_status()`** — cached accessor: refreshes at most every 30 s and
  only while the keyboard poll state machine is idle (new `Keyboard::bus_idle()`).
  The raw read blocks ~16 ms and shares the 10 kHz I2C bus with the keyboard's
  two-phase FIFO poll — an interleaved read would be decoded as FIFO data and produce
  phantom key events, hence the guard.
- **Status bar** (chrome, all screens): battery icon + percent at the far right; green
  above 50%, amber 21-50%, red at 20% and below, cyan while charging; "--" when
  unavailable.
- **Diag screen**: "Battery: N% (charging)" line.

Both boards build; 96 host tests still pass (feature isn't host-testable — HW rows
queued). **The on-device firmware is now one build behind** — reflash before resuming
the HW checklist.

---

## 2026-07-11 — Session 3: Phase 1 polish from test-drive feedback

Implemented all five actionable items from the Session-2 feedback entry (decisions
D11, D12). Both boards build; host tests 96/96 (6 new `e` checks); clang-format run.

1. **Graph colors** — new `colors::kGridLine` rgb(60,60,60) for grid lines (axes stay
   white); Y7 palette slot dark green → yellow rgb(250,220,40).
2. **Diag screen exit** — ESC now pops the overlay (F6 already toggled); added an
   on-screen "F6 or ESC exits." hint. Removed `GraphScreen`'s unreachable F6 handler
   and fixed its softkey label (F6 said "Y=", now "DIAG").
3. **Input history (D12)** — UP/DOWN walk past inputs shell-style (in-progress line
   stashed and restored); Shift+UP/DOWN scroll the history view. Supersedes 5.5's
   UP-on-empty single recall.
4. **`e` constant (D11)** — `build_lookup()` no longer binds letter 'e', so tinyexpr's
   builtin Euler constant resolves; `->e`/`->E` store returns "E is reserved".
5. **HOME key** — global intercept in `main.cpp` (like F6): pops to the home screen
   from any screen via new `ScreenManager::pop_to_root()`; on the home screen it
   falls through to the input line's cursor-to-start.

README per-screen usage updated. On-device verification for all five queued in the
HW-PENDING table (the shift+arrow reporting is the one real HW unknown — D12 names
the fallback). KIV: F-key layout rethink (feedback item 7).

**Addendum (same day)**: developer corrected the F-key picture — F1-F5 are physical,
F6-F10 are Shift+F1-F5 *translated by the STM32 into distinct scan codes* (that's how
F6 opened diagnostics). Extended `Key` to kF10 and decode to 0x8A (0x87-0x8A assumed
from the 0x81-0x86 pattern; HW-PENDING). This translation behavior cuts both ways for
D12: the STM32 demonstrably remaps shifted non-printables, so Shift+UP/DOWN may well
arrive as something other than arrow-plus-shift-flag — verify early.

---

## 2026-07-11 — Session 2: Phase 2/3 specs imported + consistency pass

Imported the developer's drafts of `phase2-spec.md` and `phase3-spec.md` (from
`picogc_phase2_3.zip`) into `docs/phases/` and reconciled them against the Phase 1
code and the committed Phase 4 spec.

**Added**: built-in help planned into Phase 2 (new §10; tasks 2.26–2.28, ~9 hrs;
total now ~110 hrs). `HelpScreen` with Functions/Keys/Syntax tabs, entry Home F5
(unassigned in Phase 1). Function catalog driven by a `math::catalog` descriptor
table that `build_lookup()` also consumes — one source of truth so help can't drift
from the parser. Motivated by test-drive feedback item 6 (entry below).

**Consistency fixes applied to the drafts**:

- phase2 §5.3/§6.1: corrected engine claims — `eval_compiled` hardcodes X (task 2.4
  parameterizes the swept slot), and `theta` is its own variable slot (`kTheta`),
  which the polar sweep drives (draft claimed theta would be aliased to `t`).
- phase2 §8.1/§14: framebuffer clips strip-only (vertical); split-screen needs a real
  clip-rect — added to task 2.19 (draft assumed clipped rendering already sufficed).
- phase2 §12: removed unmeasured "~400K evals/sec benchmarked" figure (5.6 is still
  HW-PENDING); replaced with a pessimistic bound.
- phase2: `TableConfig` moved `apps::` → `graph::` to keep apps→graph layering;
  header/total hours fixed (~120 vs ~101 → both ~110 incl. help).
- phase3: header hours 160→170 (matches task tables); dropped "reuse Phase 1's
  numeric solver" (none exists — the solver is Phase 4 §3.4; 3C uses a local
  bisection); `extern` globals → singleton accessors per project convention; noted
  tinyexpr is fixed-arity (no default args) for parser registration (3C.7).
- phase4-spec: weeks renumbered 16–25 → 26–35 (it predates Phase 3's existence;
  Phase 3 occupies weeks 17–25).
- README Phase 2 line updated ("multi-function graphing" already shipped in Phase 1).

**Left as-is (deliberate)**: Phase 2 `GraphState` sizes slots at `kMaxExprLen` (256)
vs Phase 1's 96-char `YFunctions` — persisted struct grows to ~6.4 KB, fine; the
Matrix-vs-Array reconciliation stays deferred to Phase 4 start (phase3 §10 records it).

---

## 2026-07-11 — Test-drive feedback (on-device, Pico 1)

Developer used the calculator for a while on hardware. Observations, each diagnosed in
code this session. No fixes applied yet — several need a design decision first.

1. **Graph grid too bright.** Grid lines use `colors::kGrayLine` = rgb(200,200,200) —
   near-white on the panel (`graph_screen.cpp draw_axes`). Proposal: add a dedicated
   dark-gray `kGridLine` (~rgb 60,60,60) for the grid; axes stay white. Don't darken
   `kGrayLine` itself — it's also the history-expression text color and hint text.
   Palette on black bg: Y7 dark green rgb(0,120,0) is the dim one; consider yellow.
2. **Can't exit the F6 diagnostics screen.** F6 is a global toggle in `main.cpp` — a
   second F6 press *should* pop it. DiagScreen swallows every other key incl. ESC, so
   the natural exit key does nothing. Fix: ESC pops; draw an "F6/ESC exits" hint line.
   HW question: confirm whether the second F6 press really never arrives (STM32 quirk)
   or the tester only tried ESC. Related dead code found: `GraphScreen::on_key` has a
   kF6 case (unreachable — main intercepts F6 first) and the graph softkey bar labels
   F6 as "Y=" — mislabeled, should be DIAG.
3. **Input history navigation.** Today: UP on empty input recalls only the newest
   expression (5.5); any further UP scrolls the output view. Wanted: shell-style —
   repeated UP/DOWN walks back/forward through past inputs; a separate control scrolls
   the view. Keyboard has no PgUp/PgDn (coyote scan codes), but shift state is tracked.
   Proposal: UP/DOWN = input-history recall, Shift+UP/DOWN = scroll output.
4. **`e` is not Euler's number.** `build_lookup` binds all 26 letters as variables and
   tinyexpr checks the user lookup *before* its builtins (tinyexpr.c base()), so `e`
   resolves to variable E (0.0). `pi` works because it's two letters — never collides.
   Current convention: single letters = variables (case-folded by preprocess),
   multi-letter names = builtins + pi/theta/ans. Workaround today: `exp(1)`.
   Decision needed: reserve `e` as the constant and drop variable E (TI users rarely
   use E; recommended), vs. case-sensitivity (e=const, E=var — needs preprocess rework).
5. **Home key does nothing.** It decodes fine (0xD2 → `Key::kHome`) but only InputLine
   consumes it (cursor-to-start — invisible on an empty line; other screens ignore it).
   Proposal: global intercept in main.cpp like F6 — pop to the root (home) screen from
   anywhere; when already on Home with text in the input, keep cursor-to-start.
6. **Built-in help**: not planned in any written spec (phase 2/3 specs don't exist yet;
   phase 4 is CAS/matrix/MicroPython). Candidate for `phase2-spec.md`: a catalog/help
   screen — function list with signatures + key map. Cheap and high-value on a device
   with no manual.
7. **F-key mapping (KIV).** Corrected 2026-07-11: F1-F5 are direct physical keys;
   F6-F10 arrive via Shift+F1-F5 (the STM32 translates them to their own scan codes).
   That's exactly TI's five top-row keys on the direct layer — a TI-order remap
   (Y=/WINDOW/ZOOM/TRACE/GRAPH) with secondary functions (DIAG, HELP) on the shifted
   layer is the obvious candidate. Watch what feels natural during the next test
   drive before committing.

---

## 2026-07-10 — Session 2: first hardware bring-up (Pico 1)

First flash to a real PicoCalc. Initial symptom: screen of random colors + dead keyboard.
Diagnosed on-device by bisection (vendored-only `picocalc_diag` proved the panel works →
bug is ours) and USB-serial boot tracing. Found and fixed three bugs, all in **D10**:

1. **Boot hang** → the "random colors". `run_self_tests()` used the vendored *bulk* PSRAM
   transfer, which hangs on hardware (single-word PSRAM works). Froze after display init
   but before first draw, leaving power-on noise on the panel. Fix: word-based self-test;
   bulk `Psram::read/write` quarantined (added `read_word`/`write_word`).
2. **Dual-core display stall**. Pushing strips through a core-1 FIFO service stalled on
   frame 1. Fix: render synchronously on core 0 via the vendored blocking `spi_write_fast`
   path (the diagnostic's known-good mechanism). Core 1 idle; DMA/dual-core deferred.
3. **Dead keyboard**. I2C timeouts were 2 ms but a 2-byte read on the 10 kHz keyboard bus
   takes ~3.5 ms, so every read timed out. Fix: `kI2cTimeoutUs = 100 ms`.

Result: **boots to the home screen; display + keyboard confirmed working on hardware.**
Also made rendering event-driven (redraw only after a keypress) since a full-frame push is
~200 ms (5 fps) — removes idle redraws. Removed the debug instrumentation afterward.

Follow-ups: verify the rest of the calculator on-device (needs a FAT32 SD card for
persistence — boot showed sd=0), capture graph-profiling numbers, then the ~200 ms redraw
latency is the main perf item (dirty-rectangle partial updates — task 5.6). See the
HW-PENDING queue above.

---

## 2026-07-08 — Session 1: environment + skeleton + Phase 0 start

**Done:**

- Verified/repaired host toolchain. Quirk: Homebrew *formula* `arm-none-eabi-gcc` lacks
  newlib (`nosys.specs` link failure); the working compiler is ArmGNUToolchain 15.2.rel1
  from the `gcc-arm-embedded` cask. Documented in `docs/dev-environment.md`, AGENTS.md.
- Pico SDK 2.2.0 + pico-examples checked out in-repo (gitignored). picotool 2.3.0 via brew.
- Extracted the project skeleton package; adapted docs to this host; fixed build-dir naming
  (`build/pico`, `build/pico2`), pinned SDK 2.2.0 in CI, scoped C++-only compile flags.
- Initial commit `f76d10c`. Both boards build the blink stub to .uf2.

**Phase 0 status vs checklist:** 0.1.1–0.1.5 done (verified via hello_serial + skeleton
builds), 0.1.6 HW-PENDING, 0.2.* done, 0.4.* done, 0.5 done, 0.6.1/0.6.2/0.6.5 done.
Remaining: 0.3 (vendor drivers), 0.6.3 (optional CLAUDE.local.md — skipped, developer's
call), 0.7 (remote push — no remote configured yet).

**Next:** task 0.3 — clone Coyote OS, vendor `lcdspi/ i2ckbd/ rp2040-psram/ pwm_sound/`,
vendor FatFs R0.15a, record SHAs + licenses.

### Checkpoint: Phase 0 complete (2026-07-08)

- Vendored from Coyote OS `e86cf36d` (2026-02-05): `lcdspi/` (incl. font1/battery fonts),
  `i2ckbd/`, `rp2040-psram/` (MIT, upstream polpo; examples dropped), `pwm_sound/`, plus
  `coyote_reference/` (config.h SD pinout, keyboard_definition.h scan codes — reference only).
  GPL-2.0 text kept as `drivers/LICENSE.coyote-os`; noted GPL implication in dependencies.md.
- Vendored FatFs R0.15a from elm-chan.org (`ff.c`, `ffsystem.c`, `ffunicode.c`, stub
  `diskio.c`; SD SPI glue to be written in task 1.5).
- Phase 0 checklist all [x] except 0.1.6 (HW-PENDING) and 0.6.3/0.7 ([s], developer's call).
- Not yet in build: drivers are intentionally NOT in CMakeLists.txt (per 0.3.4) —
  integrated incrementally in tasks 1.3–1.6.
- Useful discovery: Coyote OS uses tinyexpr (C) and pico-vfs as submodules; we'll use
  tinyexpr++ per spec (task 2.1) and plain FatFs instead of pico-vfs.

### Checkpoint: Milestone 1 (bootstrap) code complete (2026-07-08)

Tasks 1.1–1.9 all [x] in phase1-plan.md; both boards build the diagnostics firmware.
Decisions D6–D9 recorded (RGB666 wire format, async keyboard, FatFs LFN, interim font).

Layer map as built:
- `src/platform/`: display (565→666 push, DMA), keyboard (async poll SM), sd_card
  (own SD SPI driver) + sd_diskio (FatFs glue) + storage (FatFs API), psram (bump
  allocator over PSRAM addresses), system (battery via STM32), platform::init().
- `src/gfx/`: framebuffer (strip ping-pong on Pico 1 / full FB on Pico 2, clipped
  primitives, core-1 display service over multicore FIFO), font (UTFT-format).
- `src/ui/`: Screen base + fixed-depth ScreenManager.
- `src/main.cpp`: core dispatch + DiagScreen (self-tests for SD/PSRAM, key echo).

Notes / known limitations:
- **Full-frame push is ~98 ms @ 25 MHz SPI** (3 B/px wire format) → ~10 fps if the
  whole screen redraws every frame. Fine for milestone-1 accept ("text visible"),
  but the spec's 30 fps target needs dirty-rect updates and/or SPI overclock —
  planned lever for task 5.6. Milestone 2+ UI should avoid full-screen redraws.
- Overclock constant (`config::kOverclockHz`) intentionally NOT applied yet.
- `set_backlight` uses STM32 reg 0x05 (standard PicoCalc fw) — not in the vendored
  driver, needs HW confirmation.
- lint.sh not run: clang-tidy unavailable (Homebrew `llvm` not installed — ~1.5 GB;
  developer's call). clang-format installed (v22) and applied; line width 100.
- Vendored driver C files emit warnings under our -Wall/-Wextra/-Wpedantic (they
  compile as part of our target via INTERFACE libs). Cosmetic; suppress later if
  it drowns signal.

### Checkpoint: Milestone 2 (calculator core) code complete (2026-07-08)

Tasks 2.1–2.8 all [x]; both boards build the calculator firmware; 53/53 host tests pass.
Decisions D1 (store op `->`) and D4 (plaintext history) recorded.

What's new:
- `src/math/`: `Engine` (tinyexpr wrapper + `->` store op + `!`→`fac()` preprocess),
  `functions` (angle-aware trig, ln/log10, nCr/nPr, fac, rand, round, min/max, deg/rad),
  `format_number` (int / 10-sig-fig / scientific), `types` (calc_t=double, AngleMode).
- `src/ui/input_line`: cursor editing (insert/backspace/del/home, horizontal scroll).
- `src/apps/home_screen`: input line + 50-entry ring-buffer history (right-aligned
  results, pretty via format_number), UP/DOWN scroll, F4 angle toggle, SD persistence
  of history (TSV) and variables (binary). main.cpp now boots to HomeScreen; F6 opens
  the milestone-1 diagnostics overlay.

Testing approach (NEW — matters for session continuity):
- `tests/host/test_math.cpp` + `scripts/host-tests.sh` compile the math layer with the
  **host** compiler and assert real values (2.1–2.4, 2.6 acceptance criteria). This is
  how we verify calculator correctness without a PicoCalc. Extend this suite as the
  math/renderer grows. Cross-compile + host-test + doc-update is now the per-milestone
  loop.

Decision worth remembering: used **C tinyexpr** (not tinyexpr++) — see dependencies.md
for rationale (C ABI fits -fno-exceptions/-fno-rtti; extended fns live in our C++).

Known limitations / deferred:
- Results in history are plain text (format_number), NOT yet 2D-typeset — that's
  milestone 3 (the renderer replaces the result string with a layout tree).
- HomeScreen redraws the full frame each key/frame (~10 fps ceiling, see M1 note).
  Acceptable for text entry; revisit with dirty-rects if it feels laggy on HW.
- Softkey bar is a static label placeholder; real softkey dispatch is task 5.2.
- rand() uses libc rand() unseeded — deterministic across boots. Seed from an ADC
  noise source or uptime at first use during polish.

### Checkpoint: Milestone 3 (natural math renderer) code complete (2026-07-08)

Tasks 3.1–3.7 all [x]; both boards build; 74/74 host tests pass (53 math + 21 layout).
Decision D2 (simple-operand fraction heuristic) recorded.

Design:
- `src/render/layout_node.hpp`: one fixed-size tagged-union `LayoutNode`
  (Text/HBox/Fraction/Superscript/Paren). **No virtual functions** — a plain
  `NodeType` tag + switch, so it's RTTI-free (-fno-rtti) and pool-friendly.
- `src/render/pool.{hpp,cpp}`: 8 KB bump allocator, `pool_new<T>()` placement-new,
  reset per build. On exhaustion, builders degrade to plain text (never crash).
- `src/render/layout_builder.cpp`: recursive-descent parser producing sized nodes.
  Grammar: expr(+/-) → term(*,/) → power(^ right-assoc) → unary(-) → atom
  (number | ident | ident(args) | (expr)). Fractions only for simple operands (D2).
- `src/render/layout_render.cpp`: `render_node()` walks the tree; strip-clipped;
  stroked auto-scaling parens for tall content.
- HomeScreen history now renders **expressions** as 2D math (`render_node`); results
  stay plain text (a formatted number is already display-ready, and the layout parser
  would choke on result annotations like "5>A" / error strings).

Testing: NEW `tests/host/test_layout.cpp` asserts node types + sizes for the Phase 1
constructs (fractions, superscripts, parens, function calls, nesting, right-assoc `^`).
This is how the renderer is verified without a screen; the visual/alignment quality
still needs the HW-PENDING check.

Known limitations / deferred:
- Single font size → superscripts are same-size-but-raised, not 75%-shrunk. Fine at
  8x12; revisit if a smaller font is added (would also help nested exponents).
- Fraction vertical centering is approximate (bar sits at the math baseline; adjacent
  inline text aligns to the bar, so it reads slightly "numerator-high"). Cosmetic;
  tune after seeing it on HW.
- SqrtNode deferred to Phase 2 per spec (sqrt renders as "sqrt(x)" text for now).
- Rebuilding history trees every strip (~20x/frame) is wasteful but cheap for short
  strings; optimize with dirty-rects / cached measurement in task 5.6 if needed.

### Checkpoint: Milestone 4 (graphing) code complete (2026-07-08)

Tasks 4.1-4.9 all [x]; both boards build; 81/81 host tests pass. Decisions D3 (trace
readout at bottom) and D5 (keep double, float deferred) recorded. **Phase 1 is now
feature-complete** — only milestone 5 (polish) and hardware verification remain.

New:
- `math::Engine` gained a compile-once/eval-many path (`compile` / `eval_compiled` /
  `free_compiled`) so graphing does 320 evals per function, not 320 re-parses. Shared
  `build_lookup()` helper. GraphScreen saves/restores the user's X around the sweep.
- `src/apps/graph_model`: Y1..Y7 + GraphWindow singletons, 7-color palette, SD
  persistence (yfuncs.txt TSV, window.dat binary), zoom presets/ops.
- `src/apps/y_editor`: Y= list editor (navigate, inline edit via InputLine, enable
  checkbox, clear, jump to graph).
- `src/apps/graph_screen`: column-cached plotting (recompute on dirty), axes+grid,
  discontinuity detection, trace cursor, zoom. Softkeys F1 trace / F2,F3 zoom /
  F5 Y=; keys S,T = standard/trig presets.
- `src/apps/window_screen`: 6-field editor; ESC replots and returns to graph.
- HomeScreen softkeys now F1=Y=, F2=WINDOW, F3=GRAPH, F4=MODE; main loads graph
  state at boot.

Host tests: added compile/eval_compiled coverage to test_math (60 checks now).
The plotting/axes/trace geometry is not host-tested (needs the framebuffer) — that's
the largest HW-PENDING surface this milestone.

Known limitations / deferred:
- **ZoomFit (F4 in spec) not implemented** — needs a y-range auto-scan over enabled
  functions. Left as a small follow-up; F2/F3/S/T cover the common cases.
- **Axis tick numeric labels not drawn** (grid lines are). Deferred to M5 polish;
  needs careful label placement to avoid clutter at 8x12.
- Graph coordinate coloring/labels and the "no functions" hint are basic; refine in M5.
- Trace steps by pixel column (reads the cache), not by evaluating at sub-pixel x —
  fine for a 320px viewport.

### Checkpoint: Milestone 5 (polish) code complete — PHASE 1 CODE COMPLETE (2026-07-08)

Tasks 5.1-5.5, 5.8, 5.9 [x]; 5.6 [~] (profiling hook in, numbers need HW); 5.7 [!]
(HW test, no PicoCalc attached). Both boards build; 90/90 host tests pass.

New:
- `src/ui/chrome`: shared `draw_status_bar` (title + RAD/DEG + FLT/FIX/SCI + 2nd/A
  indicators) and `draw_softkeys` (6 cells). HomeScreen/GraphScreen/ModeScreen use them.
- `src/apps/mode_screen`: angle mode, display format (FLOAT/FIX/SCI), fix digits, and
  "Reboot to bootloader" → `reset_usb_boot(0,0)` for flashing without the BOOTSEL button
  (task 5.8). Reached via Home F4.
- `math::format_number` gained FIX/SCI modes (global `DisplayMode` + fix digits),
  host-tested.
- Expression recall: UP on an empty Home input line restores the last expression (5.5).
- Graph `recompute()` times itself and prints "graph recompute: N us" to USB serial +
  stores `last_recompute_us_` (5.6 profiling hook).
- Error handling verified by host tests: 1/0→Inf, 0/0→NaN, syntax→"Syntax error"
  (shown in red), no crashes (5.4).
- README rewritten: feature list, host-tests section, per-screen usage, flash-from-
  firmware note.

**Phase 1 is code-complete.** The only remaining work is on real hardware (see the
HW-PENDING queue above): boot both boards, run the exit test, confirm SD persistence,
record graph-render timing, then write `docs/notes/phase1-retro.md` and begin
`docs/phases/phase2-spec.md`.

Deferred within M5:
- clang-tidy not run (Homebrew `llvm`/`clang-tidy` not installed — ~1.5 GB; developer's
  call). clang-format IS applied repo-wide each checkpoint.
- ZoomFit (4.7) and axis numeric tick labels (4.4) still deferred from M4.
- 2nd/Alpha status indicators are plumbed through `StatusFlags` but not yet driven by a
  real 2nd/Alpha key mode (the STM32 reports ASCII directly). Wire when those modes land.
