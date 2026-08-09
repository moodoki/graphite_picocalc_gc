# Graphite — a graphing calculator for the PicoCalc

TI-83/84-inspired graphing calculator firmware for the
[ClockworkPi PicoCalc](https://www.clockworkpi.com/picocalc), written in C++17
on the Raspberry Pi Pico SDK. Runs on both the Pico 1 H (RP2040) and Pico 2 H
(RP2350) modules from one source tree.

**[Download v0.4.0](https://github.com/moodoki/graphite_picocalc_gc/releases/latest)**
· [Features](FEATURES.md) · [How to use it](USAGE.md) ·
[Roadmap](ROADMAP.md) · [Contributing](CONTRIBUTING.md)

> **Status**: feature-complete as a graphing calculator, plus a symbolic CAS.
> Phases 0 through 5.2 are done and hardware-verified on both boards. Phase 6
> (an app framework and MicroPython) is specced and not started. Full
> breakdown in [ROADMAP.md](ROADMAP.md).

## What it does

- **Scientific and symbolic** — natural math display with stacked fractions and
  raised exponents; exact forms (`sqrt(8)` shows as `2√2`, `sin(pi/3)` as
  `√3/2`); a CAS with `simplify`, `expand`, `factor`, `diff`, `integ` and
  `solve`.
- **Graphing** — function, parametric, polar and sequence modes, with trace,
  zoom, shading, a TI-84-style CALC menu (zero, min/max, intersect, `dy/dx`,
  `fnInt`), value tables, and split-screen graph|table.
- **Statistics** — six lists plus named lists, 1-var and 2-var stats, ten
  regression models, seven distributions, the full inference suite, and stat
  plots.
- **Linear algebra** — ten matrix variables with determinant, inverse,
  row-echelon form, eigenvalues and eigenvectors.
- **Complex numbers** throughout — `a+bi` and `r∠θ` modes, with complex-valued
  variables, lists and matrices.
- **Persistence** — everything survives a power cycle, on the SD card.

The full list is in [FEATURES.md](FEATURES.md). The TI-83/84 is the reference
for behaviour, but the UI is modernized for the PicoCalc's $320\times320$ color
display rather than reproducing a $96\times64$ monochrome one.

## Hardware

| | Pico 1 H (RP2040) | Pico 2 H (RP2350) |
|---|---|---|
| CPU | $2\times$ Cortex-M0+ @ 133 MHz | $2\times$ Cortex-M33 @ 150 MHz |
| SRAM | 264 KB | 520 KB |
| Flash | 2 MB | 4 MB |
| FPU | None (ROM softfloat) | Single-precision hardware |

Mainboard (shared): 4" $320\times320$ IPS LCD (ST7365P over SPI), 67-key QWERTY
keyboard (STM32 co-processor over I2C), 8 MB PSRAM, SD card, piezo buzzer.
Details in [docs/hardware.md](docs/hardware.md).

## Install

Grab `picocalc_graphcalc-pico.uf2` (Pico 1) or `picocalc_graphcalc-pico2.uf2`
(Pico 2) from the
[latest release](https://github.com/moodoki/graphite_picocalc_gc/releases/latest),
hold **BOOTSEL** while plugging in USB-C, and drop the file onto the volume
that appears. Then read [USAGE.md](USAGE.md).

## Quick start (building from source)

```bash
# 1. Dependencies (macOS)
brew install cmake ninja git python3 picotool
brew install --cask gcc-arm-embedded   # ARM GNU Toolchain → /Applications/ArmGNUToolchain
# Do NOT use the Homebrew *formula* arm-none-eabi-gcc — it ships without newlib
# and fails at link with "nosys.specs: No such file or directory".

# 2. Clone this repo, plus the Pico SDK inside it
git clone https://github.com/moodoki/graphite_picocalc_gc.git
cd graphite_picocalc_gc
git clone -b 2.2.0 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git

# 3. Environment (the toolchain path carries its version — check yours with
#    ls -d /Applications/ArmGNUToolchain/*/arm-none-eabi | sort -V | tail -1)
export PICO_SDK_PATH="$PWD/pico-sdk"
export PICO_TOOLCHAIN_PATH="/Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi"

# 4. Build → build/pico{,2}/picocalc_graphcalc.uf2
./scripts/build-all.sh

# 5. Flash — hold BOOTSEL, plug in USB-C
cp build/pico/picocalc_graphcalc.uf2 /Volumes/RPI-RP2/
```

Already running Graphite? **Home → `F4` MODE → "Reboot to bootloader"** saves
reaching for the button. Full setup notes:
[docs/dev-environment.md](docs/dev-environment.md).

## Documentation

| If you want to… | Read |
|---|---|
| Use the calculator | [USAGE.md](USAGE.md), and `F5` HELP on the device |
| Know what it can do | [FEATURES.md](FEATURES.md) |
| See what's built and what's next | [ROADMAP.md](ROADMAP.md) |
| Build, test or contribute | [CONTRIBUTING.md](CONTRIBUTING.md) |
| Understand *how* it works | [docs/architecture.md](docs/architecture.md) and the decision log, [docs/notes/decisions.md](docs/notes/decisions.md) |
| Work on it as an AI agent | [AGENTS.md](AGENTS.md) |

The decision log is the one to read if you are curious rather than
contributing. It records why things are the way they are — including the
mistakes, the rejected alternatives, and the several bugs that only appeared
on hardware.

## Acknowledgments

- [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) by laingcc —
  peripheral drivers and foundational reference
- [Delta Pico](https://github.com/AaronC81/delta-pico) by Aaron Christiansen —
  the `rbop` natural math renderer architecture, a design reference
- [DB48X](https://github.com/c3d/db48x) by Christophe de Dinechin — embedded
  CAS reference architecture
- [tinyexpr](https://github.com/codeplea/tinyexpr) by Lewis Van Winkle —
  expression parser
- [ClockworkPi](https://www.clockworkpi.com/) — the PicoCalc hardware

## License

A personal project, for fun, education and sharing.

- **This project's own code** (`src/`, `tests/`, `scripts/`, `docs/`) is
  **MIT** — see [LICENSE](LICENSE). Reuse the math engine, graph subsystem or
  renderer freely.
- **The combined firmware binary** links GPL-2.0 vendored drivers (display and
  keyboard, from Coyote OS), so the firmware as a whole is distributed under
  **GPL-2.0**.
- **The font headers** in `src/gfx/fonts/` carry their upstream fonts'
  licenses (SIL OFL 1.1, BSD-2-Clause, public domain), not MIT.

[NOTICE.md](NOTICE.md) has the full component table and scopes what a fully
permissive firmware would take — a future option, not a roadmap item.
