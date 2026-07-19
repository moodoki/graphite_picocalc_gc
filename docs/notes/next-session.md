# Start here — next session

**Last session:** 2026-07-19 (Session 12). **Phase 3 sub-phase 3B
(descriptive stats + all ten regressions) is code-complete, lint-clean,
host-tested (suite now 473 checks) and flashed to the Pico 2** (boot
verified over serial; functional eval pending — Session 11 **and** 12
rows in worklog HW-PENDING). What landed (see **D23**):

- **`math::stats`**: 1-var (plain + freq-weighted) / 2-var stats.
  Quartiles/medians via **streaming rank selection** (binary search on
  the double bit space, <= 64 shared passes, weighted + x-filterable) —
  no sort, no temp region, works identically on the PSRAM tier.
- **All ten regressions**: polynomial 1-4 (normal equations on
  center+scaled x), ln/exp/pwr (linearized, TI-style r/r²), logistic +
  sinusoidal (**LM**, P3-3 resolved; logit / frequency-scan seeds;
  `converged` flag), median-median (filtered selection; x-boundary ties
  group by value). `r` NaN where TI doesn't define it.
- **`stats` typed command** → form (Analysis / lists / Freq / Store /
  Calculate) + scrollable results. **Store to y1..y7** writes the
  numeric model (engine-parseable, SinReg degree-converted in DEGREE
  mode) and enables the slot. Help KEYS/SYNTAX updated.

## The next job

1. **On-device eval of the 3A + 3B batches** (Session 11 and Session 12
   rows in worklog HW-PENDING): list editor + home list syntax first,
   then the stats screen sweep (form feel, results, store→graph
   overlay, error paths, the 10000-element 1-Var timing feel — decide
   whether a "computing..." indicator is warranted, D23 revisit).
2. Then **sub-phase 3C** (`phase3-spec.md` §5, weeks 22-23):
   distributions. Start with 3C.1 — vendor the needed cephes sources
   (`ndtr`, `incbet`, `igam`/`igamc`; public domain) into
   `drivers/cephes/`, update `docs/dependencies.md` + NOTICE. Open
   call **P3-4** (naming: TI two-arg `normal_cdf(lo, hi, ...)` vs
   one-tailed standard) — spec leans TI two-arg for test usefulness;
   decide at 3C.2. Registration goes through `math::catalog` (3C.7,
   full-arity only — tinyexpr has no default args); local bisection
   for inverse CDFs (no Phase 1 solver exists).
3. KIV for 3C UI: distributions are scalar functions, so they mostly
   ride the normal engine path + catalog/help; the guided-entry helper
   (3C.8) could follow the `stats` form pattern.

Mind the §8 strip-safety rule (idempotent `render()` — StatsScreen
follows it: compute in on_key, cached result lines) and task 3D.14
(combined Pico 1 pass, D18).

## D14 rail settle — NEXT BENCH SESSION (keep here until done at a scope)

**Status (developer, 2026-07-18): non-blocking, deliberately kept on this
page until the scope session happens.** Nothing needs PSRAM at boot and
the few-second late-init wait feels fine in use — this is root-causing,
not firefighting. Software already gives timestamps (`late-init:` lines,
`psram-bulk:`/`battery:` heartbeats; lists wait for late-init).
Schematic findings (2026-07-18, `clockwork_Mainboard_V2.0_Schematic.pdf`
in the clockworkpi/PicoCalc repo; copy fetched during Session 10):

- **Two 3.3 V nets — don't probe the wrong one.** `MCU_3V3` comes from
  the mainboard's AXP2101 PMU (STM32 etc.); **`3V3_OUT` is the Pico
  module's own regulator output** and powers the PSRAM (U301,
  ESP-PSRAM64H) and SD — it's the D14 suspect (only rail that changes
  with the Pico 1→2 swap). The side-port 3V3 pin's net is unverified:
  discriminate by pulling the Pico module and checking if the pin dies
  (dead = 3V3_OUT, usable; alive = MCU_3V3, wrong rail).
- Unambiguous probe points (back cover off): **PSRAM VCC = U301 pin 8**,
  or **3V3_OUT on the Pico 20-pin socket header**. TP1-TP6 exist on the
  mainboard — identify with a meter.
