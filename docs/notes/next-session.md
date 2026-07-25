# Start here — next session

**Last session:** 2026-07-25, a bug-fixing session on the Pico 1 (connected
throughout; flashed via `picotool load -f` all session — the `cp` xattr
complaint recurred, so picotool was the reliable path). Cleared **both**
queued jobs from the prior handoff plus a perf measurement. Three commits:
**(1)** `5852c35` — **`!` factorial fixed** on the complex path (shared
engine's `!`→`fac()` rewrite via a new public `math::preprocess_factorial`;
`5!`/`4!` HW-verified). Closes the last open 3D.14 finding. **(2)**
`04128a3` — **D10 dual-core stall root-caused and fixed.** Bisected on HW:
the raw multicore FIFO handshake works flawlessly (echo round-trips in us);
DMA-to-SPI works on core 0 (warm + cold boot); but core 1 running
`push_rect_dma` *hard-wedges the chip* (USB drops). Developer's key tell —
the crashing firmware boots fine with USB unplugged — pinned it to **XIP
flash contention**: core 0's tinyusb/`stdio_usb` churns the shared XIP
cache while core 1 executes the display path from flash → hard fault. Fix:
mark the whole core-1 display path `__not_in_flash_func` (RAM-resident),
extending what the vendored driver already did for `spi_write_fast`/
`spi_finish`. **(3)** `a7fa78f` — **production dual-core display pipeline
wired** (`display_service_main` + two-buffer strip pipeline in
`render_frame`; core 0 renders strip N+1 while core 1 DMAs strip N; Pico 1
strip-mode only, Pico 2 full-framebuffer path left synchronous/untouched).
HW-validated (correct render, no tearing/hang). **Measured** (A/B vs the
old blocking path, full-frame redraw): **−16%** light render, **−22%**
medium, **−12%** status band; pipeline frame time is flat ~146-148 ms
(compute hides under the push, so the win grows with screen complexity),
push floor ~146 ms = SPI wire time. Also recorded a graph usage-feedback
wishlist item (cap grid-line count per axis by range when too dense to
see). Full detail: `worklog.md`'s 2026-07-25 entry, `decisions.md` D10
addendum. **Phase 4D remains the next major work — unchanged from the
2026-07-24 plan below.**

## The next job

1. **DONE 2026-07-25 — `!` factorial bug fixed** (`5852c35`).
   `math::complexexpr::evaluate` now shares engine's `!`→`fac()` rewrite via
   the new public `math::preprocess_factorial` (`engine.hpp`/`.cpp`), applied
   to the trimmed body before parsing. Host tests + `5!`/`4!` on the Pico 1.
1b. **DONE 2026-07-25 — D10 dual-core display stall RESOLVED end to end**
   (`04128a3` root cause + RAM-residency fix, `a7fa78f` production pipeline).
   The "FIFO stall" was never the FIFO (echo handshake is flawless) and never
   DMA (fine on core 0) — it was **XIP flash contention**: core 1 executing
   the display path from flash hard-faults while core 0's USB stack churns
   the shared XIP cache. Fixed by making the core-1 display path
   `__not_in_flash_func`. Pipeline shipped (core 0 renders strip N+1 while
   core 1 DMAs strip N); measured −16..22% on full-frame redraws. Full trace:
   `worklog.md` 2026-07-25, `decisions.md` D10 addendum. **Two follow-ups
   left open** (not blocking, no phase home):
   - **Extend the pipeline to Pico 2** — its full-framebuffer push is still
     synchronous on core 0 (left untouched because the RP2350 board wasn't in
     hand to test). When it is: route the full-frame push through core 1 too
     and re-verify the RAM-residency fix on the RP2350.
   - **Secondary compute-parallelization candidate** (unchanged from the
     2026-07-24 scoping, still open): the pipeline gives ~0 benefit on
     compute-bound screens (render > ~146 ms push budget — a heavy graph
     redraw measured 1.17 s in the render callback). Those want
     `GraphScreen::recompute_function` (`src/apps/graph_screen.cpp:313`)
     parallelized — it sweeps up to 10 independent `Y=` slots but shares one
     `math::engine()` instance (mutates a shared `X`), so it needs a second
     engine/vars context, not just a spawned task. Matrix ops and iterative
     regression fits stay ruled out (inherently sequential).
