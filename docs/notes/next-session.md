# Start here — next session

**Last session:** 2026-07-11 (Session 6, live HW session). **Pico 1 hardware
verification is complete** — three rounds in one sitting. STM32 fw updated to v1.6
(battery works, incl. a cold-boot grace fix); dirty-band partial rendering (D13)
implemented and verified (typing instant, task 5.6 closed); pretty-print fraction
fix for function calls/powers (D2 revised); SD persistence, store op, trace/presets,
mode, reboot-to-bootloader all verified. On-device firmware = tree = committed build.

Read `docs/notes/worklog.md` (Session 6 + queue) and `docs/notes/decisions.md`
(D13, D2 revision) for the full story. This file is just the short "what to do next".

## Current state

- **Pico 1: everything verified.** The only open HW items are the two queue rows:
  charging color (needs battery <~95%; bit-7 assumption still unproven) and
  **Pico 2 bring-up** (never flashed).
- **Dirty-band rendering (D13):** screens opt in via `track_dirty()` + `invalidate(y0, y1)`;
  home screen and Y= editor track bands; graph/mode/window/diag are full-frame by
  design. A missed invalidate = stale rows; watch for it when touching `on_key` paths.
- **STM32 fw is v1.6**; keyboard behavior identical to v1.2 (F1-F5 physical, F6-F9 =
  Shift+F1-F4, no F10, Shift swallowed on arrows). **Never poll the STM32
  aggressively** — back-to-back register reads wedge it; only a physical power
  cycle recovers.
- **Phase 2/3 specs are in `docs/phases/`**; Phase 2 starts with task 2.1 (extract
  `graph/` subsystem).
- **Session protocol:** read this file first when starting fresh; update it before
  ending a session.

## Next tasks (in priority order)

1. **Pico 2 (RP2350) bring-up.** Never flashed. Uses `kUseFullFramebuffer = true` —
   a completely different, untested display path (D13 made that buffer scratch, not
   a persistent frame image). `build/pico2/picocalc_graphcalc.uf2` is current.
   Swap boards, BOOTSEL-flash, then spot-check the Pico 1 checklist (boot, keys,
   graph, dirty-band artifacts, SD).
2. **Phase 1 wrap-up:** write `docs/notes/phase1-retro.md`, flip the phase to done.
3. **Phase 2 task 2.1:** extract the `graph/` subsystem per `docs/phases/phase2-spec.md`.
4. **KIV during next test drive:** F-key layout rethink (feedback item 7); charging
   color once battery <95%.

## Backlog (not blocking, but tracked)

- Bulk PSRAM transfer hangs on HW — root-cause before Phase 3/4 (needs it). (D10)
- Dual-core display stall never fully diagnosed (worked around with sync core-0 render). (D10)
- Graph-screen latency: full-frame by design (D13) — if trace/zoom feel slow, the lever
  is SPI clock / DMA / plot-region caching, not bands.
- clang-tidy not installed → `lint.sh` can't run clean.
- No audio HAL (pwm_sound vendored/linked but unused).
- Deferred: DMA push, ZoomFit + axis tick labels, 8x16 font (D9), unbounded history file
  (D4), unused overclock, `float` graph-eval lever (D5), `rand()` unseeded, key auto-repeat
  suppressed, 2nd/Alpha indicators not driven.

## Hardware debugging kit (reminder)

- Reset to BOOTSEL without touching the board: `stty -f /dev/cu.usbmodem* 1200`, then
  `cp build/pico/picocalc_graphcalc.uf2 /Volumes/RPI-RP2/`.
- USB serial: `cat /dev/cu.usbmodem*`.
- `picocalc_diag` target (`src/diag_main.cpp`) = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
