# Graphite - PicoCalc Graphing Calculator

TI-83/84-inspired graphing calculator firmware for the [ClockworkPi PicoCalc](https://www.clockworkpi.com/picocalc), written in C++17 using the Raspberry Pi Pico SDK. Targets both the Pico 1 H (RP2040) and Pico 2 H (RP2350) modules. Personal-use project.

> **Status**: Phases 0–3 complete and hardware-verified on both Pico 1 and
> Pico 2 (parametric/polar modes, tables, split-screen, built-in help,
> unified persistence; statistics — lists, regression, distributions,
> inference, stat plots). **Phase 4 (sub-phases 4A–4D) is now complete and
> hardware-verified** on both boards: matrices, graph-analysis CALC menu,
> complex numbers (4A–4C), plus GC completeness (4D) — complex
> variables/lists/matrices, sequence graphing, zoom and shading, list↔matrix/
> constants/units glue, named lists, ENG/`>frac`/`π`-tick display polish,
> eigenvectors, and auto-power-down + brightness. All nine 4D batches passed
> their on-device evals on the Pico 1; the Pico 2 leg is closed as a
> formality (board-independent logic, same precedent as earlier phases).
> **Phase 4's completion is the project's pre-release milestone** — a
> feature-complete TI-83/84+-class graphing calculator, tagged
> [v0.1.0](https://github.com/moodoki/graphite_picocalc_gc/releases/tag/v0.1.0)
> (prebuilt UF2s for both boards); CI (build, lint, docs validation, release)
> is green on every job. **Phase 5 (symbolic CAS) is complete and
> hardware-verified on both boards, now merged to `main` and tagged
> [v0.2.0](https://github.com/moodoki/graphite_picocalc_gc/releases/tag/v0.2.0)**:
> the engine (simplify, expand, factor,
> differentiate, solve, integrate), its home-screen UI integration (inline
> calls, F6 CAS menu, `cas` command), exact-form display (`sqrt(8)` shown as
> `2√2`, `pi*2` as `2π`, `1/3` as a stacked fraction, `sin(pi/3)` as `√3/2`
> — with Alt+Enter as the decimal escape), and a Stage 5 hardening pass that
> moved the CAS passes' working arrays off a 4 KB stack they could silently
> overrun, gave the recursion stated depth caps, and turned pool exhaustion
> into a reported error instead of a plausible-looking wrong answer. **Next up
> are two dotted sub-phases** — work that turned up rather than planned phase
> goals (see `AGENTS.md` for what the dotted-vs-lettered numbering means):
> **Phase 5.1**, serial line injection for on-device test automation, and
> **Phase 5.2**, the unified evaluator, which replaces the three home-screen
> mini-evaluators with one built on an explicit evaluation stack. Both sit
> ahead of Phase 6. See
> [`docs/notes/next-session.md`](docs/notes/next-session.md) for the current
> handoff and [`docs/notes/worklog.md`](docs/notes/worklog.md) for history.

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
- **Phase 2 (complete, HW-verified on both boards)**: Graph modes — **parametric**
  ($X_{nT}(t), Y_{nT}(t)$ pairs) and **polar** ($r_n(\theta)$, angle-mode aware)
  alongside function mode, selected from the MODE screen; mode-aware
  Y=/parametric/polar editors and window screen (Tmin/Tmax/Tstep, $\theta$
  range); **value table** for every mode with auto (infinite scroll) and ask
  modes plus horizontal column scrolling; **split-screen graph|table** with
  pane focus and trace↔row sync; **built-in help browser** (function catalog
  driven by the same table the parser registers from, key reference, syntax
  notes); all graphing state persists in one SD file with automatic migration
  from the Phase 1 format.
- **Phase 3 (complete, HW-verified on both boards)**: Six data lists
  ($L_1 \ldots L_6$) backed by a shared `Array` primitive with a
  spreadsheet-style list editor; 1-var/2-var descriptive statistics and all
  ten TI-style regression models; probability distributions (PDF/CDF/inverse)
  for normal, $t$, $\chi^2$, $F$, binomial, Poisson, and geometric; the full
  inference suite (hypothesis tests, confidence intervals, ANOVA);
  statistical plots (histogram, box plot, scatter) overlaid on the graphing
  engine.
- **Phase 4 — the pre-release milestone (complete, HW-verified on both
  boards)**: 10 matrix variables (`[A]`–`[J]`) with arithmetic, determinant,
  inverse, transpose, row-echelon form, eigenvalues, eigenvectors, and a
  numeric equation solver (4A + 4D.23); a TI-84-style **CALC menu** on the
  graph screen — value, zero, min/max, intersect, `dy/dx`, numeric integral
  — across function/parametric/polar modes (4B); **complex numbers** with
  `a+bi`/polar (`r∠θ`) display modes, complex-aware arithmetic and
  elementary functions, complex-valued variables/lists/matrices with full
  complex linear algebra, and complex matrix eigenvalue spectra (4C + 4D).
  A swappable 8x16 font system with real math glyphs
  (`π θ σ Σ μ λ ≠ √ ∠ ⇒ …`) ships alongside this work. Sub-phase **4D (GC
  completeness)** closes out the remaining TI-83/84+ parity gaps — sequence
  graphing, fuller zoom/shading (ZBox/ZDecimal/ZSquare, curve/band
  shading), list↔matrix conversion, scientific constants, unit conversions,
  home-screen matrix literals, complex-valued variable/list/matrix storage,
  named lists, and device polish (auto power-off, brightness persistence) —
  making Phase 4's completion the point at which the calculator is
  feature-complete as a graphing calculator, independent of CAS or
  programmability. See
  [docs/phases/phase4-spec.md](docs/phases/phase4-spec.md).
- **Phase 5 (complete, `phase-5` branch pending merge)**: symbolic math
  (CAS) — simplify, expand, factor, differentiate, solve (complex-aware),
  and a bounded form of symbolic integration, HW-verified on both boards
  and reachable inline from the home screen (`diff()`, `integ()`,
  `factor()`, `expand()`, `simplify()`, `solve()`) or via the F6 CAS menu.
  Exact-value display also ships: results with a clean closed form typeset
  in amber instead of a decimal — `sqrt(8)` as `2√2`, `1/sqrt(2)` as `√2/2`,
  `pi*2` as `2π`, `1/3` as a stacked fraction, and `sin(pi/3)` as `√3/2`
  (special-angle trig, in both RADIAN and DEGREE). Alt+Enter is the decimal
  escape. Stage 5 hardening closed the phase: the simplifier's per-call
  working arrays moved off the stack into the expression pool (they were
  ~1.1 KB frames nesting on a 4 KB stack shared with core 1 — a silent
  overrun reproduced on hardware), parser and simplifier recursion gained
  stated depth caps sized to measured frame sizes, and pool exhaustion is
  now reported as an error rather than returning an unconverged tree that
  looks converged. See
  [docs/phases/phase5-spec.md](docs/phases/phase5-spec.md).
- **Phases 5.1 and 5.2**: two *dotted* sub-phases — significant work that
  turned up rather than planned phase goals (see `AGENTS.md` for the
  numbering convention). **5.1 (complete, HW-verified)** adds serial line
  injection so a host script can submit expressions to the home screen over
  USB and read back the result and its kind, turning hand-driven bench
  checks into repeatable ones. **5.2 (code-complete, hardware verification
  pending)** is the unified evaluator: one tagged-value evaluator — a
  shunting-yard compiler emitting a flat RPN program, run by a stack machine
  — replacing the three home-screen mini-evaluators, motivated by two
  independent findings: the real and complex evaluators silently disagreeing
  on DEGREE-mode trig (D46), and four parsers each needing a
  separately-discovered stack budget, three found by a crash (D48). As built
  it deletes 3,903 lines and three of those four depth caps, returns ~6.9 KB
  of static RAM, and found a *second* live disagreement between the two
  shipped evaluators along the way (`(-2)^2`, D50). Every behaviour that
  changed is recorded in
  [docs/notes/unified-evaluator-changes.md](docs/notes/unified-evaluator-changes.md).
  See [docs/phases/phase5.1-spec.md](docs/phases/phase5.1-spec.md)
  and [docs/phases/phase5.2-spec.md](docs/phases/phase5.2-spec.md).
- **Phase 6 (planned)**: non-calculator functions — an app-launcher
  framework (6A) and MicroPython as its first base app (6B), plus room
  for future apps and release engineering (docs site, versioned
  releases) as unscoped, order-independent sub-phases. See
  [docs/phases/phase6-spec.md](docs/phases/phase6-spec.md).

The TI-83/84 design language is the reference, but the UI is modernized to take advantage of the PicoCalc's $320\times320$ color display.

## Hardware

| | Pico 1 H (RP2040) | Pico 2 H (RP2350) |
|---|---|---|
| CPU | $2\times$ Cortex-M0+ @ 133 MHz | $2\times$ Cortex-M33 @ 150 MHz |
| SRAM | 264 KB | 520 KB |
| Flash | 2 MB | 4 MB |
| FPU | None (ROM softfloat) | Single-precision hardware |

Mainboard (shared): 4" $320\times320$ IPS LCD (ST7365P over SPI), 67-key QWERTY (STM32 co-processor over I2C), 8 MB PSRAM, 32 GB SD card, piezo buzzer.

## Quick start

```bash
# 1. Install dependencies (macOS)
brew install cmake ninja git python3 picotool
brew install --cask gcc-arm-embedded   # ARM GNU Toolchain → /Applications/ArmGNUToolchain
# (Do NOT use the Homebrew *formula* arm-none-eabi-gcc — it ships without newlib
#  and fails at link with "nosys.specs: No such file or directory".)

# 2. Clone this repo, plus the Pico SDK next to it
git clone <this-repo>
cd picocalc_gc
git clone -b 2.2.0 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git

# 3. Set environment (the toolchain path carries its version — check yours with
#    ls -d /Applications/ArmGNUToolchain/*/arm-none-eabi | sort -V | tail -1)
export PICO_SDK_PATH="$PWD/pico-sdk"
export PICO_TOOLCHAIN_PATH="/Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi"

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
./scripts/host-tests.sh   # test_math, test_layout, test_graph, test_lists, test_stats,
                           # test_dist, test_infer, test_analysis, test_matrix,
                           # test_complex(_expr), test_solve — 1200+ checks
./scripts/lint.sh         # clang-format check + clang-tidy (warnings are errors)
```

The tests cover expression evaluation, the function catalog (the same table the
parser registers from), angle modes, number formatting (FLOAT/FIX/SCI),
variables/store, viewport transforms, the function/parametric/polar point
sources, trace stepping, the mode-aware table model, lists/statistics/
regression, distributions and inference, matrices and the numeric solver,
graph-analysis (CALC menu), and complex-number arithmetic/display. They are
the primary correctness check between on-device sessions.

## Using the calculator

There is also a built-in help browser on the device: **Home → `F5` HELP**
(function catalog, per-screen key reference, syntax notes).

- **Home**: type an expression, `ENTER` to evaluate. `UP`/`DOWN` walk back/forward
  through past inputs (shell-style); `Alt+UP`/`Alt+DOWN` (or `Ctrl+`) scroll the
  history view — the keyboard's STM32 translates Shift chords into their own
  scan codes rather than passing Shift through (arrows are swallowed entirely;
  `Shift+Enter` arrives as `INS`), so bindings use Alt/Ctrl.
  Results with a clean closed form show it in amber instead of a decimal
  (`sqrt(8)` as `2√2`, `1/3`, `sin(pi/3)` as `√3/2`); `Alt+ENTER` — or a
  trailing `>dec` — gives the decimal, and on an empty input line `Alt+ENTER`
  re-runs the last exact result as a decimal.
  Store with `2->A` (`e` is Euler's constant; variable `E` is reserved). Softkeys:
  `F1` Y= editor, `F2` window, `F3` graph, `F4` mode, `F5` help, `F6` (= `Shift+F1`)
  hardware diagnostics. `HOME` returns here from anywhere; `F6`/`ESC` exits diagnostics.
- **Mode**: angle (RAD/DEG), display format (FLOAT/FIX/SCI), fix digits,
  **graph mode (FUNC/PARAM/POLAR)**, **number mode (REAL/`a+bi`/`r∠θ`)**, and
  reboot to the USB bootloader for flashing.
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
- **Lists & stats** (typed `lists`/`stats`, or `list`/`stat`): a spreadsheet-style
  editor for six lists ($L_1 \ldots L_6$); 1-var/2-var descriptive stats,
  regression models, distributions (PDF/CDF/inverse), the inference suite, and
  stat plots (histogram/box/scatter) overlaid on the graph engine.
- **Matrices** (TI-style `[A]` … `[J]` bracket typing, or the matrix editor):
  arithmetic, determinant, inverse, transpose, row-echelon form, `eigenvals`/
  `eig`, and a numeric equation solver.
- **CALC menu** (`F6` on the graph screen, or typed `calc`/`analyze`): value,
  zero, min/max, intersect, `dy/dx`, and numeric `fnInt`, cursor-driven on the
  graph curve across function/parametric/polar modes.
- **Complex numbers**: home-screen expressions like `3+2i` or `sqrt(-4)`
  evaluate per the MODE Number row (REAL results say "Non-real result" instead
  of `NaN`; `a+bi`/`r∠θ` modes display the complex value).
- **Fonts**: the 8x16 main font is a build-time choice —
  `-DPICOCALC_FONT=spleen|juliamono|iosevka|unifont|terminus` (default
  `terminus`) — all carrying the same math-glyph slot map.

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
│   ├── math/              # Expression engine, function catalog, numeric eval,
│   │                      #   lists/stats/distributions/inference, matrices,
│   │                      #   complex numbers, cas/ (Phase 5, complete   
│   │                      #   on the `phase-5` branch)
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
- **[docs/phases/phase2-spec.md](docs/phases/phase2-spec.md)** — Phase 2 design contract (complete; retro in [docs/notes/phase2-retro.md](docs/notes/phase2-retro.md))
- **[docs/phases/phase3-spec.md](docs/phases/phase3-spec.md)** — Phase 3 design contract (statistics; code-complete)
- **[docs/phases/phase4-spec.md](docs/phases/phase4-spec.md)** — Phase 4 design contract, the pre-release milestone (matrix, graph analysis, complex numbers, GC completeness; complete, HW-verified on both boards)
- **[docs/phases/phase5-spec.md](docs/phases/phase5-spec.md)** — Phase 5 design contract (CAS: simplify, expand, factor, differentiate, solve, integrate; complete and merged — engine, UI integration, exact-form display and Stage 5 hardening, HW-verified on both boards)
- **[docs/phases/phase5.1-spec.md](docs/phases/phase5.1-spec.md)** — Phase 5.1 design contract (serial line injection for on-device test automation; specced, not started)
- **[docs/phases/phase5.2-spec.md](docs/phases/phase5.2-spec.md)** — Phase 5.2 design contract (unified evaluator, idea F — one tagged-value evaluator on an explicit stack; specced, not started)
- **[docs/phases/phase6-spec.md](docs/phases/phase6-spec.md)** — Phase 6 design contract (non-calculator functions: app framework, MicroPython; specced, not started)
- **[docs/architecture.md](docs/architecture.md)** — system architecture
- **[docs/hardware.md](docs/hardware.md)** — hardware reference
- **[docs/dev-environment.md](docs/dev-environment.md)** — macOS Apple Silicon dev setup
- **[docs/notes/next-session.md](docs/notes/next-session.md)** — start-here handoff for the next dev session
- **[docs/notes/next-bench-session.md](docs/notes/next-bench-session.md)** — deferred hardware bench work (D14 rail settle)
- **[docs/notes/wishlist.md](docs/notes/wishlist.md)** — desired-but-unplanned features
- **[docs/notes/decisions.md](docs/notes/decisions.md)** — architecture & design decision log
- **[docs/notes/ti-parity.md](docs/notes/ti-parity.md)** — feature parity stocktake vs. TI-83/84+ and TI-Nspire CX II CAS
- **[docs/references/risch-algorithm.md](docs/references/risch-algorithm.md)** — reading list for symbolic integration, and why Phase 5's integrator stops where it does (the limit is differential algebra, not the hardware)
- **[docs/notes/design-departures-matrix-complex.md](docs/notes/design-departures-matrix-complex.md)** — unbuilt ideas for first-class matrices/vectors/complex numbers
- **[docs/notes/docs-site-plan.md](docs/notes/docs-site-plan.md)** — plan for a public GitHub Pages docs site with TI-guidebook-style workbooks
- **[AGENTS.md](AGENTS.md)** — for AI coding agents

Background research:

- **[docs/notes/feasibility.md](docs/notes/feasibility.md)** — initial feasibility analysis (kept for reference)

## Project status

| Phase | Status | Notes |
|-------|--------|-------|
| 0: Prep | Complete | Environment, repo, vendored drivers |
| 1: HAL + calculator + basic graphing | **Complete** | HW-verified on Pico 1 + Pico 2 (retro: docs/notes/phase1-retro.md) |
| 2: Graph modes + table + split + help | **Complete** | HW-verified on Pico 1 + Pico 2 (retro: docs/notes/phase2-retro.md; Pico 1 pass via task 3D.14) |
| 3: Statistics | **Complete** | Lists, regression, distributions, inference, stat plots; HW-verified on Pico 1 + Pico 2 (retro: docs/notes/phase3-retro.md; Pico 1 pass via task 3D.14) |
| 4A–4C: Matrix + graph analysis + complex numbers | **Complete** | HW-verified on Pico 1 + Pico 2 (D28/D29/D30) |
| 4D: GC completeness (pre-release milestone) | **Complete** | All 9 D38 batches shipped 2026-07-26, HW-verified on Pico 1 2026-07-26/27; Pico 2 leg closed as a formality; docs/phases/phase4-spec.md §7, decisions.md D40 |
| 5: CAS (symbolic math) | **Complete** | Stages 0-5: engine, UI integration, exact-form display, hardening; HW-verified on Pico 1 + Pico 2 2026-08-05; merged to `main` and tagged v0.2.0 2026-08-08; docs/phases/phase5-spec.md (D32, D41, D42, D43, D44, D45) |
| 5.1: Serial line injection | **Complete** | Dev tooling — `scripts/serial-console.py` submits expressions over USB serial and reads back result + kind; HW-verified on Pico 2 2026-08-09 (the D48 det ladder now runs unattended); docs/phases/phase5.1-spec.md (D48) |
| 5.2: Unified evaluator (idea F) | Specced, not started | Replaces matexpr/complexexpr/listexpr with one tagged-value evaluator on an explicit stack; highest-risk item on the list; docs/phases/phase5.2-spec.md (D37, D40, D46, D48) |
| 6: Non-calculator functions (app framework + MicroPython) | Specced, not started | docs/phases/phase6-spec.md (D33) |

Both boards build clean and the host test suite (1300+ checks) passes. See
[docs/notes/next-session.md](docs/notes/next-session.md) for exactly what's
HW-PENDING right now.

## Acknowledgments

- [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) by laingcc — peripheral drivers, foundational reference
- [Delta Pico](https://github.com/AaronC81/delta-pico) by Aaron Christiansen — `rbop` natural math renderer architecture, design reference
- [DB48X](https://github.com/c3d/db48x) by Christophe de Dinechin — embedded CAS reference architecture
- [ClockworkPi](https://www.clockworkpi.com/) — the PicoCalc hardware
- [tinyexpr](https://github.com/codeplea/tinyexpr) by Lewis Van Winkle — expression parser/evaluator

## License

A personal project for fun, education, and sharing.

- **This project's own code** (`src/`, `tests/`, `scripts/`, `docs/`) is
  **MIT-licensed** — see [LICENSE](LICENSE). Reuse the math engine, graph
  subsystem, or renderer freely.
- **The combined firmware binary** links GPL-2.0 vendored drivers (display,
  keyboard, and their bitmap font from Coyote OS), so the firmware as a whole
  is distributed under **GPL-2.0**. See [NOTICE.md](NOTICE.md) for the full
  component/license table.

A fully permissive firmware would require rewriting the two GPL drivers and
swapping the font; the effort is scoped in NOTICE.md as a future option, but
it is not on any roadmap.