2. **Phase 4D (GC completeness)** is the next major work, per
   `phase4-spec.md` §7 (weeks 32-35) — the closing pass that rounds Phase 4
   out into the project's pre-release milestone (sequence graphing, fuller
   zoom/shading, list↔matrix bridge, scientific constants, unit conversions,
   home-screen matrix literals, complex-valued variable/Ans storage, device
   polish). Its own task list may want to pick up two things surfaced
   2026-07-22: a home-screen `MatAns` token (currently editor-only, a UX
   gap relative to scalar `ans`) and fnInt shading following curve color
   (feature request, alpha-blend or hatching if true alpha isn't feasible).
   **Phase 5 (CAS engine)** follows 4D per `phase5-spec.md` — symbolic
   simplify/expand/solve, using the Session 18 `Complex` type as the numeric
   backing for complex roots (§4.1 hook; quadratic/polynomial solves with
   negative discriminant emit `i`-valued symbolic roots). **Phase 6
   (non-calculator functions)** follows Phase 5 — an app-launcher framework
   (6A) with MicroPython as its first app (6B), replacing the old 4E plan.
   This ordering (4 → 5 → 6) and the phase split itself were decided
   2026-07-21 (D32, D33) — see `phase4-spec.md`, `phase5-spec.md`,
   `phase6-spec.md`, and `decisions.md` D32/D33. MicroPython's phase slot
   (an open question as of D32) is now resolved: Phase 6 sub-phase 6B.
3. **Low-priority, not blocking**: Pico 2 perf feel for Phase 3/4 features
   (lists, StatPlots, CALC-menu rendering, matrix ops) has never been
   re-measured against current code — only the pre-Phase-3 2.25 baseline
   exists. The two Pico 1 sluggish spots found 2026-07-22 (list editor,
   5000-point scatter) are now **fixed (D35)** — stat-plot point cache,
   list-editor dirty bands, one-file-per-list/matrix persistence — and the
   fixes are board-generic (same code path, no `#ifdef`s), so the Pico 2
   should already benefit. That's still **unverified**, though — it wasn't
   reflashed this session. Worth a quick spot-check next time the Pico 2 is
   in hand: confirm it still feels fine (it already did pre-fix, per
   Phase 4A-4C) and ideally confirm the fixes measurably help there too,
   not just on the Pico 1.
4. **CLOSED 2026-07-24 (D37): all matrix/complex "first-class" departures
   are now decided**, no post-4D scoping pass left to do — see
   [design-departures-matrix-complex.md](design-departures-matrix-complex.md)
   (status: closed) and `decisions.md` D32/D33 (A/B), D36 (E, G), D37
   (C, D, F). Every idea A-G now either has a 4D task ID or, for F, a
   committed (not open-ended) follow-on timing:
   - A (home-screen matrix literals) → 4D.14
   - B (complex variable/`Ans` storage) → 4D.15; its open question P4-11
     (error vs. silent truncation on real-only reads) is **resolved:
     error**, generalized to cover complex list/matrix elements too
   - E (vector ops) → 4D.22; the list↔matrix bridge half already shipped
     as 4D.12
   - G (matrix eigenvectors, real-only v1) → 4D.23
   - C (complex-valued lists, PSRAM-only tier so bss doesn't grow; v1 =
     storage/elementwise/`sum`/`mean`, error on `stdev`/regression/`sort`)
     → 4D.24
   - D (complex-valued matrices, full complex linear algebra in v1,
     excluding eigendecomposition of a complex matrix) → 4D.25
   - F (unify `matexpr`/`complexexpr`/`listexpr`) → committed as the
     real follow-on once 4D ships, not a phase/week slot yet
   4D's subtotal is now ~160 hrs (Phase 4 total ~295 hrs) — see
   `phase4-spec.md` §8 for the updated task table.
   **New idea H, raised same session, explicitly NOT decided**:
   polymorphic variables (any `A`-`Z` holds real/complex/list/matrix,
   MATLAB-style) — bigger than F (collapses the three *namespaces*, not
   just the three evaluators). Scoped in
   [design-departures-matrix-complex.md](design-departures-matrix-complex.md)
   §H with a rough 100+ hr ballpark, no phase slot. **Revisit after 4D
   ships**, same checkpoint as F.

Mind the §8 strip-safety rule (idempotent `render()`) for any new
screens touched during the on-device passes.

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Firmware on the Pico 2 is still Session 19's font/glyph build**
  (Terminus default, `-DPICOCALC_FONT=terminus`; flashed 2026-07-21,
  boots healthy, telemetry clean over serial) — it was **not reflashed
  this session**, so it does not yet have the D35 perf fixes (stat-plot
  cache, list-editor dirty bands, per-list/per-matrix persistence) that
  are now on the Pico 1; it's a build behind. This build layers on top
  of Sessions 11/12/15/16/17/18 (3A lists, 3B stats, 3D inference/plots,
  4A matrices/solver, 4B CALC menu, 4C complex numbers). **All of their
  hands-on on-device evals are now closed as a formality (2026-07-22)** —
  board-independent logic, and the harder rendering case (Pico 1) passed
  the identical checklists the same day (3D.14 for 11/12/15, the Phase
  4A-4C pass for 16/17/18); see `worklog.md`'s 2026-07-22 entries.
  What's genuinely still open, Pico-2-specific (not closeable by this
  reasoning): its own perf re-baseline for Phase 3/4 features, now also
  covering whether the D35 fixes help there too once it's reflashed (see
  "The next job" #3) — **deliberately deferred**, not part of this
  session's scope. Session 15's storage-health row (hot-plug/retry-forever,
  Y=-editor truncation) is now fully closed — confirmed on both boards.
  **Session 10 round 2 is now also closed (2026-07-22)**: `L` toggle
  surviving a reboot, `rand()` showing a sensible varying value, ZTrig
  short tick labels (`1.571`-style), and `F` ZoomFit auto-fit all
  confirmed on the Pico 1. The HW-PENDING table is now clear except the
  deferred Pico 2 perf re-baseline and the still-informal Session 19 font
  sweep.
