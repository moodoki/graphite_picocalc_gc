# Start here — next session

**Last session:** 2026-07-18 (Session 9). **All ten Session 8 fixes are now
verified on the Pico 2.** The offline spin passed 8/10; the two failures
(held-key scroll overrun, battery staleness) were re-root-caused, re-fixed,
flashed, and re-verified on-device the same session. The charging-bit decode
is confirmed as a bonus. The Pico 1 Phase 2 pass is **deferred to post-Phase 3
(D18)**. Full record: worklog Session 9;
`docs/notes/testdrive-phase2-observations.md` (offline-spin section).

## The next job: close out Phase 2

- 2.24: **close it** — the test drive, offline spin, and fix verification are
  all logged (Pico 1 no longer gates it, D18).
- 2.25: baseline is captured and healthy (recompute ≪ frame push; table
  scroll lag was event backlog, now truly coalesced after the Session 9 drain
  fix — re-judge the feel). Decide if the remaining lever (table
  compile-once-per-regenerate) is still needed.
- 2.22: decide what "mode selector integration" needs beyond the MODE row.
- Then: Phase 2 retro → pick up `docs/phases/phase3-spec.md` (note the
  strip-safety rule added to its §8 and the 3D.14 note re: D18).

## Key things to note — Pico 2 specific

- **Firmware on the unit is the Session 9 build** (079a8b2 + the item-8/9
  re-fixes: key drain via `Keyboard::fifo_empty()`, cache-only
  `battery_status()` + `battery_poll()` at 5 s). The Pico 1 is still on
  Session 7 firmware; its pass is deferred to post-Phase 3 (D18).
- **D14 cold boot (~5-8 s rail settle):** PSRAM/SD may fail early init on a
  cold power-on; self-tests retry inside the 30 s late-init window and serial
  prints `late-init: ...` lines. If the F6 diag screen shows FAIL more than
  ~30 s after a cold boot, that's real.
- **Flash path (revised Session 9):** the `RP2350` BOOTSEL volume mounts
  again, and **cp to the volume is the preferred path** — `picotool load`
  proved slow (minutes at a black screen). Sequence: reboot to BOOTSEL
  (`stty -f /dev/cu.usbmodem* 1200`, or `picotool reboot -f -u` — note `-u`;
  plain `-f` reboots to *application* mode), then
  `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/` (auto-reboots).
  Keep `picotool load` + `picotool reboot` as the fallback for when the
  volume doesn't mount.
- **Battery/charging: fully verified 2026-07-18.** Charging decode
  (`raw & 0x8000`, value-byte bit 7) confirmed on-device (charger plug at
  84% → status bar follows in ~5-6 s). Serial `battery:` line now prints on
  value change + a 30 s heartbeat. Refresh cadence is 5 s by design —
  stability over snappiness; don't "optimize" it back down.
- **Boot printfs still race USB enumeration** — only prints after ~2 s
  (late-init, battery, recompute) are capturable. Don't chase "missing" early
  boot output.
- **STM32 caution unchanged (both boards):** never poll STM32 registers
  back-to-back; a wedge needs a physical power cycle. Fw is v1.6.

## Pico 1 pass: DEFERRED to post-Phase 3 (D18)

Decided 2026-07-18: the board swap is tedious, the board-conditional surface is
tiny (clip logic is shared and Pico-2-exercised; RP2040 RAM headroom is ~195 KB),
and the residual risks (strip-render idempotency, perf feel) are localized, not
architectural. One combined pass after Phase 3 (task 3D.14) covers the Phase 2
sweep — headline: **split-pane clipping on the strip renderer** — plus the
Session 8+9 fix list and Phase 3 acceptance. Until then the Pico 1 stays on
Session 7 firmware; reflash before that pass (`build/pico/…uf2`, BOOTSEL volume
`RPI-RP2`). Guardrail: Phase 3 render code must be strip-safe (idempotent, may
run ~20×/frame) — rule recorded in `phase3-spec.md` §8.

## Open design threads

- F-key layout rethink (feedback item 7): home F1 fix shipped; still open —
  graph on F3 (home) vs F4 (everywhere else), WINDOW unreachable from the
  graph screen. Any change must update help KEYS (2.27).
- D16 trace-sync option b (trace steps by table-step) — judge after more
  split use.
- Backlog unchanged: Pico 2 rail settle root cause (scope 3V3), PSRAM bulk
  transfer hang (D10), 340-point curve cache cap, audio HAL, licensing (D17).

## Hardware debugging kit (reminder)

- Serial: `cat /dev/cu.usbmodem*` — `late-init:` and `battery:` lines (the
  latter on change + 30 s heartbeat); `graph recompute: N us` on window
  changes.
- Flash: see the Pico 2 notes above; Pico 1 BOOTSEL volume is `RPI-RP2`.
- `picocalc_diag` target = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
- Session protocol: read this file first when starting fresh; update it
  before ending a session.
