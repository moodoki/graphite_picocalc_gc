# Start here — next session

**Last session:** 2026-07-10. First hardware bring-up on Pico 1 done — firmware boots to
the home screen with working display and keyboard. Phase 1 is code-complete (all 5
milestones); both boards build; 90 host tests pass. HW bring-up fixes in commit `a6f4bc3`.

Read `docs/notes/worklog.md` (top entry + HW-PENDING queue) and `docs/notes/decisions.md`
(D10) for the full story. This file is just the short "what to do next".

## Current state

- **On Pico 1 hardware, verified:** boot → home screen, display, keyboard, PSRAM word r/w,
  backlight.
- **The user is driving the calculator around** to shake out on-device behavior — expect
  observations/bugs to come back from that.
- Rendering is synchronous on core 0, event-driven (redraw only after a keypress).
  Full-screen redraw is ~200 ms (5 fps).

## Next tasks (in priority order)

1. **Triage whatever the user found** while driving the calculator around (may reprioritize
   the rest).
2. **SD card / persistence.** Boot showed `sd=0`. With a FAT32 card inserted, confirm
   `/picocalc` is created and history/vars/Y-funcs survive a power cycle. If it still fails
   *with* a card, that's a live bug in the SD SPI init/mount path.
3. **Full on-device exercise** (currently only display+keyboard confirmed): evaluate
   `2+3*sin(pi/4)`; check pretty-printed fractions/exponents/parens; Y= editor → graph
   `sin(x)`/`x^2-3` with trace, zoom, window edits; mode screen; reboot-to-bootloader;
   store op `2->A` (confirm the keyboard can type `-` and `>`).
4. **Graph profiling (task 5.6 part 1).** Firmware prints `graph recompute: N us` to USB
   serial — capture it against the <50 ms target.
5. **Dirty-rectangle / partial rendering (task 5.6 part 2).** The biggest quality item:
   only repaint changed regions instead of the whole 320x320 each keypress (~200 ms, snappier).
6. **Pico 2 (RP2350) bring-up.** Never flashed. Note it uses `kUseFullFramebuffer = true` —
   a completely different, untested display path (200 KB SRAM framebuffer, not strips).
7. **Phase 1 wrap-up:** `docs/notes/phase1-retro.md`, then `docs/phases/phase2-spec.md`.

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
