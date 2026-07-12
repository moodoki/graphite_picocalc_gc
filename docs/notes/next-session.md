# Start here — next session

**Last session:** 2026-07-12 (Session 7). Highlights: **lint baseline is clean and
gating** (clang-tidy installed; config + lint.sh fixed — backlog item closed) and
**Phase 2 task 2.1 is done**: `src/graph/` now holds `Viewport` +
`Plotter`/`PointSource`, GraphScreen routes through them, behavior-preserving
(transforms locked by the new `tests/host/test_graph.cpp`). Tree = committed;
on-device firmware is one refactor behind (needs a reflash on the next test drive).

Read `docs/notes/worklog.md` (Session 7 + queue) for the full story. This file is
the short "what's next".

## Current state

- **Phase 2 in progress.** Task 2.1 done (graph/ extraction). Spec:
  `docs/phases/phase2-spec.md`; task table in §11.
- **Lint is now a real gate**: `./scripts/lint.sh` runs clang-format +
  clang-tidy with `WarningsAsErrors: '*'` and exits non-zero on any finding.
  Run it before committing. It self-locates Homebrew's keg-only clang-tidy and
  feeds arm-none-eabi-g++'s include paths to clang-tidy.
- **Careful with `clang-tidy --fix`**: this session it produced invalid
  `const char const*` from `const char* arr[]` declarations and const-ified a
  pointer that gets written through (`*p++`). Always rebuild + re-lint after.
- **Plotter/cache split (2.1 design call):** GraphScreen keeps the int16 column
  cache and replays it via `Plotter::begin()/point()`; `Plotter::plot()` feeds
  the same path. Parametric/polar sources (2.4/2.8) should implement
  `PointSource` and use `plot()`.
- **Dirty-band rendering (D13):** home screen + Y= editor track dirty row bands;
  a missed `invalidate()` = stale rows — watch for it when touching `on_key`.
- **STM32 fw is v1.6**; keyboard behavior identical to v1.2. **Never poll the
  STM32 aggressively** — back-to-back register reads wedge it (physical power
  cycle to recover).
- **Pico 2 debug notes:** BOOTSEL volume is `RP2350` (not `RPI-RP2`); 1200-baud
  reset works; boot printfs race USB enumeration — buffer and dump late.
- **Session protocol:** read this file first when starting fresh; update it
  before ending a session.

## Next tasks (in priority order)

1. **Phase 2 task 2.5:** parametric editor (6 X/Y pairs, spec §5.1). First UI
   task of Phase 2 — decide whether to generalize `YEditorScreen` or write a
   separate screen; the mode descriptor (`graph::descriptor_for`) exists for
   labels/slot counts. Slots live in `graph::state().param`.
2. **Phase 2 task 2.6:** mode-aware window screen (Tmin/Tmax/Tstep rows when
   mode == parametric; fields live in `graph::state()`).
3. **Phase 2 task 2.7:** parametric plotting + trace — wire `ParametricSource`
   (done, host-tested) into GraphScreen via `Plotter::plot()`; includes the
   trace generalization deferred from 2.1 (spec §14).
4. **Done this session:** 2.1–2.4. `GraphState` is nested-structs (not
   spec-flat); save/load deferred to 2.23 with the migration.
3. **KIV during next test drives:** function-mode visual check after the 2.1
   refactor (plot/trace/zoom should be pixel-identical); Pico 2 functional
   spot-check (eval/graph/dirty-band/persistence); charging color once battery
   <95%; F-key layout rethink (feedback item 7).

## Backlog (not blocking, but tracked)

- Root-cause the Pico 2 rail settle electrically (scope 3V3 on cold boot) — D14
  works around it; matters if Phase 3/4 needs PSRAM at boot.
- Bulk PSRAM transfer hangs on HW — root-cause before Phase 3/4 (needs it). (D10)
- Dual-core display stall never fully diagnosed (worked around with sync core-0
  render). (D10)
- Graph-screen latency: full-frame by design (D13) — if trace/zoom feel slow, the
  lever is SPI clock / DMA / plot-region caching, not bands.
- No audio HAL (pwm_sound vendored/linked but unused).
- Deferred: DMA push, ZoomFit + axis tick labels, 8x16 font (D9), unbounded
  history file (D4), unused overclock, `float` graph-eval lever (D5), `rand()`
  unseeded (NOLINTed), key auto-repeat suppressed, 2nd/Alpha indicators not
  driven.

## Hardware debugging kit (reminder)

- Reset to BOOTSEL without touching the board: `stty -f /dev/cu.usbmodem* 1200`, then
  `cp build/pico/picocalc_graphcalc.uf2 /Volumes/RPI-RP2/` (Pico 1) or
  `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/` (Pico 2).
- USB serial: `cat /dev/cu.usbmodem*`.
- `picocalc_diag` target (`src/diag_main.cpp`) = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
