# Start here — next session

**Last session:** 2026-07-11 (Session 6, live HW session with the developer).
STM32 keyboard fw updated to **v1.6** → battery indicator works (plus a boot-grace
fix for the cold-boot "--", HW-verified). **Dirty-band partial rendering (D13)
implemented and HW-verified** — typing is instant, no artifacts; task 5.6 closed.
v1.6 still swallows Shift-on-arrows and still has no F10, so D12 stands.
On-device firmware = tree = committed build. Both boards build; 106 host tests.

Read `docs/notes/worklog.md` (Session 6 + HW-PENDING queue) and `docs/notes/decisions.md`
(D13) for the full story. This file is just the short "what to do next".

## Current state

- **On Pico 1 hardware, verified:** boot → home screen, display, keyboard, PSRAM word
  r/w, backlight, all polish fixes (D11/D12), graph recompute 15-17 ms, dirty-band
  rendering (D13: typing = ~28-row push instead of 320), battery % incl. cold-boot
  grace. No phantom keys across battery refreshes.
- **Dirty-band rendering (D13):** screens opt in via `track_dirty()` + `invalidate(y0, y1)`;
  home screen and Y= editor track bands; graph/mode/window/diag are full-frame by
  design. A missed invalidate = stale rows; watch for it when touching `on_key` paths.
- **STM32 fw is v1.6** (`PicoCalc_BIOS_v1.6.bin` in repo root). Keyboard behavior
  identical to v1.2: F1-F5 physical, F6-F9 = Shift+F1-F4, no F10, Shift swallowed on
  arrows (kShift arrives, no arrow event — confirmed via diag serial). **Never poll
  the STM32 aggressively** — back-to-back register reads wedge it; only a physical
  power cycle recovers.
- **Charging color unverified:** battery was at 100% (charger idle when full, so the
  charging bit can't be judged). Retest when <~95%: icon should go cyan when plugged.
- **Phase 2/3 specs are in `docs/phases/`**; Phase 2 starts with task 2.1 (extract
  `graph/` subsystem).
- **Session protocol:** read this file first when starting fresh; update it before
  ending a session.

## Next tasks (in priority order)

1. **Remaining HW-PENDING rows** (see worklog queue): SD card/persistence (FAT32 card
   needed — `sd=0` at boot), store op `2->A` (types `-` and `>`), pretty-math
   legibility, trace + S/T presets, 5.3 mode/reboot-to-bootloader, 5.7 full exit test.
2. **Pico 2 (RP2350) bring-up.** Never flashed. Uses `kUseFullFramebuffer = true` — a
   completely different, untested display path (note: D13 made that buffer scratch,
   not a persistent frame image).
3. **Phase 1 wrap-up:** `docs/notes/phase1-retro.md` once the HW queue closes, then
   Phase 2 task 2.1.
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
