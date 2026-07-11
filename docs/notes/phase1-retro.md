# Phase 1 retrospective

**Window:** 2026-07-08 → 2026-07-12 (six working sessions).
**Outcome:** Phase 1 complete. A usable TI-style graphing calculator boots on real
PicoCalc hardware: expression evaluation with history and variables, pretty-printed
2D math, Y1–Y7 function graphing with trace/zoom/window/presets, mode settings,
SD persistence, battery indicator, diagnostics overlay — verified end-to-end on
Pico 1 (RP2040) and brought up on Pico 2 (RP2350).

## What shipped

- **Platform layer** over vendored Coyote OS drivers: display (ST7365P, RGB565
  buffers → RGB666 wire, D6), non-blocking STM32 I2C keyboard (D7), PIO PSRAM
  word access, SD/FatFs storage (D8), battery status with paced reads.
- **Rendering**: strip-mode framebuffer (Pico 1) / full framebuffer (Pico 2) behind
  one clipped-primitives API; event-driven redraw; **dirty-band partial rendering**
  (D13) — typing pushes ~28 of 320 rows, so keypress latency went from ~200 ms to
  ~20 ms without touching renderer code.
- **Math**: tinyexpr (right-assoc powers) + preprocessor (implicit-mult, `->` store
  D1, `e`/reserved-E D11), 25 letter variables + theta + ans, FLOAT/FIX/SCI +
  RAD/DEG modes, TI-compatible formatting (softfloat `%e` normalization included).
- **Layout engine**: pool-allocated node tree (text/hbox/fraction/superscript/paren)
  with D2's "simple operand" fraction heuristic (revised on hardware feedback so
  calls and powers stack: `1/sqrt(2)`, `x^2/2`).
- **Apps/UI**: home screen (history scroll, shell-style input recall D12), Y=
  editor, window editor, graph screen (column-cached plots, trace, S/T/zoom
  presets), mode screen with reboot-to-bootloader, status bar + softkeys chrome,
  screen-stack manager.
- **Verification kit**: 112 host-side checks (math + layout, no hardware needed),
  `picocalc_diag` vendored-only display target, `build-all`/`host-tests`/`format`
  scripts, no-touch reflash via 1200-baud reset.

Measured on hardware: graph recompute 15–17 ms for two functions (<50 ms target,
task 5.6); full-frame push ~200 ms (5 fps) — hence D13; typing now effectively
instant; boot to home screen well under a second (Pico 1).

## Timeline vs plan

The plan budgeted ten calendar weeks (~184 h). Wall-clock elapsed was five days,
in six agent-assisted sessions plus developer test drives and a live hardware
session. Two structural reasons beyond raw speed:

1. **Host-testable core.** The math engine and layout builder compile on the host,
   so 80% of the logic iterated at laptop speed with 112 regression checks.
   Hardware time was spent only on genuinely hardware-shaped problems.
2. **The HW-PENDING queue.** Sessions without the device attached didn't block on
   acceptance — items accumulated in a table and were cleared in batched live
   sessions with the developer driving the keyboard and the agent driving
   flash/serial. The final live session cleared ~15 items in one evening.

## What went well

- **Layered HAL discipline (D-prelude-2) paid for itself twice**: the dirty-band
  change touched only `Framebuffer`/`Screen`/two apps, and Pico 2 bring-up needed
  zero application changes — the board differences stayed inside `config.hpp` and
  the platform layer.
- **Decisions log**: D10's "quarantine the bulk PSRAM path" and the STM32 pacing
  rule were written down the day they were learned and prevented at least two
  would-be regressions later in the same week.
- **Vendored known-good drivers + a vendored-only diag target** made hardware
  bisection fast: every "screen is garbage" moment reduced to "our code or the
  panel?" in one flash.
- **Live HW sessions with serial capture**: the flash → observe → fix loop ran at
  ~10 minutes per round trip, including three full root-cause cycles for the
  RP2350 cold-boot issue in a single evening.

## What was hard (and the lessons)

- **Hardware breaks assumptions silently.** The three D10 bugs (bulk-PSRAM hang,
  dual-core display stall, keyboard I2C timeout) all passed builds and host tests;
  only printf boot-tracing on the device found them. Lesson: budget for a
  bring-up debugging kit *before* the first flash, not after.
- **The STM32 keyboard controller is fragile**: aggressive register polling wedges
  it until a physical power cycle, it swallows Shift on arrows, and shipped
  firmware (pre-v1.6) lacked the battery register entirely. Lesson: treat the
  south bridge as a slow, stateful peripheral — pace everything, verify per
  firmware version (v1.2 behaviors re-verified on v1.6).
- **Cold boot is not warm boot.** The RP2350 peripheral rail needs ~5–8 s to settle;
  PSRAM and SD passed every warm-reboot test and failed every cold power-on
  (D14). Timestamped, *buffered* boot traces (dumped after USB attach — boot
  prints race enumeration) were the tool that cracked it. Lesson: always test
  cold power-on explicitly; it is a different hardware state.
- **Real-keyboard UX differs from imagined UX.** The first test drive produced
  seven feedback items (colors, key behaviors, `e` shadowed by variable E, input
  recall). All were cheap to fix; none were predictable from the spec. Lesson:
  get the device into hands early — the D11/D12 fixes did more for feel than any
  planned task.

## Decisions to revisit later

- D5: `float` graph-eval lever — unused; recompute is fast enough.
- D9: 8x16 font upgrade — the 8x12 interim font is legible but dense.
- D10 leftovers: bulk PSRAM (needed by Phase 3/4) and the dual-core display path.
- D13 "revisit-when": graph-screen latency wants SPI clock / DMA / plot caching.
- D14: understand the rail settle electrically (scope the 3V3) before Phase 3/4
  leans on PSRAM at boot.

## Carried into Phase 2

Open queue: charging-color check (battery <95%), Pico 2 functional spot-check.
Backlog as listed in `next-session.md`. Phase 2 starts with task 2.1 (extract the
`graph/` subsystem); specs for Phases 2/3 are reconciled and in `docs/phases/`.
