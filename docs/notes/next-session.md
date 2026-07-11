# Start here — next session

**Last session:** 2026-07-11 (five sessions, ending with HW verification round 1).
Phase 1 code-complete; all test-drive polish fixes **verified on Pico 1 hardware**;
graph recompute measured 15-17 ms (5.6 target met); both boards build; 106 host tests.
On-device firmware = the committed build (verified, then committed).

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
- **Keyboard facts (HW-verified 2026-07-11):** F1-F5 physical; F6-F9 = Shift+F1-F4
  (scan 0x86-0x89); **F10 does not exist** (Shift+F5 → plain F5). The STM32 swallows
  Shift on arrows; Alt/Ctrl pass through → view scroll = Alt/Ctrl+UP/DOWN (D12 revised).
  **Never poll the STM32 aggressively** — back-to-back register reads wedge it and only
  a physical power cycle recovers (USB reflash does not reset the STM32).
- **Battery:** this unit's STM32 keyboard fw lacks the battery register (0x0B times
  out; 0x01 answers) — indicator shows "--" by design, gives up after 5 failures.
  Revisit after a keyboard-firmware update.
- **Session protocol:** read this file first when starting fresh; update it before
  ending a session (developer convention, 2026-07-11).

## Next tasks (in priority order)

1. **Remaining HW-PENDING rows** (short now): SD card/persistence (FAT32 card needed —
   `sd=0` at boot), store op `2->A` (types `-` and `>`), trace + S/T presets, 5.3
   mode/reboot-to-bootloader, 5.7 full exit test.
2. **Dirty-rectangle / partial rendering (task 5.6 part 2).** Biggest remaining *code*
   item and worth doing before the next test drive: ~200 ms full-frame redraw per
   keypress dominates the feel of the device (recompute itself is only 15-17 ms).
3. **Pico 2 (RP2350) bring-up.** Never flashed. Uses `kUseFullFramebuffer = true` — a
   completely different, untested display path (200 KB SRAM framebuffer, not strips).
4. **Phase 1 wrap-up:** `docs/notes/phase1-retro.md` after HW verification closes.
5. **KIV during next test drive:** F-key layout rethink (feedback item 7). F1-F5
   physical matches TI's five top-row keys exactly; F6-F9 shifted for secondary
   functions (DIAG, Phase 2 HELP). Note F10 doesn't exist.

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
