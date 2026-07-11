# Start here — next session

**Last session:** 2026-07-11 (three sessions: test-drive triage → phase 2/3 spec import +
consistency pass → phase 1 polish fixes). Phase 1 is code-complete *including* polish from
the first test drive; both boards build; 96 host tests pass.

Read `docs/notes/worklog.md` (top entries + HW-PENDING queue) and `docs/notes/decisions.md`
(D11, D12) for the full story. This file is just the short "what to do next".

## Current state

- **On Pico 1 hardware, verified:** boot → home screen, display, keyboard, PSRAM word r/w,
  backlight.
- **Test-drive feedback all addressed in code** (2026-07-11): dark grid + yellow Y7,
  ESC exits diagnostics, shell-style input recall (UP/DOWN) with Shift+UP/DOWN view scroll
  (D12), `e` = Euler's constant / variable E reserved (D11), HOME pops to home screen.
  None of it re-verified on hardware yet — see the "Polish:" rows in the HW-PENDING queue.
- **Phase 2/3 specs are in `docs/phases/`** (imported from developer drafts, reconciled
  against the code; built-in help planned as phase 2 §10; phase 4 renumbered to weeks
  26–35). Phase 2 starts with task 2.1 (extract `graph/` subsystem).
- Rendering is synchronous on core 0, event-driven; full-screen redraw ~200 ms (5 fps).
- **Keyboard correction (2026-07-11):** F1-F5 are physical; F6-F10 = Shift+F1-F5,
  translated by the STM32 into distinct scan codes (decode extended to 0x8A, F7-F10
  codes assumed — HW check queued). Because the STM32 remaps its shift layer, the
  Shift+UP/DOWN view-scroll (D12) may not arrive as arrow+shift — verify early.
- **Session protocol:** read this file first when starting fresh; update it before
  ending a session (developer convention, 2026-07-11).

## Next tasks (in priority order)

1. **HW verification queue** (table in worklog): SD card/persistence (`sd=0` at boot —
   FAT32 card needed), full on-device exercise, the five "Polish:" rows — especially
   whether Shift+arrow reaches us as arrow-plus-shift (D12's revisit trigger) and whether
   a 2nd F6 press exits diagnostics (original report said it didn't) — and graph
   profiling numbers (5.6, `graph recompute: N us` on USB serial, target <50 ms).
2. **Dirty-rectangle / partial rendering (task 5.6 part 2).** Biggest remaining *code*
   item and worth doing before the next test drive: ~200 ms full-frame redraw per
   keypress dominates the feel of the device.
3. **Pico 2 (RP2350) bring-up.** Never flashed. Uses `kUseFullFramebuffer = true` — a
   completely different, untested display path (200 KB SRAM framebuffer, not strips).
4. **Phase 1 wrap-up:** `docs/notes/phase1-retro.md` after HW verification closes.
5. **KIV during next test drive:** F-key layout rethink (feedback item 7). Corrected
   picture: F1-F5 are physical, F6-F10 = Shift+F1-F5 (STM32-translated scan codes) —
   the direct layer matches TI's five top-row keys exactly, so a TI-order remap with
   secondary functions on the shift layer is the leading candidate.

## Backlog (not blocking, but tracked)

- Bulk PSRAM transfer hangs on HW — root-cause before Phase 3/4 (needs it). (D10)
- Dual-core display stall never fully diagnosed (worked around with sync core-0 render). (D10)
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
