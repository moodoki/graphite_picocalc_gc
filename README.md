# PicoCalc Graphing Calculator

TI-83/84-inspired graphing calculator firmware for the [ClockworkPi PicoCalc](https://www.clockworkpi.com/picocalc), written in C++17 using the Raspberry Pi Pico SDK. Targets both the Pico 1 H (RP2040) and Pico 2 H (RP2350) modules. Personal-use project.

> **Status**: Phase 0 (project setup). Not yet functional. See [project status](#project-status).

## Features (planned)

- **Phase 1**: Scientific calculator with natural math display, single-function graphing, $Y_1 \ldots Y_n$ editor, window settings, trace/zoom.
- **Phase 2**: Multi-function graphing, parametric and polar modes, table view.
- **Phase 3**: Lists, statistics, regression, distributions, statistical plots.
- **Phase 4**: Matrix operations, symbolic math (CAS — differentiation, simplification, factoring, equation solving, basic integration), MicroPython programming environment.
- **Phase 5**: App framework, polish, release.

The TI-83/84 design language is the reference, but the UI is modernized to take advantage of the PicoCalc's 320$\times$320 color display.

## Hardware

| | Pico 1 H (RP2040) | Pico 2 H (RP2350) |
|---|---|---|
| CPU | 2$\times$ Cortex-M0+ @ 133 MHz | 2$\times$ Cortex-M33 @ 150 MHz |
| SRAM | 264 KB | 520 KB |
| Flash | 2 MB | 4 MB |
| FPU | None (ROM softfloat) | Single-precision hardware |

Mainboard (shared): 4" 320$\times$320 IPS LCD (ST7365P over SPI), 67-key QWERTY (STM32 co-processor over I2C), 8 MB PSRAM, 32 GB SD card, piezo buzzer.

## Quick start

```bash
# 1. Install dependencies (macOS)
brew install cmake ninja git python3 picotool
brew install --cask gcc-arm-embedded   # ARM GNU Toolchain → /Applications/ArmGNUToolchain
# (Do NOT use the Homebrew *formula* arm-none-eabi-gcc — it ships without newlib
#  and fails at link with "nosys.specs: No such file or directory".)

# 2. Clone this repo, plus the Pico SDK next to it
git clone <this-repo>
cd picocalc-graphcalc
git clone -b 2.2.0 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git

# 3. Set environment (adjust the toolchain version to what's installed)
export PICO_SDK_PATH="$PWD/pico-sdk"
export PICO_TOOLCHAIN_PATH="/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi"

# 4. Build
./scripts/build-all.sh
# → build/pico/picocalc_graphcalc.uf2
# → build/pico2/picocalc_graphcalc.uf2

# 5. Flash
# Hold BOOTSEL on the PicoCalc, plug in USB-C
cp build/pico/picocalc_graphcalc.uf2 /Volumes/RPI-RP2/
# Or drop the .uf2 onto the SD card if uf2loader is installed
```

See [docs/dev-environment.md](docs/dev-environment.md) for detailed setup instructions.

## Repository layout

```
.
├── AGENTS.md              # AI agent context (read this if you're an agent)
├── CLAUDE.md              # Pointer to AGENTS.md
├── README.md              # You are here
├── CMakeLists.txt         # Top-level build
├── pico_sdk_import.cmake  # Standard SDK import
├── src/                   # Application code (C++17)
│   ├── platform/          # HAL — only layer that touches hardware
│   ├── gfx/               # Framebuffer, fonts, drawing primitives
│   ├── ui/                # Screen manager, widgets
│   ├── math/              # Expression engine, numeric eval
│   │   └── cas/           # (Phase 4) symbolic math
│   ├── render/            # Natural math layout-node renderer
│   └── apps/              # Screen implementations (home, graph, etc.)
├── drivers/               # Vendored C drivers (Coyote OS) — read-only
├── assets/                # Bitmap fonts, icons
├── docs/
│   ├── phases/            # Per-phase specs and plans
│   ├── architecture.md    # Layer rules
│   ├── dev-environment.md # Toolchain setup
│   ├── hardware.md        # PicoCalc hardware reference
│   └── notes/             # Design notes, decisions
├── scripts/               # Helpers: build, format, lint, flash
├── .clangd                # clangd config
├── .clang-format          # Formatting rules
├── .clang-tidy            # Static analysis rules
└── .editorconfig          # Universal editor settings
```

## Documentation

- **[docs/phases/phase0-prep.md](docs/phases/phase0-prep.md)** — environment setup, repo bootstrap (do this first)
- **[docs/phases/phase1-plan.md](docs/phases/phase1-plan.md)** — current phase implementation plan
- **[docs/phases/phase1-spec.md](docs/phases/phase1-spec.md)** — Phase 1 design contract
- **[docs/phases/phase4-spec.md](docs/phases/phase4-spec.md)** — Phase 4 design contract (CAS, matrix, programming)
- **[docs/architecture.md](docs/architecture.md)** — system architecture
- **[docs/hardware.md](docs/hardware.md)** — hardware reference
- **[docs/dev-environment.md](docs/dev-environment.md)** — macOS Apple Silicon dev setup
- **[AGENTS.md](AGENTS.md)** — for AI coding agents

Background research:

- **[docs/notes/feasibility.md](docs/notes/feasibility.md)** — initial feasibility analysis (kept for reference)

## Project status

| Phase | Status | Notes |
|-------|--------|-------|
| 0: Prep | In progress | Environment setup, repo bootstrap |
| 1: HAL + calculator + basic graphing | Not started | ~10 weeks part-time |
| 2: Full graphing + table | Not started | Spec pending |
| 3: Statistics | Not started | Spec pending |
| 4: CAS + matrix + MicroPython | Specced, not started | ~10 weeks part-time |
| 5: App framework + polish | Not started | Spec pending |

## Acknowledgments

- [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) by laingcc — peripheral drivers, foundational reference
- [Delta Pico](https://github.com/AaronC81/delta-pico) by Aaron Christiansen — `rbop` natural math renderer architecture, design reference
- [DB48X](https://github.com/c3d/db48x) by Christophe de Dinechin — embedded CAS reference architecture
- [ClockworkPi](https://www.clockworkpi.com/) — the PicoCalc hardware
- [tinyexpr++](https://github.com/Blake-Madden/tinyexpr-plus-plus) — expression parser/evaluator

## License

Personal-use project. License terms TBD.
