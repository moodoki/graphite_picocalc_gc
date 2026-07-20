# Start here — next session

**Last session:** 2026-07-20 (Session 17). **Sub-phase 4B is
code-complete (D29)** — graph analysis / CALC menu, on top of the
Session 16 Phase 4A close. Suite **1070 checks**, lint clean, both
boards build clean. **No hardware was connected this session — nothing
was flashed.** The Pico 2 is still on the **Session 16 (4A) build**;
Pico 1 is still on Session 7 firmware awaiting the deferred 3D.14 pass.
What landed:

- **Numeric calculus primitives** added to
  `src/math/numeric_solve.{hpp,cpp}` (the 4A solver file): Brent's-
  method extremum, central-difference+Richardson derivative, adaptive
  Gauss-Kronrod (G7-K15) integral — each a callback core (`EvalFn`)
  plus an expr-string wrapper (parametric/polar integrands aren't a
  single expression string).
- **Graph analysis engine** (`src/graph/analysis.{hpp,cpp}`): mode-
  aware value/zero/extremum/intersect/derivative/integral across
  function/parametric/polar (parametric slope = (dy/dt)/(dx/dt); polar
  slope via the Cartesian forms; polar fnInt = area only, radians
  internally regardless of angle mode).
- **Interactive session state machine**
  (`src/graph/analysis_cursor.{hpp,cpp}`): `AnalysisSession`, modeled
  on `TraceCursor`, drives the TI-84 step flow (Left/Right Bound,
  Guess, First/Second curve for intersect).
- **UI**: new F6 **"CALC"** softkey on the graph screen + typed
  `calc`/`analyze`; new `src/apps/calc_menu.{hpp,cpp}` menu screen;
  `graph_screen.{hpp,cpp}` draws the result marker, tangent line
  (dy/dx), and shaded fnInt region (function mode, new — no Phase-3
  shaded-region primitive existed despite the spec assuming one).
- **D29 resolves two open spec questions**: P4-6 (intersect curve
  picking = cursor-cycle, TI-84 behavior) and P4-8 (polar fnInt = area
  only, no arc length). Min/max keep the "Guess?" UI step but Brent's
  method only uses the bracket — a judgment call to revisit on
  hardware. Full details in `decisions.md` (D29).
- Pico 1 size **unchanged from the 4A baseline** (text 354036, bss
  188616 of 264 KB) — the Gauss-Kronrod tables are compile-time
  constexpr arrays in flash/text, not bss.

## The next job

1. **On-device evals** (worklog HW-PENDING; the flashed build is still
   Session 16/4A — flash the Session 17/4B build first): Session 11
   (3A lists), Session 12 (3B stats), Session 15 storage health + 3D
   (inference + stat plots — expect the PCG4 one-time reset on first
   boot), Session 16 4A (matrix editor, bracket typing, solver), and
   **Session 17 4B**: F6 CALC menu on the physical keyboard, cursor
   feel riding curves in all three graph modes, shaded fnInt region +
   tangent-line rendering, result-store-to-variable behavior, and
   specifically judge whether the min/max "Guess?" step should
   actually influence Brent's bracket or whether cursor-only bounds
   are fine (D29).
2. **3D.14 — the combined Pico 1 pass (D18)** closes Phase 3: swap the
   board, reflash `build/pico/…uf2` (BOOTSEL volume `RPI-RP2`), run the
   Phase 2 sweep (headline: split-pane clipping on the strip renderer),
   the Session 8+9 fix list, Phase 3 acceptance, and watch every §8
   screen for strip-render artifacts. **Re-check the map file** — Pico 1
   bss is ~188 KB of 264 KB (D28, unchanged by D29); if the ~76 KB
   stack/heap headroom pinches, shrink `ArrayStore::kSlabCount`.
3. After the 4B on-device eval: **Phase 4C (complex numbers)** is next
   per `phase4-spec.md` §5 (weeks 30-31) — `Complex` type + arithmetic
   + a+bi/polar mode, prerequisite for 4D CAS's complex-aware solve.

Mind the §8 strip-safety rule (idempotent `render()` — Stats/Dist/
Infer/Solver screens cache result lines; matrix editor caches cells;
stat plots split recompute/draw; the new CALC fnInt shading reads the
graph's cached plot-y column array rather than recomputing).

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Firmware on the unit is the Session 16 build** (4A matrices +
  solver on top of everything prior; flashed 2026-07-20, warm-boot
  verified over serial). The PCG4 graph-state reset already happened
  on the Session 15 build — no reset this time. The Pico 1 is still
  on Session 7 firmware; its pass is 3D.14 (D18).
- **`lists.dat` / `matrices.dat` may not exist yet on the SD card** —
  first save creates them. If a load ever misbehaves, deleting the
  file resets that store (magics PCL1 / PCM1; bump on layout change).
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

## Pico 1 pass: DEFERRED to post-Phase 3 (D18)

Decided 2026-07-18: the board swap is tedious, the board-conditional surface is
tiny (clip logic is shared and Pico-2-exercised; RP2040 RAM headroom is ~195 KB),
and the residual risks (strip-render idempotency, perf feel) are localized, not
architectural. One combined pass after Phase 3 (task 3D.14) covers the Phase 2
sweep — headline: **split-pane clipping on the strip renderer** — plus the
Session 8+9 fix list and Phase 3 acceptance. Until then the Pico 1 stays on
Session 7 firmware; reflash before that pass (`build/pico/…uf2`, BOOTSEL volume
`RPI-RP2`). Guardrail: Phase 3 render code must be strip-safe (idempotent, may
run ~20x/frame) — rule recorded in `phase3-spec.md` §8. Note for that pass:
Sessions 11+12 added static SRAM (ArrayStore slabs, list buffers, stats chunk
buffers + sinusoid scan accumulators ~10 KB) — Pico 1 bss is ~126 KB of 264 KB,
still comfortable, but re-check the map file then.

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
- Backlog: D14 rail settle ([next-bench-session.md](next-bench-session.md) —
  the last deferred HW item); 340-point curve cache cap; audio HAL; licensing (D17 —
  display/keyboard rewrites remain); dual-core display service (D10
  addendum).

## Feature wishlist

Desired-but-unplanned features live in **[wishlist.md](wishlist.md)**. Complex
numbers and TI-84 CALC-menu graph analysis have since graduated into Phase 4
(sub-phases 4C and 4B — [phase4-spec.md](../phases/phase4-spec.md)); still-open
item there is symbolic display (KIV).

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