- **Trigger: GP21 = RAM_SCK is exposed on the side port** (side sockets
  carry GP2/GP3/GP4/GP5/GP21/GP28). Channel 2 on RAM_SCK shows every
  PSRAM init attempt as a clock burst — first attempt, each ~2 s
  late-init retry, and the first success — case closed, no firmware
  change. GP28 is free if a firmware boot-marker is ever wanted.
- Verify left/right pin positions of J702/J703 by continuity against the
  schematic before probing (community docs don't give positions).

Bench session plan:

1. Cold power-on (unit off long enough to discharge; battery path).
   Ch1 = 3V3_OUT rail (rising-edge capture), Ch2 = RAM_SCK (GP21),
   ~10 s window.
2. Measure: ramp shape/time to 3.3 V, dips during boot (SD inrush), and
   rail state at the moment of each RAM_SCK burst — distinguishes "rail
   late/dirty" from "rail fine, PSRAM internal init late".
3. Correlate against serial `late-init:` timestamps from the same boot
   (`scripts/serial-capture.py 40` on a second terminal).
4. If the rail is clean, suspicion moves to PSRAM power-up state (needs
   RESETEN/RESET after VDD stable — `Psram::reinit()` already does this;
   an early-boot retry-with-reset loop might then beat the 5-8 s wait).
5. Compare Pico 1 (no symptom) vs Pico 2 (symptom) — regulator/mainboard
   interaction is the working hypothesis (D14).

## Key things to note — Pico 2 specific

- **Firmware on the unit is the Session 12 build** (Phase 3B stats on
  top of everything from Session 11; flashed 2026-07-19, `psram-bulk:
  OK` + battery heartbeat seen on serial). The Pico 1 is still on
  Session 7 firmware; its pass is deferred to post-Phase 3 (D18).
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

- **Symbolic display (KIV, Session 10 eval)**: pi-multiple axis ticks,
  surd displays, fraction/pi-fraction answers. Phase 3/4 polish family;
  the 4-sig-digit tick cap is the stopgap.
- **List UX watch-items (Session 11, judge on device)**: F8 clear-list is
  immediate (no confirm); list history results truncate at ~40 chars
  (`,...`); reductions bare-arg limitation (D22); `lists`/`stats` are
  typed-command-only entries — decide whether stats deserves an
  F-key/menu slot now that the screen exists.
- **Stats watch-items (Session 12, judge on device)**: synchronous
  Calculate with no "computing..." indicator (matters only for
  PSRAM-tier lists / slow LM fits); results are plain text lines (no
  two-column layout for 2-Var's 17 lines); `mean/median/stdev` are NOT
  home-screen reductions (only sum/prod/length are, D22) — the stats
  screen is the path; consider promoting them if that grates (D22
  revisit: tagged-value evaluator).
- F3 MODE vs ZOOM (TI's F3 slot) — judge after real use (D20 KIV).
- D16 trace-sync option b (trace steps by table-step) — after more split
  use.
- Backlog: D14 rail settle (scope plan above — the last deferred HW
  item); 340-point curve cache cap; audio HAL; licensing (D17 —
  display/keyboard rewrites remain); dual-core display service (D10
  addendum).

## Feature wishlist — wanted, not yet scheduled (raised 2026-07-18)

Neither appears in any phase spec yet; **plan them in** when scoping the
next phases (they need a home, not immediate work):

- **Complex numbers.** The whole stack is real-valued `double` today.
  Wants: an `a+bi` value type through the engine, complex-aware
  functions, display format, MODE row entry. **Committed scope: lists
  and matrices hold complex values too** — that's why `Array`/`lists.dat`
  carry the dtype tag (D21/D22); 3A shipped with all element access
  routed through `get`/`set`, so the accessor internals are the only
  place the complex representation lands. (3B note: `math::stats`
  streams through `read_range` — real-valued stats stay correct
  whatever the storage dtype becomes.)
- **TI-84 CALC-menu graph analysis** (value, zero, min/max, intersect,
  dy/dx, numeric fnInt): numeric + interactive on the graph screen;
  could be a Phase 3.5 / early-Phase-4 slice on the existing
  compiled-eval machinery.

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
