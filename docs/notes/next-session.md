# Start here — next session

**Last session:** 2026-07-18 (Session 10, two rounds). Pre-Phase-3
deferred-item batch shipped and **round-1 eval passed on-device** (screens
look good; **axis labels are keepers**). Round 2 then fixed the three eval
findings — `L` toggle persisted (GraphState, **PCG3 magic bump — one-time
state reset on first boot**), `rand())` history-render bug (layout parser
empty-arg-list), tick labels capped at 4 sig digits — and was **flashed;
its quick checklist is in HW-PENDING**. The batch:

- **D9 done**: Spleen 8x16 main font + 5x8 `gfx::small_font()` (BSD-2,
  `drivers/spleen/`, converter `scripts/bdf_to_utft.py`). Coyote `font1` no
  longer compiled — D17 permissive-path step 3; NOTICE.md updated.
- **rand() seeded**: xorshift64* in `math::fn`, boot-seeded from
  `get_rand_64()`; host tests deterministic (216 checks green).
- **ZoomFit**: `F` on the graph screen (function mode refits y;
  parametric/polar refit both axes).
- **Numeric axis tick labels** (small font): kept after eval; `L` toggles,
  persisted (default on), labels at 4 sig digits.

Phase 2 remains CLOSED (retro: `docs/notes/phase2-retro.md`). Keymap
reminders: **F6 no longer opens diag** (type `diag` on home); **trace is
F4**; help is the typed `help` command; new graph keys **`F` ZoomFit / `L`
labels** sit beside `S`/`T` presets.

## The next job: Phase 3 (statistics), sub-phase 3A

Start `docs/phases/phase3-spec.md` sub-phase 3A: the `Array` primitive +
list editor. Everything is decided and nothing blocks:

- **D21 (as amended)**: cap **10000**, **SRAM pool <= 256 elements /
  PSRAM tier above** (D10 is fixed — chunked bulk at ~6.8 MB/s,
  Pico-2-verified), elements double-only **+ dtype tag** (complex lists/
  matrices are committed future scope — keep element access tag-aware).
- Cold-boot caveat: PSRAM can lag a few seconds on cold power-on (D14,
  bench session pending, non-blocking) — list load waits for late-init;
  don't require PSRAM at boot.
- Mind the §8 strip-safety rule (idempotent `render()`) and task 3D.14
  (combined Pico 1 pass, D18).

Also fold in when convenient: the Session 10 round-2 quick checklist
(worklog HW-PENDING): `L` persists across reboot (expect the one-time
PCG3 state reset first boot), `rand()` history render, short ZTrig ticks.

## D14 rail settle — NEXT BENCH SESSION (keep here until done at a scope)

**Status (developer, 2026-07-18): non-blocking, deliberately kept on this
page until the scope session happens.** Nothing needs PSRAM at boot and
the few-second late-init wait feels fine in use — this is root-causing,
not firefighting. Software already gives timestamps (`late-init:` lines,
`psram-bulk:`/`battery:` heartbeats).
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

## Phase 3 notes (unchanged from Session 9)

`docs/phases/phase3-spec.md` — begin with sub-phase 3A (weeks 17–18): the
`Array` primitive (§2 — P3-1/P3-2 already decided, see D21) and the list
editor. Notes already embedded in the spec:

- §8 **strip-safety rule**: new `render()`s must be idempotent (they run
  ~20×/frame on the Pico 1 strip renderer; no host coverage catches
  violations).
- Task **3D.14** carries the deferred Pico 1 pass (D18): Phase 2 sweep +
  Session 8/9 fixes + Phase 3 acceptance in one board swap.
- §10: reconcile Phase 4's `Matrix` onto `Array` when Phase 4 begins.
- D10's bulk-PSRAM path is quarantined but Phase 3 lists will likely want
  it — budget a hardware session if it gets un-quarantined.

## Key things to note — Pico 2 specific

- **Firmware on the unit is the Session 10 round-2 build** (fonts, seeded
  rand, `F` ZoomFit, persisted `L` labels, rand() render fix — flashed
  2026-07-18). First boot does a **one-time PCG3 state reset**. The Pico 1
  is still on Session 7 firmware; its pass is deferred to post-Phase 3
  (D18).
