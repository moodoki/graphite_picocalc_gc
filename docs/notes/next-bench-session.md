# Next bench session — D14 rail settle

Split out of `next-session.md` to keep it short. This is the last deferred
hardware item; pull it back into focus when a scope/bench session is actually
scheduled. Linked from `next-session.md`.

## D14 rail settle — NEXT BENCH SESSION

**Status (developer, 2026-07-18): non-blocking, deliberately kept as a live
page until the scope session happens.** Nothing needs PSRAM at boot and the
few-second late-init wait feels fine in use — this is root-causing, not
firefighting. Software already gives timestamps (`late-init:` lines,
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
