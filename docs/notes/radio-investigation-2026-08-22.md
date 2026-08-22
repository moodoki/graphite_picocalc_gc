# Radio (WiFi/Bluetooth) investigation — 2026-08-22

**Status: investigation only. Nothing planned, nothing decided.** No design
doc, no task, no decision entry — this is background research prompted by a
soak-session question, kept here so it isn't lost.

## The hardware

The Pico 2 board in hand is the **wireless variant (Pico 2 W)**: an
Infineon **CYW43439** combo Wi-Fi/Bluetooth chip on a **shared SPI bus**
with the RP2350, running up to 33 MHz.

- **Wi-Fi**: 802.11n, single-band 2.4 GHz only (no 5 GHz), WPA3, can run as
  station or soft AP (up to 4 clients).
- **Bluetooth**: 5.2, both BLE (Central + Peripheral) and Classic.
- **Antenna**: on-board, licensed from ABRACON/ProAnt — no external
  connector.
- **Hazard worth remembering**: the chip's IRQ line is multiplexed with its
  own SPI DIN/OUT data pin — the datasheet says you can only poll for an
  IRQ when no SPI transaction is in flight. Same *class* of problem as the
  panel SPI race fixed in issue #39/D85 (nothing outside a known-safe
  window may touch shared bus state) — just a different peripheral.
- Confirms an existing detail already in this repo: `phase6.3-spec.md` and
  D89's UF2-family gate already name-and-refuse the **`CYW43` family ID**
  as an unsupported foreign family for the app-slot loader. Same chip
  family, not a coincidence — the board really is a Pico 2 W.
- Pico 1 has no equivalent chip at all, so any radio feature is
  **Pico-2-only** by hardware, the same board-conditional shape as the
  strip-buffer/framebuffer scratch-accessor split already in `platform/`.

Sources: [Pico series docs](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html), [Adafruit Pico 2 W product page](https://www.adafruit.com/product/6087), [PiShop Radio Module 2 (CYW43439) datasheet listing](https://www.pishop.us/product/raspberry-pi-radio-module-2/)

## Resource cost, against this repo's actual numbers

| Component | Flash | RAM |
|---|---|---|
| CYW43 WiFi/BT firmware blob (`43439A0.bin` + CLM) | ~225 KB | ~0 (stays in flash by default) |
| BTstack (Classic + BLE both on) | ~80 KB | ~20 KB |
| lwIP (TCP/IP, needed for WiFi networking) | tens of KB | ~20-40+ KB, config-dependent — no fixed official number found |
| cyw43 driver control state | small | a few KB |

Sources: [cyw43-driver firmware sizes](https://github.com/georgerobotics/cyw43-driver/tree/main/firmware), [BTstack Pico W footprint (arduino-pico docs)](https://arduino-pico.readthedocs.io/en/latest/bluetooth.html), [cyw43 RAM/flash forum discussion](https://forums.raspberrypi.com/viewtopic.php?t=384445), [Pico SDK networking libraries overview](https://www.raspberrypi.com/documentation/pico-sdk/networking.html)

**Against this project's real budget:**

- **Flash is not the constraint.** ~633 KB used of 2 MB as of the last
  measured point (6B.1) — over a megabyte free, comfortably covers the
  ~300-350 KB these stacks would add.
- **SRAM is.** Free SRAM is **24 KB on Pico 2** right now (the only board
  this could ever apply to). BTstack alone (~20 KB) would consume nearly
  all of that headroom with nothing left over. WiFi (cyw43 + lwIP) needs
  more RAM on its own, in typical configs, than the *entire* current free
  budget — before BTstack is even in the picture.
- Every number in the table above is a third-party estimate, not measured
  on this firmware. **D69's lesson applies directly**: a hand-estimated
  58 KB of Phase 6 headroom turned out to be 14.3 KB once `.data` was
  counted correctly — a ~44 KB miss. Any radio work would need its own
  `size-report.sh` spike against a minimal poll-mode example before any
  of the numbers above are trusted.

## Rough shape, if ever pursued (not scoped, not scheduled)

1. Spike/measure first (vendor `cyw43-driver`/`btstack` as pinned
   submodules, D71's precedent; build the smallest possible poll-mode
   example; measure on the actual board).
2. BLE-only is a plausible feature *if* the spike shows it fits without
   a dedicated SRAM-recovery pass first.
3. WiFi realistically needs an SRAM-recovery project (comparable in scope
   to D70's 54 KB recovery) *before* it is a feature at all, on top of the
   integration work — no guarantee this project has another 20-40 KB left
   to recover the same way twice.
4. Prefer cyw43's poll-mode architecture over its background/IRQ mode —
   it fits the existing single-main-loop model (`main.cpp`'s per-frame
   hooks) without contending with core 1 the way issue #39 did for the
   panel.