- **D14 cold boot (~5-8 s rail settle):** PSRAM/SD may fail early init on a
  cold power-on; self-tests retry inside the 30 s late-init window and serial
  prints `late-init: ...` lines. If the F6 diag screen shows FAIL more than
  ~30 s after a cold boot, that's real.
- **Flash path (revised Session 9):** the `RP2350` BOOTSEL volume mounts
  again, and **cp to the volume is the preferred path** — `picotool load`
  proved slow (minutes at a black screen). Sequence: reboot to BOOTSEL
  (`stty -f /dev/cu.usbmodem* 1200`, or `picotool reboot -f -u` — note `-u`;
  plain `-f` reboots to *application* mode), **wait ~15 s for the volume to
  mount**, then `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/`
  (auto-reboots). Keep `picotool load` + `picotool reboot` as the fallback
  for when the volume doesn't mount at all.
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

- **Symbolic display (KIV, raised in the Session 10 eval)**: (a) axis ticks
  as pi / pi/2 etc. when scl is an irrational multiple (ZTrig); (b) surd-form
  displays (sqrt(2)/2-style); (c) answers as fractions and pi-fractions
  (pi/2, pi/3). Natural Phase 3/4 polish family — needs exact-value
  detection; the 4-sig-digit tick cap is the stopgap.
- F-key layout: **resolved and shipped** (D20). KIV only: F3 MODE vs ZOOM
  (TI's F3 slot) — judge after real use.
- D16 trace-sync option b (trace steps by table-step) — judge after more
  split use.
- Backlog: **D10 bulk PSRAM — RESOLVED 2026-07-18** (see D10 addendum;
  dual-core display service still deferred). Remaining: D14 rail settle
  (scope plan above — the last deferred HW item); 340-point curve cache
  cap; audio HAL; licensing (D17 — font step done, display/keyboard
  rewrites remain).

## Feature wishlist — wanted, not yet scheduled (raised 2026-07-18)

Neither appears in any phase spec yet; **plan them in** when scoping the
next phases (they need a home, not immediate work):

- **Complex numbers.** The whole stack is real-valued `double` today
  (tinyexpr, format, storage). Wants: an `a+bi` value type through the
  engine, complex-aware functions (sqrt(-1), ln of negatives, ...), a
  display format, and a MODE row entry (real / a+bi, TI-style).
  **Committed scope (2026-07-18): lists and matrices will hold complex
  values too** — D21's dtype tag in `Array`/`lists.dat` exists precisely
  so this lands as a non-breaking addition; Phase 3/4 code touching
  element access should not bake in double-only assumptions beyond the
  tag check.
- **TI-84 CALC-menu graph analysis** (2nd+TRACE on the TI): value, zero,
  minimum, maximum, **intersect**, dy/dx, **numeric fnInt** over an
  interval. Distinct from Phase 4's *symbolic* CAS: these are numeric +
  interactive on the graph screen (cursor-picked left/right bounds +
  guess; root-finding on the compiled function, intersect = root of
  Y1-Y2; quadrature for the integral, shaded region). Could land as a
  Phase 3.5 / early-Phase-4 slice since it only needs the existing
  compiled-eval machinery, no CAS.

## Hardware debugging kit (reminder)

- Serial: **plain `cat` reads nothing** — pico stdio_usb only transmits
  with DTR asserted (learned 2026-07-18). Interactive:
  `./scripts/monitor.sh` (screen). Non-interactive/agent:
  `./scripts/serial-capture.py [seconds] [match-substring]`. Lines:
  `late-init:`, `battery:` (change + 30 s heartbeat), `psram-bulk:`
  (30 s heartbeat), `graph recompute: N us` on window changes.
- Flash: see the Pico 2 notes above; Pico 1 BOOTSEL volume is `RPI-RP2`.
- `picocalc_diag` target = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
- Session protocol: read this file first when starting fresh; update it
  before ending a session.
