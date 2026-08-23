# Features

What Graphite does, organized by capability. For the order things were built in
and what's still ahead, see [ROADMAP.md](ROADMAP.md); for how to drive it, see
[USAGE.md](USAGE.md).

Everything below is implemented and hardware-verified on both the Pico 1 H
(RP2040) and Pico 2 H (RP2350) unless noted.

## Calculating

- **Natural math display** — stacked fractions, raised exponents, auto-scaling
  parentheses. Expressions render as they would be written, not as a flat line
  of ASCII.
- **Variables** `a`–`z`, `theta` and `ans`, with a TI-style `->` store operator
  (`2->a`). Input is case-sensitive and variables are lowercase. `e` and `i`
  are reserved — Euler's number and the imaginary unit — so 24 letters are
  free.
- **Expression history** with shell-style recall (`UP`/`DOWN`) and a separately
  scrollable history view.
- **Angle modes** (RAD/DEG) and **display formats** (FLOAT/FIX/SCI, plus ENG
  and `>frac`).
- **Exact-form results** — a result with a clean closed form is typeset in
  amber instead of a decimal: `sqrt(8)` as `2√2`, `1/sqrt(2)` as `√2/2`,
  `pi*2` as `2π`, `1/3` as a stacked fraction, `sin(pi/3)` as `√3/2` (special
  angles, in both RADIAN and DEGREE). `Alt+ENTER` or a trailing `>dec` gives
  the decimal.
- **Scientific constants and unit conversions** on the home screen.
- **Persistence** — history, variables, graph state, lists, matrices and
  display settings all survive a power cycle, stored on the SD card with
  automatic migration between firmware format versions.

## Graphing

- **Four graph modes**: function ($Y_1 \ldots Y_7$), **parametric**
  ($X_{nT}(t), Y_{nT}(t)$), **polar** ($r_n(\theta)$, angle-mode aware), and
  **sequence**. Selected from the MODE screen; the Y=, window and table screens
  all adapt to the active mode.
- **Multi-function color plots** with discontinuity handling, axes and grid.
- **Trace** — `LEFT`/`RIGHT` to move along a curve, `UP`/`DOWN` to switch
  curves; the readout shows `x/y`, `t`, or `θ` per mode.
- **Zoom**: in/out, standard, trig, ZBox, ZDecimal, ZSquare, ZoomFit.
- **Shading** — curve and band shading between functions.
- **CALC menu** (TI-84 style): value, zero, min/max, intersect, `dy/dx`, and
  numeric `fnInt`, cursor-driven across function/parametric/polar modes.
- **Value table** for every mode, with auto (infinite scroll) and ask modes,
  plus horizontal column scrolling.
- **Split-screen graph|table** with pane focus and trace↔row sync.

## Data, statistics and inference

- **Six data lists** ($L_1 \ldots L_6$) plus **named lists**, backed by a shared
  `Array` primitive that transparently spills to PSRAM past 256 elements, with
  a spreadsheet-style editor.
- **Descriptive statistics** — 1-var and 2-var.
- **Regression** — all ten TI-style models.
- **Probability distributions** (PDF / CDF / inverse) for normal, $t$,
  $\chi^2$, $F$, binomial, Poisson and geometric.
- **Inference suite** — hypothesis tests, confidence intervals, ANOVA.
- **Stat plots** — histogram, box plot and scatter, overlaid on the graphing
  engine.

## Linear algebra

- **Ten matrix variables** `[A]`–`[J]`, with TI-style bracket typing on the
  home screen or a dedicated editor.
- Arithmetic, determinant, inverse, transpose, row-echelon form, **eigenvalues
  and eigenvectors** (real and complex spectra).
- A **numeric equation solver**.
- **List↔matrix conversion** both ways.

## Complex numbers

- Number modes: REAL, `a+bi`, and polar `r∠θ`, selected from the MODE screen.
  In REAL mode a non-real result says so rather than returning `NaN`.
- Complex-aware arithmetic and elementary functions; complex-valued
  **variables, lists and matrices**, with full complex linear algebra.
- Integer powers of a complex base are computed exactly rather than routed
  through `exp`/`log`, so `(2+3i)^2` is exact.

## Symbolic math (CAS)

`simplify`, `expand`, `factor`, `diff`, `solve` (complex-aware), and a bounded
form of symbolic `integ`. Reachable inline from the home screen as ordinary
function calls, or via the `F6` CAS menu / typed `cas` command.

The integrator's limit is differential algebra, not the hardware —
[docs/references/risch-algorithm.md](docs/references/risch-algorithm.md)
explains where it stops and why.

## Apps and scripting

- **App launcher** — `F6` on the home screen, or the `apps` command. `ESC`
  from an app returns to the launcher; `HOME` goes straight back to the
  calculator from anywhere.
- **Notepad** — plain-text notes under `/picocalc/notes/`, on a shared
  line-numbered text editor.
- **File browser** — directory navigation plus rename, delete, and cut/move
  between folders.
- **MicroPython, embedded and running on the device** — pinned to upstream
  v1.28.0. Write a script on the calculator, press RUN, read its output, save
  it, power-cycle, and reload it. `ESC` stops a runaway loop; two unread
  presses kill a script that has stopped responding.
- **The `calc` module** — 54 functions that give a script the calculator
  itself: evaluate expressions through the same pipeline the home screen uses,
  read and write the calculator's variables, call the CAS, plot and analyse
  graphs, reach the six lists and ten matrices, draw on the screen, read keys,
  and read and write SD files. See
  [guide chapter 16](docs-site/guide/16-micropython.md).
- **Apps on the SD card** — a folder under `/picocalc/apps/` with a manifest
  becomes its own launcher tile, so an app can be installed by copying a
  directory.

## Device and platform

- **Two boards, one source tree** — Pico 1 H (RP2040, Cortex-M0+, softfloat)
  and Pico 2 H (RP2350, Cortex-M33, hardware FPU) both build from the same
  code, with the hardware differences confined to `src/platform/`.
- **Built-in help browser** (type `help` or `?`) — the function catalog is driven
  by the same table the parser registers from, so it cannot drift from what the
  firmware actually accepts.
- **Swappable 8x16 fonts** at build time
  (`-DPICOCALC_FONT=spleen|juliamono|iosevka|unifont|terminus`), all carrying
  the same math-glyph slot map (`π θ σ Σ μ λ ≠ √ ∠ ⇒ …`).
- **Status bar** with angle/display mode and battery level (cached STM32 read,
  refreshed every 30 s, cyan while charging; `--` on units whose keyboard
  firmware predates the battery register).
- **Auto power-down** and persisted brightness.
- **Reboot to bootloader** from the MODE screen, so flashing does not require
  reaching for the BOOTSEL button.
- **Serial line injection** — a host script can submit expressions over USB and
  read back the result and its type, which is what makes on-device regression
  runs repeatable rather than hand-driven. See
  [CONTRIBUTING.md](CONTRIBUTING.md).

## Design language

The TI-83/84 is the reference for behaviour and key layout, but the UI is
modernized for the PicoCalc's $320\times320$ color display rather than
reproducing a $96\times64$ monochrome screen. Where this project deliberately
departs from TI, [docs/notes/ti-parity.md](docs/notes/ti-parity.md) records
the gap and the reason.
