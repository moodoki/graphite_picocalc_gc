# Start here — next session

**Last session:** 2026-07-17/18 (Session 8). **The Phase 2 test drive (2.24)
passed on the Pico 2**, every bug found was root-caused and fixed the same
session, plus quick features (commit 079a8b2, flashed to the Pico 2 before
ending). Full record: `docs/notes/testdrive-phase2-observations.md`; narrative
in worklog Session 8. The developer is taking the unit for a **longer offline
spin** — expect fresh observations at the start of this session; log them
first.

## Key things to note — Pico 2 specific

- **Firmware on the unit is 079a8b2** (all Session 8 fixes). The Pico 1 is
  still on Session 7 firmware — reflash it before any Pico 1 testing.
- **PCG2 one-time state reset:** the graphstate magic was bumped (MODE-row
  settings now persist), so the *first* boot on this firmware discards the old
  Phase 2 state (curves, mode, T/TH ranges) and re-runs legacy yfuncs/window
  migration. Curves entered after that must persist normally — if state resets
  *again* on a later boot, that's a real bug.
- **D14 cold boot (~5-8 s rail settle):** PSRAM/SD may fail early init on a
  cold power-on. New behavior: self-tests keep retrying inside the 30 s
  late-init window (the old stuck-FAIL is fixed), and serial now prints
  `late-init: ...` lines — first boot-path instrumentation we have. If the F6
  diag screen shows FAIL more than ~30 s after a cold boot, that's real.
- **Flash path:** macOS has stopped reliably mounting the `RP2350` BOOTSEL
  volume. Reliable sequence: `stty -f /dev/cu.usbmodem* 1200` (or
  `picotool reboot -f` from the running app), then `picotool load
  build/pico2/picocalc_graphcalc.uf2` + `picotool reboot` — picotool sees the
  device even when the volume never mounts.
- **Battery/charging:** serial prints `battery: raw=0x.... pct=.. chg=..`
  every ~30 s. The charging decode was fixed to value-byte bit 7
  (`raw & 0x8000`) but is **unverified** — needs the battery below ~95% while
  plugged in. One captured raw line settles the byte-layout question for good.
- **Boot printfs still race USB enumeration** — only prints after ~2 s
  (late-init, battery, recompute) are capturable. Don't chase "missing" early
  boot output.
- **STM32 caution unchanged (both boards):** never poll STM32 registers
  back-to-back; a wedge needs a physical power cycle. Fw is v1.6.

## First: log the offline-spin observations

Append to `docs/notes/testdrive-phase2-observations.md` (or start a fresh
section) — the fix-verification list below doubles as the checklist.

## Verify the Session 8 fixes on-device (Pico 2)

1. Polar DEGREE, THstep=5.73 (or any step that doesn't divide 360): cardioid
   **closes** at 360°; trace at the last point reads th=360, not beyond.
2. WINDOW / table setup / ASK entry: `2*pi`, `pi/180` evaluate on commit;
   garbage input keeps the old value (no silent 2.0 from `2*pi`).
3. MODE: set DEGREE (+ FIX, display mode) → reboot → all survive.
4. Hop editor↔graph via F4 a dozen times, then F1 setup / F9 split — no dead
   push keys (stack-leak fix).
5. Split view, graph pane focused: **F1 starts trace** (bar now advertises
   it). This is the one report code inspection couldn't reproduce — if it
   still fails, capture exactly what's on screen.
6. Y= editor: enter `sin(` — row renders red; fix it — white again.
7. F6 diag → F5: FILES lists /picocalc (graphstate.dat present, legacy files
   still there but ignored).
8. Hold DOWN in a table: scrolling stops when the key is released.
9. Home: battery %/charging updates within ~1 s of change without a
   keypress; tall fraction history entries never draw over the status bar.
10. Home F1 in PARAM/POLAR opens the matching editor (softkey label follows).

## Then: the Pico 1 pass (deferred from Session 8)

Reflash `build/pico/picocalc_graphcalc.uf2` (BOOTSEL volume `RPI-RP2`; picotool
path works here too). Full Phase 2 sweep — headline is **split-screen pane
clipping on the strip renderer** (no bleed across the divider, D16 worry), plus
the fix list above. PCG2 reset note applies to this board's SD state too... the
SD is on the mainboard, so it was already reset by the Pico 2's first 079a8b2
boot.

## Then: close out Phase 2

- 2.24: close after the offline spin + Pico 1 pass are logged.
- 2.25: baseline is captured and healthy (recompute ≪ frame push; table
  scroll lag was event backlog, now coalesced — re-judge the feel). Decide if
  the remaining lever (table compile-once-per-regenerate) is still needed.
- 2.22: decide what "mode selector integration" needs beyond the MODE row.
- Then: Phase 2 retro → `docs/phases/phase3-spec.md`.

## Open design threads

- F-key layout rethink (feedback item 7): home F1 fix shipped; still open —
  graph on F3 (home) vs F4 (everywhere else), WINDOW unreachable from the
  graph screen. Any change must update help KEYS (2.27).
- D16 trace-sync option b (trace steps by table-step) — judge after more
  split use.
- Backlog unchanged: Pico 2 rail settle root cause (scope 3V3), PSRAM bulk
  transfer hang (D10), 340-point curve cache cap, audio HAL, licensing (D17).

## Hardware debugging kit (reminder)

- Serial: `cat /dev/cu.usbmodem*` — now includes `late-init:` and `battery:`
  lines; `graph recompute: N us` on window changes.
- Flash: see the Pico 2 notes above; Pico 1 BOOTSEL volume is `RPI-RP2`.
- `picocalc_diag` target = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
- Session protocol: read this file first when starting fresh; update it
  before ending a session.
