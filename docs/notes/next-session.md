# Start here — next session

**Last session:** 2026-07-12 (Session 7, long). Highlights: **lint baseline clean
and gating** (backlog item closed); **Phase 2 tasks 2.1–2.7 done** — `src/graph/`
subsystem (Viewport/Plotter/PointSource/Mode/GraphState/sources/TraceCursor),
editors per D15, mode-aware window screen, parametric plotting + trace. The whole
parametric path is code-complete but **unreachable until a mode selector exists**.
Tree = committed; on-device firmware is a full session behind (reflash needed).

Read `docs/notes/worklog.md` (Session 7 + queue) for the full story. This file is
the short "what's next".

## Current state

- **Phase 2 in progress.** Tasks 2.1–2.11 done (weeks 11–13 of spec §11) plus
  help (2.26–2.28) and a minimal 2.22. Remaining: tables (2.12–2.18),
  split-screen + integration (2.19–2.25). Spec: `docs/phases/phase2-spec.md`.
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

1. **HW test drive (parametric + polar + help + persistence on-device):**
   - Function-mode parity first (2.1/2.5 refactors should be invisible).
   - **Persistence/migration (2.23)**: first boot must carry over existing
     Y-funcs + window from the old files (graphstate.dat appears on SD);
     afterwards parametric/polar curves, graph mode, and T/TH ranges must
     survive a cold power cycle.
   - Parametric acceptance: MODE → Graph mode → PARAM; X1T=cos(t),
     Y1T=sin(t) → circle; Lissajous (cos(3t), sin(2t)); trace t/x/y.
   - Polar acceptance (spec week 13): POLAR mode; r1=1+cos(theta) →
     cardioid; r2=2*sin(3*theta) → rose; **both angle modes** (in DEGREE
     set THmax=360, THstep~5.7); trace th/x/y readout.
   - Help: Home F5 → FUNC lists 17 functions, KEYS/SYNTAX scroll.
2. **Next coding block:** week 14–15 tables (2.12–2.18) — table setup
   screen, mode-aware evaluate_table_row, auto/ask table screen.
3. **Done this session:** 2.1–2.11 + minimal 2.22 + help (2.26–2.28) +
   persistence (2.23). All of spec weeks 11–13 code-complete plus the two
   biggest week-16 items. Parameter-mode curve cache = 340 points/curve
   (tiny steps truncate — documented). KEYS help content must be revised
   if the F-key rethink lands.
4. **KIV during next test drives:** Pico 2 functional spot-check
   (eval/graph/dirty-band/persistence); charging color once battery <95%;
   F-key layout rethink (feedback item 7).

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
