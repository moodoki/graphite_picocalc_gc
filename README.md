# PicoCalc Graphing Calculator

TI-83/84-inspired graphing calculator firmware for the [ClockworkPi PicoCalc](https://www.clockworkpi.com/picocalc), written in C++17 using the Raspberry Pi Pico SDK. Targets both the Pico 1 H (RP2040) and Pico 2 H (RP2350) modules. Personal-use project.

> **Status**: Phase 1 complete and hardware-verified on both boards. Phase 2
> code-complete (parametric/polar modes, tables, split-screen, built-in help,
> unified persistence); on-device verification pending — see
> [`docs/notes/next-session.md`](docs/notes/next-session.md) for the test-drive
> checklist and [`docs/notes/worklog.md`](docs/notes/worklog.md) for history.

## Features

- **Phase 1 (done, HW-verified)**: Scientific calculator with natural math display
  (stacked fractions, raised exponents, auto-scaling parens), variables A–Z + Ans +
  theta with a `->` store operator, expression history with recall, degree/radian
  modes, FLOAT/FIX/SCI display formats, a $Y_1 \ldots Y_7$ function editor, function
  graphing with axes/grid, multi-function color plots, discontinuity handling, trace,
  and zoom (in/out, standard, trig). History, variables, and graph state persist to
  the SD card. The status bar shows angle/display mode and battery level (cached
  STM32 read, refreshed every 30 s; cyan while charging; shows `--` on units whose
  STM32 keyboard firmware predates the battery register).
- **Phase 2 (code-complete)**: Graph modes — **parametric** ($X_{nT}(t), Y_{nT}(t)$
  pairs) and **polar** ($r_n(\theta)$, angle-mode aware) alongside function mode,
  selected from the MODE screen; mode-aware Y=/parametric/polar editors and window
  screen (Tmin/Tmax/Tstep, θ range); **value table** for every mode with auto
  (infinite scroll) and ask modes plus horizontal column scrolling; **split-screen
  graph|table** with pane focus and trace↔row sync; **built-in help browser**
  (function catalog driven by the same table the parser registers from, key
  reference, syntax notes); all graphing state persists in one SD file with
  automatic migration from the Phase 1 format.
- **Phase 3 (planned)**: Lists, statistics, regression, distributions, statistical plots.
- **Phase 4 (planned)**: Matrix operations, symbolic math (CAS — differentiation, simplification, factoring, equation solving, basic integration), MicroPython programming environment.
- **Phase 5 (planned)**: App framework, polish, release.

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
# (From the running firmware: Home → F4 MODE → "Reboot to bootloader" → ENTER.)
```

See [docs/dev-environment.md](docs/dev-environment.md) for detailed setup instructions.

## Host tests & lint

The math engine, layout builder, and graph subsystem have host-side unit tests
that run on your development machine (no Pico hardware or cross-toolchain needed):

```bash
./scripts/host-tests.sh   # test_math + test_layout + test_graph (200+ checks)
./scripts/lint.sh         # clang-format check + clang-tidy (warnings are errors)
```

The tests cover expression evaluation, the function catalog (the same table the
parser registers from), angle modes, number formatting (FLOAT/FIX/SCI),
variables/store, viewport transforms, the function/parametric/polar point
sources, trace stepping, and the mode-aware table model. They are the primary
correctness check between on-device sessions.

## Using the calculator

There is also a built-in help browser on the device: **Home → `F5` HELP**
(function catalog, per-screen key reference, syntax notes).

- **Home**: type an expression, `ENTER` to evaluate. `UP`/`DOWN` walk back/forward
  through past inputs (shell-style); `Alt+UP`/`Alt+DOWN` (or `Ctrl+`) scroll the
  history view — the keyboard's STM32 swallows Shift on arrows.
  Store with `2->A` (`e` is Euler's constant; variable `E` is reserved). Softkeys:
  `F1` Y= editor, `F2` window, `F3` graph, `F4` mode, `F5` help, `F6` (= `Shift+F1`)
  hardware diagnostics. `HOME` returns here from anywhere; `F6`/`ESC` exits diagnostics.
- **Mode**: angle (RAD/DEG), display format (FLOAT/FIX/SCI), fix digits,
  **graph mode (FUNC/PARAM/POLAR)**, and reboot to the USB bootloader for flashing.
- **Editors** (`F5` from the graph opens the active mode's editor): `UP`/`DOWN`
  select, `ENTER`/`F1` edit, `F2` toggle enable, `F3` clear, `F4` graph. The
  parametric editor shows six $X_{nT}/Y_{nT}$ pairs (committing an X expression
  auto-focuses its empty partner); the polar editor shows $r_1 \ldots r_6$ with
  `theta` typed out.
- **Window**: mode-aware — adds `Tmin/Tmax/Tstep` in parametric mode and
  `THmin/THmax/THstep` in polar mode ahead of the shared x/y fields.
- **Graph**: `F1` trace (`LEFT`/`RIGHT` move, `UP`/`DOWN` switch curve; readout
  shows `x/y`, `t`, or `th` per mode), `F2`/`F3` zoom in/out, `S`/`T`
  standard/trig presets, `F4` table, `F5` editor, `F9` (= `Shift+F4`) split-screen.
- **Table** (`F4` from the graph): columns adapt to the mode (`x|Y…`,
  `T|X1T Y1T…`, `th|r…`). Auto mode scrolls infinitely in both directions;
  ask mode accumulates typed values (`ENTER` adds, `F5` deletes). `LEFT`/`RIGHT`
  scroll columns; `F1` table setup (Start/Step/AUTO-ASK); `F4`/`ESC` back to graph.
- **Split-screen** (`F9` from graph or table): graph pane above, table below;
  `F4` switches the focused pane, trace and table row stay in sync, `F9`/`ESC`
  exits.

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
│   ├── math/              # Expression engine, function catalog, numeric eval
│   │   └── cas/           # (Phase 4) symbolic math
│   ├── render/            # Natural math layout-node renderer
│   ├── graph/             # Graphing subsystem: viewport, plotter, modes,
│   │                      #   point sources, trace, persisted GraphState
│   └── apps/              # Screen implementations (home, graph, table, help…)
├── drivers/               # Vendored C drivers (Coyote OS) — read-only
├── tests/host/            # Host-side unit tests (run via scripts/host-tests.sh)
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
- **[docs/phases/phase1-spec.md](docs/phases/phase1-spec.md)** / **[phase1-plan.md](docs/phases/phase1-plan.md)** — Phase 1 design contract + plan (complete; retro in [docs/notes/phase1-retro.md](docs/notes/phase1-retro.md))
- **[docs/phases/phase2-spec.md](docs/phases/phase2-spec.md)** — Phase 2 design contract (current phase)
- **[docs/phases/phase3-spec.md](docs/phases/phase3-spec.md)** — Phase 3 design contract (statistics)
- **[docs/phases/phase4-spec.md](docs/phases/phase4-spec.md)** — Phase 4 design contract (CAS, matrix, programming)
- **[docs/architecture.md](docs/architecture.md)** — system architecture
- **[docs/hardware.md](docs/hardware.md)** — hardware reference
- **[docs/dev-environment.md](docs/dev-environment.md)** — macOS Apple Silicon dev setup
- **[docs/notes/decisions.md](docs/notes/decisions.md)** — architecture & design decision log
- **[AGENTS.md](AGENTS.md)** — for AI coding agents

Background research:

- **[docs/notes/feasibility.md](docs/notes/feasibility.md)** — initial feasibility analysis (kept for reference)

## Project status

| Phase | Status | Notes |
|-------|--------|-------|
| 0: Prep | Complete | Environment, repo, vendored drivers |
| 1: HAL + calculator + basic graphing | **Complete** | HW-verified on Pico 1 + Pico 2 (retro: docs/notes/phase1-retro.md) |
| 2: Graph modes + table + split + help | **Code complete** | Both boards build; 200+ host checks green; on-device test drive pending |
| 3: Statistics | Specced | docs/phases/phase3-spec.md |
| 4: CAS + matrix + MicroPython | Specced | ~10 weeks part-time |
| 5: App framework + polish | Not started | Spec pending |

## Acknowledgments

- [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) by laingcc — peripheral drivers, foundational reference
- [Delta Pico](https://github.com/AaronC81/delta-pico) by Aaron Christiansen — `rbop` natural math renderer architecture, design reference
- [DB48X](https://github.com/c3d/db48x) by Christophe de Dinechin — embedded CAS reference architecture
- [ClockworkPi](https://www.clockworkpi.com/) — the PicoCalc hardware
- [tinyexpr](https://github.com/codeplea/tinyexpr) by Lewis Van Winkle — expression parser/evaluator

## License

Personal-use project. License terms TBD.
