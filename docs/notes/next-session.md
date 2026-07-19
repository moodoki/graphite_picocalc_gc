# Start here — next session

**Last session:** 2026-07-20 (Session 15). **Phase 3 is code-complete**:
part 1 implemented the Session 14 observation batch (**D26** — storage
health: retry-forever heartbeat, SD hot-plug via DET poll, red
`SD`/`PSRAM` status-bar indicators; Y=-editor `...` truncation); part 2
shipped **all of sub-phase 3D except the Pico 1 pass (3D.1-3D.13,
D27** — resolves P3-5 + P3-6). Suite **716 checks**, lint clean, both
boards build, **flashed to the Pico 2**. What landed in part 2:

- **`math::stats` inference** (`src/math/infer.{hpp,cpp}`): z/t
  (pooled + Welch, Data or summary), paired t, 1/2-prop z, chi-square
  GOF + 2-way (columns = l1..lk), one-way ANOVA (groups = l1..lk),
  linreg slope t-test, and the six interval families. `Alt` (!=, <, >)
  on the mean/prop/slope tests; p-values via new one-sided `dist`
  survival functions.
- **`test` typed command** (alias `infer`): 15-kind form (10 tests + 5
  intervals), Data/Stats source toggle, results as cached lines.
- **StatPlots**: Plot1-3 (`plot` command; persisted — **PCG4, one-time
  graph-state reset on first boot**): scatter, xy-line, histogram,
  modified box plot, normal-probability plot. Cache/draw split for
  strip safety; graph draws plots under curves; **`Z` = ZoomStat**.

## The next job

1. **On-device eval** of the outstanding batches (worklog HW-PENDING):
   Session 11 (3A lists), Session 12 (3B stats), Session 15 storage
   health (hot-plug needs the physical card) **and** Session 15 3D
   (inference + stat plots — expect the PCG4 one-time reset on first
   boot).
2. **3D.14 — the combined Pico 1 pass (D18)** closes Phase 3: swap the
   board, reflash `build/pico/…uf2` (BOOTSEL volume `RPI-RP2`), run the
   Phase 2 sweep (headline: split-pane clipping on the strip renderer),
   the Session 8+9 fix list, Phase 3 acceptance, and watch every §8
   screen for strip-render artifacts (stats/dist/test results, stat
   plots — scatter/normprob re-stream per strip; judge the feel).
   Pico 1 budget as of Session 15: text ~305 KB, bss ~147 KB of
   264 KB — re-check the map file.
3. Then **Phase 4** (`phase4-spec.md`): 4A matrices, 4B graph analysis
   (CALC menu), 4C complex numbers.

Mind the §8 strip-safety rule (idempotent `render()` — Stats/Dist/
Infer screens cache result lines; stat plots split recompute/draw).

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Firmware on the unit is the Session 15 build** (storage health +
  full 3D on top of everything prior; flashed 2026-07-20). **First boot
  does a one-time graph-state reset (PCG4)** — window/mode/axis-labels/
  plots return to defaults once. The Pico 1 is still on Session 7
  firmware; its pass is 3D.14 (D18).
- **`lists.dat` may not exist yet on the SD card** — first save creates
  it. If a load ever misbehaves, deleting the file resets all lists
  (magic PCL1; bump to PCL2 on layout change).
- **D14 cold boot (~5-8 s rail settle):** PSRAM/SD may fail early init on
  a cold power-on; self-tests retry inside the 30 s late-init window and
  serial prints `late-init: ...` lines (including `lists loaded`).
  Large lists are simply absent until then; the editor shows "List
  memory unavailable" if a >256-element append beats PSRAM bring-up.
  Stats on a not-yet-loaded list just sees fewer/empty elements.
- **Flash path (revised Session 9, reconfirmed Session 12):** `stty -f
  /dev/cu.usbmodem* 1200` reboots to BOOTSEL, the **RP2350 volume
  mounted in ~5 s this time**, then
  `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/`
  (auto-reboots; cp exited 0 this session — the Session 11 xattr
  complaint didn't recur). Keep `picotool load` + `picotool reboot` as
  the fallback for when the volume doesn't mount at all.
- **Battery/charging: fully verified 2026-07-18.** Refresh cadence is 5 s
  by design — stability over snappiness; don't "optimize" it back down.
- **Boot printfs still race USB enumeration** — only prints after ~2 s
  (late-init, battery, recompute) are capturable. Don't chase "missing"
  early boot output.
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
run ~20x/frame) — rule recorded in `phase3-spec.md` §8. Note for that pass:
Sessions 11+12 added static SRAM (ArrayStore slabs, list buffers, stats chunk
buffers + sinusoid scan accumulators ~10 KB) — Pico 1 bss is ~126 KB of 264 KB,
still comfortable, but re-check the map file then.

## Open design threads

- **List UX watch-items (Session 11, judge on device)**: F8 clear-list is
  immediate (no confirm); list history results truncate at ~40 chars
  (`,...`); `lists`/`stats` are typed-command-only entries (now with
  `list`/`stat` aliases, D24) — decide whether stats deserves an
  F-key/menu slot now that the screen exists. (Resolved by D24:
  reductions bare-arg limitation; mean/median/stdev promotion.)
- **Stats watch-items (Session 12, judge on device)**: results are
  plain text lines (no two-column layout for 2-Var's 17 lines).
  (Resolved by D24: "Computing..." indicator — verify its visibility
  on a 10000-element 1-Var.)
- **Session 13 caps to watch**: 4 lift operands per expression, 64
  elements per brace literal — revisit if real use pinches (D24).
- F3 MODE vs ZOOM (TI's F3 slot) — judge after real use (D20 KIV).
- D16 trace-sync option b (trace steps by table-step) — after more split
  use.
- Backlog: D14 rail settle ([next-bench-session.md](next-bench-session.md) —
  the last deferred HW item); 340-point curve cache cap; audio HAL; licensing (D17 —
  display/keyboard rewrites remain); dual-core display service (D10
  addendum).

## Feature wishlist

Desired-but-unplanned features live in **[wishlist.md](wishlist.md)**. Complex
numbers and TI-84 CALC-menu graph analysis have since graduated into Phase 4
(sub-phases 4C and 4B — [phase4-spec.md](../phases/phase4-spec.md)); still-open
item there is symbolic display (KIV).

## Hardware debugging kit (reminder)

- Serial: **plain `cat` reads nothing** — pico stdio_usb only transmits
  with DTR asserted. Interactive: `./scripts/monitor.sh` (screen).
  Non-interactive/agent: `./scripts/serial-capture.py [seconds]
  [match-substring]`. Lines: `late-init:` (incl. `lists loaded`),
  `battery:` (change + 30 s heartbeat), `psram-bulk:` (30 s heartbeat),
  `graph recompute: N us`.
- Flash: see the Pico 2 notes above; Pico 1 BOOTSEL volume is `RPI-RP2`.
- `picocalc_diag` target = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
- Session protocol: read this file first when starting fresh; update it
  before ending a session.
