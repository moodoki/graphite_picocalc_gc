# Start here — next session

**Last session:** 2026-07-19 (Session 11). **Phase 3 sub-phase 3A (Array +
lists + list editor) is code-complete, lint-clean, host-tested (suite now
351 checks) and flashed to the Pico 2** (boot verified over serial;
functional eval pending — the quick checklist is the Session 11 row in
worklog HW-PENDING). What landed:

- **`math::Array`** per D21 (dtype tag, cap 10000, SRAM <= 256 doubles /
  PSRAM tier above) — but note **D22**: PSRAM is *not memory-mapped*, so
  the API is `get`/`set` + `read_range`/`write_range`, no references/
  `data()`; `ArrayStore` recycles 2 KB SRAM slabs + fixed 80 KB PSRAM
  regions (free-list over the bump allocator).
- **l1..l6** (`math::lists()`), persisted to `lists.dat` (streamed
  chunks; new `Storage::read_file_range`). Load is all-or-nothing and
  waits for PSRAM on cold boot (late-init retries; serial prints
  `late-init: lists loaded` when late).
- **List ops**: in-place sorts (external merge sort for PSRAM-tier),
  cumsum, delta_list, seq, sum/prod/length, element-wise **vector lift**
  (`sin(l1)+2*l2` — engine compile-once with l1..l6 as bound variables).
- **Home syntax (D22)**: `{1,2,3}->l1`, `l1*2`, `sum(l1)` (bare-arg
  reductions embed in scalar exprs), `sort_asc(l1)` in place. Errors for
  length mismatch / scalar→list store. List results don't set Ans.
- **List editor** via typed **`lists`** command: grid (3 of 6 columns),
  type-to-edit, DEL row, F6/F7 sort, F8 clear (immediate — watch it),
  F1-F5 global scheme intact. Help FUNC/KEYS/SYNTAX updated.

## The next job

1. **On-device eval of the 3A batch** (Session 11 HW-PENDING row):
   editor feel, home list syntax, persistence across reboot + cold boot,
   the 1000-element PSRAM-tier path, help pages. Fold in anything still
   open from the Session 10 round-2 checklist (same table).
2. Then **sub-phase 3B** (`phase3-spec.md` §4, weeks 19-21): 1-var/2-var
   stats then the ten regressions. First consumers of `Array`; the
   normal-equations solve (3B.3) is the first small matrix-math user.
   Open call there: P3-3 (LM vs Gauss-Newton for logistic/sinusoidal —
   spec leans LM). Stats results UI (3B.9) needs a home; a `stats`
   typed command + screen would match the D20 pattern.
3. KIV for 3B+: D22 notes reductions are bare-list-arg only and literals
   can't join element-wise arithmetic — if 3B's UI wants richer
   expressions, consider promoting list_expr to a tagged-value evaluator.

Mind the §8 strip-safety rule (idempotent `render()` — the list editor
follows it: cached cells, draw-only render) and task 3D.14 (combined
Pico 1 pass, D18).

## D14 rail settle — NEXT BENCH SESSION (keep here until done at a scope)

**Status (developer, 2026-07-18): non-blocking, deliberately kept on this
page until the scope session happens.** Nothing needs PSRAM at boot and
the few-second late-init wait feels fine in use — this is root-causing,
not firefighting. Software already gives timestamps (`late-init:` lines,
`psram-bulk:`/`battery:` heartbeats; lists now also wait for late-init).
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

- **Firmware on the unit is the Session 11 build** (Phase 3A lists on
  top of everything from Session 10; flashed 2026-07-19, `psram-bulk:
  OK` + battery heartbeat seen on serial). The Pico 1 is still on
  Session 7 firmware; its pass is deferred to post-Phase 3 (D18).
- **`lists.dat` doesn't exist yet on the SD card** — first save creates
  it. If a load ever misbehaves, deleting the file resets all lists
  (magic PCL1; bump to PCL2 on layout change).
- **D14 cold boot (~5-8 s rail settle):** PSRAM/SD may fail early init on
  a cold power-on; self-tests retry inside the 30 s late-init window and
  serial prints `late-init: ...` lines (now including `lists loaded`).
  Large lists are simply absent until then; the editor shows "List
  memory unavailable" if a >256-element append beats PSRAM bring-up.
- **Flash path (revised Session 9):** the `RP2350` BOOTSEL volume mounts
  again, and **cp to the volume is the preferred path** — `picotool load`
  proved slow (minutes at a black screen). Sequence: reboot to BOOTSEL
  (`stty -f /dev/cu.usbmodem* 1200`, or `picotool reboot -f -u` — note `-u`;
  plain `-f` reboots to *application* mode), **wait ~15 s for the volume to
  mount**, then `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/`
  (auto-reboots). **macOS `cp` exits 1 with an xattr complaint — harmless,
  the UF2 landed (Session 11)**. Keep `picotool load` + `picotool reboot`
  as the fallback for when the volume doesn't mount at all.
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
Session 11 added ~24 KB static SRAM (ArrayStore slabs + list buffers) — RP2040
static was 62.5 KB, still far inside headroom, but re-check the map file.

## Open design threads

- **Symbolic display (KIV, Session 10 eval)**: pi-multiple axis ticks,
  surd displays, fraction/pi-fraction answers. Phase 3/4 polish family;
  the 4-sig-digit tick cap is the stopgap.
- **List UX watch-items (Session 11, judge on device)**: F8 clear-list is
  immediate (no confirm); list history results truncate at ~40 chars
  (`,...`); reductions bare-arg limitation (D22); `lists` command is the
  only editor entry — decide whether stats deserves an F-key/menu slot
  when 3B's stats screens land.
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
  place the complex representation lands.
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
