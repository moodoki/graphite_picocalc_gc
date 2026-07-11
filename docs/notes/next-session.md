# Start here — next session

**Last session:** 2026-07-11 (Session 6). Developer updated the STM32 keyboard
firmware to **v1.6** — **battery indicator now works on-device**, no code change
needed. Then implemented dirty-band partial rendering (task 5.6 part 2, D13):
typing now pushes ~28 rows instead of 320, so per-keypress latency should drop
from ~200 ms to ~20 ms. **Not yet flashed/verified on hardware.** Both boards
build; 106 host tests green.

Read `docs/notes/worklog.md` (top entries + HW-PENDING queue) and `docs/notes/decisions.md`
(D13) for the full story. This file is just the short "what to do next".

## Current state

- **On Pico 1 hardware, verified:** boot → home screen, display, keyboard, PSRAM word
  r/w, backlight, all test-drive polish fixes (D11/D12), graph recompute 15-17 ms,
  **battery % display (after the v1.6 STM32 firmware update, 2026-07-11)**.
- **On-device firmware is now one commit behind the tree:** the dirty-band rendering
  change (D13) builds but has never been flashed.
- **Dirty-band rendering (D13):** screens opt in via `track_dirty()` + `invalidate(y0, y1)`;
  home screen and Y= editor track bands, everything else (graph, mode, window, diag)
  still full-frame. Screen switches always full-redraw. A missed invalidate shows up
  as stale rows — that's the main bug class to watch for on hardware.
- **STM32 firmware is v1.6** (`PicoCalc_BIOS_v1.6.bin` in repo root; flashed via
  stm32flash per the wiki). Unverified under v1.6: charging bit/cyan color, phantom
  keys after a 30 s battery refresh, whether Shift-on-arrows is still swallowed
  (if not, D12 can revert scroll to Shift and free Alt/Ctrl), whether F10 exists now.
- **Keyboard facts (from v1.2-era, re-check under v1.6):** F1-F5 physical; F6-F9 =
  Shift+F1-F4; F10 didn't exist. **Never poll the STM32 aggressively** — back-to-back
  register reads wedge it and only a physical power cycle recovers.
- **Phase 2/3 specs are in `docs/phases/`**; Phase 2 starts with task 2.1 (extract
  `graph/` subsystem).
- **Session protocol:** read this file first when starting fresh; update it before
  ending a session.

## Next tasks (in priority order)

1. **Flash and verify dirty-band rendering** (top of the HW-PENDING queue):
   typing on home screen feels instant; recall/ESC/history-scroll correct; Y= editor
   edit/select/toggle correct; no stale rows anywhere; screen switches still redraw
   fully. Also do the v1.6 firmware checks while there (charging color, phantom keys,
   Shift-on-arrows, F10 — see queue).
2. **Remaining HW-PENDING rows:** SD card/persistence (FAT32 card needed — `sd=0` at
   boot), store op `2->A`, pretty-math legibility, trace + S/T presets, 5.3
   mode/reboot-to-bootloader, 5.7 full exit test.
3. **Pico 2 (RP2350) bring-up.** Never flashed. Uses `kUseFullFramebuffer = true` — a
   completely different, untested display path (note: D13 made that buffer scratch,
   not a persistent frame image).
4. **Phase 1 wrap-up:** `docs/notes/phase1-retro.md` after HW verification closes,
   then Phase 2 task 2.1.
5. **KIV during next test drive:** F-key layout rethink (feedback item 7). F1-F5
   physical matches TI's five top-row keys; F6-F9 shifted (DIAG, Phase 2 HELP).

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
