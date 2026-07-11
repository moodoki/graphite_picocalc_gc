# Start here — next session

**Last session:** 2026-07-11/12 (Session 6, long live HW session). Highlights:
STM32 fw → v1.6 (battery works + boot-grace fix); dirty-band rendering (D13)
implemented + verified (task 5.6 closed); pretty-print D2 revision (calls/powers
stack in fractions); **Pico 1 HW verification complete**; **Pico 2 brought up** —
full-framebuffer display path works, and the cold-boot PSRAM/SD failure was
root-caused (peripheral rail needs ~5-8 s to settle with the Pico 2 module) and
fixed with deferred late-init (D14, verified on a cold power-on).
On-device firmware (both boards' builds) = tree = committed.

Read `docs/notes/worklog.md` (Session 6 + queue) and `docs/notes/decisions.md`
(D13, D14, D2 revision) for the full story. This file is the short "what's next".

## Current state

- **Pico 1: everything verified.** **Pico 2: display, keyboard, battery, PSRAM,
  SD verified** (incl. cold boot via D14); a quick functional sweep (eval, graph,
  dirty-band feel, persistence) is still queued.
- **D14 late-init:** on Pico 2 cold boots the main loop retries PSRAM/SD every 2 s
  for the first 30 s; storage arriving late re-runs self-tests + loads history/
  variables/graph state. Warm reboots and Pico 1 never enter the retry path.
- **Dirty-band rendering (D13):** home screen + Y= editor track dirty row bands;
  a missed `invalidate()` = stale rows — watch for it when touching `on_key`.
- **STM32 fw is v1.6**; keyboard behavior identical to v1.2 (F1-F5 physical,
  F6-F9 = Shift+F1-F4, no F10, Shift swallowed on arrows). **Never poll the STM32
  aggressively** — back-to-back register reads wedge it (physical power cycle to
  recover).
- **Pico 2 debug notes:** BOOTSEL volume is `RP2350` (not `RPI-RP2`); the
  1200-baud reset works; boot printfs race USB enumeration — buffer and dump late.
- **Session protocol:** read this file first when starting fresh; update it before
  ending a session.

## Next tasks (in priority order)

1. **Phase 2 task 2.1:** extract the `graph/` subsystem per
   `docs/phases/phase2-spec.md`. **Phase 1 is complete** — retro written
   (`docs/notes/phase1-retro.md`, 2026-07-12).
2. **KIV during next test drives:** Pico 2 functional spot-check (eval/graph/
   dirty-band/persistence); charging color once battery <95%; F-key layout
   rethink (feedback item 7).

## Backlog (not blocking, but tracked)

- Root-cause the rail settle electrically (scope the 3V3 on Pico 2 cold boot) —
  D14 works around it; understanding it may matter if Phase 3/4 needs PSRAM at boot.
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
  `cp build/pico/picocalc_graphcalc.uf2 /Volumes/RPI-RP2/` (Pico 1) or
  `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/` (Pico 2).
- USB serial: `cat /dev/cu.usbmodem*`.
- `picocalc_diag` target (`src/diag_main.cpp`) = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