- **The Pico 1 now carries the 2026-07-25 bug-fixing work** on top of the
  D35 state: the `!` factorial fix (`5852c35`), the D10 RAM-residency fix
  (`04128a3`), and the dual-core display pipeline (`a7fa78f`) — each built,
  flashed, and developer-confirmed in turn, and the board is back on the
  clean production pipeline build. Current state: boots healthy, telemetry
  clean over serial; bss **212184 bytes** (was 201896 pre-pipeline; +10 KB
  is the second strip buffer for the double-buffered pipeline), still
  comfortably inside the ~76 KB headroom watched since D28. Phase 2 +
  Phase 3 are HW-verified on this board (3D.14 pass, closes D18/Phase 3),
  and Phase 4A-4C are HW-verified on this board (full pass, see the
  Phase 4A-4C worklog entry). Font/glyph correctness was informally
  spot-checked during the Phase 4A-4C pass and reported looking correct
  (not a dedicated sweep). **No open bug findings remain on this board** —
  the `!` factorial bug is now fixed (was the last one). The
  list-editor/scatter-plot perf feel is fixed (D35), and the whole render
  path now runs through the dual-core pipeline (D10 resolved).
- **No GraphState/persistence layout change 2026-07-25** — still **PCG5**
  (last bumped Session 18/D30); list/matrix formats unchanged (PCL2/PCM2,
  D35). The 2026-07-25 work was engine/render-path only, no on-disk change,
  so no new one-time reset.
- **List/matrix persistence changed shape this session (D35)**: the old
  single `lists.dat` / `matrices.dat` (magics PCL1 / PCM1) are replaced by
  one file per store — `/picocalc/list1.dat`..`list6.dat` (magic PCL2) and
  `/picocalc/matrix1.dat`..`matrix10.dat` (magic PCM2). Old images simply
  aren't read under the new paths (same "old files ignored" precedent as
  prior format bumps) — expect a one-time reset to empty lists/matrices on
  first boot under this firmware, already confirmed as expected. If a load
  ever misbehaves, deleting the relevant `listN.dat`/`matrixN.dat` resets
  just that one store.
- **Non-default font builds** (`build/pico2-jm|io|uni|term`) are stale
  relative to this session's non-font changes (eig alias, list scroll)
  — rebuild before re-comparing fonts. `build/pico2` (Terminus) is the
  canonical default and what's currently flashed.
- **D14 cold boot (~5-8 s rail settle):** PSRAM/SD may fail early init on
  a cold power-on; self-tests retry inside the 30 s late-init window and
  serial prints `late-init: ...` lines (including `lists loaded`).
  Large lists are simply absent until then; the editor shows "List
  memory unavailable" if a >256-element append beats PSRAM bring-up.
  Stats on a not-yet-loaded list just sees fewer/empty elements.
- **Flash path (revised Session 9, reconfirmed Session 12):** `stty -f
  /dev/cu.usbmodem* 1200` reboots to BOOTSEL, the **RP2350 volume
  mounted in ~5 s this time**, then
  `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/`
  (auto-reboots; cp exited 0 this session — the Session 11 xattr
  complaint didn't recur). Keep `picotool load` + `picotool reboot` as
  the fallback for when the volume doesn't mount at all.
- **Battery/charging: fully verified 2026-07-18.** Refresh cadence is 5 s
  by design — stability over snappiness; don't "optimize" it back down.
- **Boot printfs still race USB enumeration** — only prints after ~2 s
  (late-init, battery, recompute) are capturable. Don't chase "missing"
  early boot output.
- **STM32 caution unchanged (both boards):** never poll STM32 registers
  back-to-back; a wedge needs a physical power cycle. Fw is v1.6.

## Pico 1 pass: DONE (3D.14 + Phase 4A-4C, D18 resolved 2026-07-22)

The combined pass decided 2026-07-18 (D18) ran 2026-07-22 as task 3D.14: the
Pico 1 was reflashed to current HEAD (Session 19) and put through the full
Phase 2 sweep — headline **split-pane clipping on the strip renderer**, no
bleed — plus the Session 8+9 fix list and the Phase 3 acceptance checklist.
All passed; two non-blocking findings (factorial `!`, list-editor/scatter-plot
perf) were logged at the time. The perf finding was fixed later the same day
(D35 — see "Open design threads" and the fourth 2026-07-22 worklog entry); the
factorial bug remains open in "Backlog" below. Full detail: worklog 2026-07-22 entry,
`phase3-retro.md`, `session3D14-pico1-observations-verbatim.md`. **This
closes Phase 3.** Guardrail carried forward: Phase 3+ render code must stay
strip-safe (idempotent, may run ~20x/frame) — rule recorded in
`phase3-spec.md` §8; it held up cleanly this pass. Pico 1 bss was ~188.8 KB of
264 KB as of Session 19 (D28/D29/D30/D31 combined, essentially flat) — no
headroom pinch observed during 3D.14.

**Same session, second block**: Phase 4A-4C (matrices/solver, CALC menu,
complex numbers) also got their first-ever hands-on pass, on this same
Pico 1/build — all passed (details in "Last session" above and the worklog
Phase 4A-4C entry). This also **closed the Session 16/17/18 Pico 2
HW-PENDING rows as a formality** (board-independent logic, harder rendering
case already passed) — Pico 2 has no genuinely open board-specific gap left
except its own perf re-baseline (low priority, "The next job" #3). Map-file
re-check (the Pico 1 bss watch item, knob `ArrayStore::kSlabCount`) can now
be considered done for this generation of features — no headroom pinch
observed across either pass.

## Open design threads

- **List UX watch-items (Session 11, judge on device)**: F8 clear-list is
  immediate (no confirm); list history results truncate at ~40 chars
  (`,...`); `lists`/`stats` are typed-command-only entries (now with
  `list`/`stat` aliases, D24) — decide whether stats deserves an
  F-key/menu slot now that the screen exists. (Resolved by D24:
  reductions bare-arg limitation; mean/median/stdev promotion.)
- **Stats watch-items (Session 12, judge on device)**: results are
  plain text lines (no two-column layout for 2-Var's 17 lines).
  (Resolved by D24: "Computing..." indicator — verify its visibility
  on a 10000-element 1-Var.)
- **Session 13 caps to watch**: 4 lift operands per expression, 64
  elements per brace literal — revisit if real use pinches (D24).
- F3 MODE vs ZOOM (TI's F3 slot) — judge after real use (D20 KIV).
- D16 trace-sync option b (trace steps by table-step) — after more split
  use.
- **4B CALC watch-items (Session 17, judge on device)**: min/max "Guess?"
  step is UI-only, doesn't feed Brent's bracket — decide if that's fine or
  needs wiring through (D29). (Resolved by D29: P4-6 intersect = cursor-
  cycle; P4-8 polar fnInt = area only, no arc length — both from
  `phase4-spec.md` §11, tracked here and in `decisions.md` rather than
  editing the spec's open-questions table.)
- **4C watch-items (Session 18, judge on device)**: whether "Non-real
  result" is clear phrasing for REAL-mode domain errors; no
  complex-valued variable storage (`2i->a` errors) — decide if that's
  ever actually wanted (D30). (Resolved by D31: the ASCII `<` polar
  stand-in is now a real ∠ glyph, Terminus default.)
- **Font/glyph watch-items (Session 19, judge on device)**: informally
  spot-checked during the 2026-07-22 Phase 4A-4C pass and reported looking
  correct — not a dedicated sweep, so still worth a proper pass if time
  allows, but no longer a blind spot. Open sub-items: whether `√` read as
  inline-only (`√(x)`, no vinculum) is acceptable; whether the shared
  Unifont-derived `i`/⇒ glyphs look consistent against Terminus's own glyph
  shapes; big-radical display and true subscripts (`Sₓ`, `σₓ`) remain
  KIV/wishlist items (D31).
- **Pico 1 watch-items (task 3D.14 + Phase 4A-4C, 2026-07-22)**: `!`
  (factorial) syntax error in non-REAL Number mode — **FIXED 2026-07-25**
  (`5852c35`, `complexexpr` now shares engine's postfix-`!` rewrite;
  `5!`/`4!` HW-verified). The list editor and 5000-point scatter plot sluggishness
  are **fixed as of the same day (D35)**: bucketed stat-plot point cache,
  list-editor dirty-band narrowing, and (the real bottleneck behind "large
  lists feel sluggish to enter") one-file-per-list/matrix SD persistence —
  all flashed and developer-confirmed on the Pico 1, see `worklog.md`'s
  fourth 2026-07-22 entry and `decisions.md` D35. Two more from the
  Phase 4A-4C pass, both still open: no home-screen `MatAns` token
  (editor-only, UX gap vs. scalar `ans`); fnInt shading should follow curve
  color (feature request). All logged in
  `session3D14-pico1-observations-verbatim.md`,
  `phase4abc-pico1-observations-verbatim.md`, and `phase3-retro.md`.
- Backlog: D14 rail settle ([next-bench-session.md](next-bench-session.md) —
  the last deferred HW item); 340-point curve cache cap; audio HAL; licensing (D17 —
  display/keyboard rewrites remain); dual-core display service (D10
  addendum — **DONE 2026-07-25: root-caused + fixed + pipeline shipped,
  see "The next job" #1b; two non-blocking follow-ups there — Pico 2
  full-frame pipeline, and compute-parallelizing `recompute_function`**);
  **stale diag-screen label — DONE 2026-07-25**: `main.cpp` header comment
  de-staled; diag title line now shows `Phase 4C [<hash>-dev]` right-aligned
  (build id via CMake `PICOCALC_BUILD_ID` = `git rev-parse --short HEAD` +
  dirty check); leftover per-strip `Frame:` counter removed. Also added a
  **die-temperature read** (`platform::die_temp_c()`, on-chip ADC ch 4) on
  the diag screen + a `temp:` 30 s serial heartbeat (idle ~28-31 C).

## Feature wishlist

Desired-but-unplanned features live in **[wishlist.md](wishlist.md)**. Complex
numbers and TI-84 CALC-menu graph analysis graduated into Phase 4 (sub-phases
4C and 4B — both code-complete). The 2026-07-21 stocktaking session (D32/D33)
graduated most of the rest: eight items into Phase 4D, one into Phase 6 §9,
and the old "symbolic display" item split in two — pi-ticks/`▶Frac` into 4D,
surd/exact-value display into Phase 5 §10.1. What's left unscheduled:
antialiased font rendering (revisit once the Phase 6 desktop-emulator
candidate exists) and the SD list-data-file/CBL-CBR half of the old
"beyond 6 lists" item. See [wishlist.md](wishlist.md) for current detail.

## Hardware debugging kit (reminder)

- Serial: **plain `cat` reads nothing** — pico stdio_usb only transmits
  with DTR asserted. Interactive: `./scripts/monitor.sh` (screen).
  Non-interactive/agent: `./scripts/serial-capture.py [seconds]
  [match-substring]`. Lines: `late-init:` (incl. `lists loaded`),
  `battery:` (change + 30 s heartbeat), `psram-bulk:` (30 s heartbeat),
  `graph recompute: N us`.
- Flash: see the Pico 2 notes above; Pico 1 BOOTSEL volume is `RPI-RP2`.
- `picocalc_diag` target = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
- Session protocol: read this file first when starting fresh; update it
  before ending a session.
