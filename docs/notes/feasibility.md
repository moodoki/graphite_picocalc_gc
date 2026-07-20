# PicoCalc Graphing Calculator: Feasibility & Starting Points

**Scope**: Native C++ graphing calculator software for the PicoCalc, targeting both Pico 1 H and Pico 2 H modules. TI-83/84-inspired but modernized UI. Full graphing, statistics, programmability, and app-loading capability. No emulation.

---

## 1. Hardware summary and constraints

The PicoCalc's ClockworkPi v2.0 mainboard augments whichever Pico module is installed with shared peripherals that remain constant across both targets.

### Shared mainboard resources

| Component | Detail |
|-----------|--------|
| Display | 4" IPS LCD, $320\times320$ px, ST7365P controller (ILI9488-compatible), SPI |
| Keyboard | 67-key QWERTY, managed by STM32 co-processor over I2C |
| External PSRAM | 8 MB, SPI-attached |
| SD card | Full-size slot, 32 GB FAT32 card included |
| Audio | Piezo buzzer (PWM-driven) |
| Battery | 18650 Li-ion cell + charging circuit |

### Per-module differences

| | Pico 1 H (RP2040) | Pico 2 H (RP2350) |
|---|---|---|
| CPU | 2x Cortex-M0+ @ 133 MHz | 2x Cortex-M33 @ 150 MHz |
| SRAM | 264 KB | 520 KB |
| Flash | 2 MB | 4 MB |
| Hardware FPU | **None** (software float via ROM routines) | **Single-precision** |
| DSP instructions | None | Yes (SIMD, saturating math) |

The two constraints that dominate every architectural decision are:

1. **SRAM pressure**: a single $320\times320$ RGB565 framebuffer is ~200 KB, which is 76% of Pico 1's total SRAM. The 8 MB PSRAM (accessed via SPI at ~30–40 Mbit/s) is the escape valve — it is slow for random access (~30x slower writes than SRAM) but perfectly adequate for framebuffer storage, since pixel data flows unidirectionally to the display controller.
2. **FPU absence on Pico 1**: the RP2040's bootrom ships hand-optimized softfloat routines (QFPLIB-derived, using hardware interpolators) that achieve roughly 1.5 MFLOPS at 125 MHz. Trig functions take ~200–400 cycles each, yielding around 400K evaluations/second/core. This is sufficient for interactive graphing (plotting 320 points of $\sin(x)$ takes <1 ms) but becomes a bottleneck for heavy numerical work (large matrix operations, iterative solvers). On Pico 2, the hardware FPU eliminates this concern.

### Dual-target strategy: compile-time abstraction

Because both modules share the same mainboard pinout, peripheral drivers (display, keyboard, SD, PSRAM) are identical. The differences are handled at compile time:

```
// Build system defines:
//   PICOCALC_PICO1  → RP2040, Cortex-M0+, 264KB, no FPU
//   PICOCALC_PICO2  → RP2350, Cortex-M33, 520KB, FPU

#ifdef PICOCALC_PICO2
  // Use hardware FPU, larger framebuffer in SRAM, wider data paths
#else
  // Use ROM softfloat, framebuffer in PSRAM, conservative memory layout
#endif
```

The Pico SDK's CMake build system already supports both `pico` and `pico2` board targets, so a single codebase compiles for either with a board flag.

---

## 2. Existing projects: what's already proven

### 2.1 Coyote OS — calculator firmware already running on PicoCalc

**Language**: C | **License**: not yet specified | **Status**: active (Jan 2026)

Coyote OS by laingcc is a dedicated calculator firmware for the PicoCalc. It provides multiple calculation buffers with expression history, a **function graphing mode** that plots $f(x)$ across the display, and a built-in text editor. It uses the **TinyExpr** library (single-file C99, zlib license) for expression parsing and evaluation.

**What it proves**: expression evaluation and real-time function graphing work on both Pico 1 and Pico 2 with the PicoCalc display/keyboard stack. The project has working drivers for the ST7365P LCD, I2C keyboard, and SD card.

**What it lacks**: statistics functions, table view, matrix operations, programmability, app loading, natural math rendering (pretty-print), and any kind of plugin/extension system.

**Relevance to our project**: high. Even though Coyote OS is C rather than C++, its **peripheral drivers and display routines** are directly reusable (C is valid C++). Its TinyExpr integration demonstrates the expression-evaluation pipeline. The project's architecture is simple and flat, however — there is no UI framework, no screen/view abstraction, and no separation between math engine and presentation. Extending it to TI-83/84 scope would mean rewriting most of the application layer while keeping the HAL.

### 2.2 Delta Pico — graphing calculator on RP2040 with natural math input

**Language**: Rust | **License**: MIT | **Status**: complete (2022), not actively maintained

Delta Pico by Aaron Christiansen is a standalone RP2040 graphing calculator (custom PCB, not PicoCalc) with a $240\times320$ color display. Key features include:

- **Natural math input**: textbook-style rendering of fractions, exponents, roots, and parentheses via a custom engine called `rbop` (Rust-Based Operator Precedence).
- **Multi-graph plotting**: multiple $f(x)$ functions plotted simultaneously with configurable axis bounds, trace cursor, and zoom.
- **Hardware abstraction layer (HAL)**: explicit trait-based abstraction separating application logic from display/input hardware.

**What it proves**: a complete graphing calculator experience (input → parse → evaluate → render → graph) runs within RP2040's constraints. The author's detailed writeup documents hard-won lessons about softfloat performance, display DMA strategies, and memory management on Cortex-M0+.

**Relevance to our project**: the **architecture and design decisions** are directly informative even though the language doesn't match (Rust → C++ port is non-trivial). The HAL pattern, the approach to natural math rendering, and the graphing engine design are all transferable as design references. The `rbop` expression tree concept maps cleanly to a C++ equivalent using `std::variant` or a tagged-union AST.

### 2.3 DB48X — feature-rich calculator firmware in C++ (RPL/HP-48 style)

**Language**: C++ | **License**: GPL v3 | **Status**: very active (2024–present, 248 GitHub stars)

DB48X by Christophe de Dinechin is a ground-up reimplementation of the HP-48/50 RPL calculator for the SwissMicros DM42 hardware (STM32L476, Cortex-M4, 128 KB RAM, 1 MB flash). It provides:

- **Variable-precision decimal arithmetic** (up to 34+ digits)
- **Symbolic computation**, equation solving, integration
- **Function plotting**
- **Engineering units** library (with dimensional analysis)
- **Equations library** with hundreds of built-in formulas
- **Full programmability** in RPL

It was designed for extreme memory constraints — the FOSDEM 2024 talk was titled "How much math can you fit in 700K?" The entire firmware, including all math functions, plotting, and the RPL interpreter, fits in under 700 KB of flash.

**What it proves**: a full-featured programmable graphing calculator can be implemented in C++ within flash and RAM constraints tighter than PicoCalc's. The code compiles with GCC for ARM Cortex-M.

**Relevance to our project**: this is the closest language and architecture match (C++ on ARM Cortex-M with a HAL layer), but the UX paradigm is fundamentally different — RPL stack-based entry vs. TI-83's algebraic entry. Adapting DB48X to a TI-like interface would mean replacing the entire input/UI layer while potentially reusing the math engine, plotting system, and equation library. The GPL v3 license is also a consideration — any derivative work must also be GPL v3.

### 2.4 The PicoCalc community and ecosystem

The official `clockworkpi/PicoCalc` GitHub repo (594+ stars) provides reference firmware and drivers. The community has produced:

- **uf2loader** (by pelrun): SD card bootloader that lets users switch between multiple firmwares without a PC. Critical for an app-loading workflow.
- **awesome-pico-calc** (by jblanked): curated list of PicoCalc projects.
- Emulators: NES, ZX Spectrum, Game Boy, Chip-8 — proving real-time rendering and dual-core patterns.
- Operating systems: FUZIX (Unix-like), MicroPython, CircuitPython.
- Active forum (forum.clockworkpi.com) and Discord.

---

## 3. Key open-source libraries for the math engine

Since we are building natively (not emulating), we need expression parsing, evaluation, graphing, and statistics as library components. All of the following are C or C++ and suitable for embedded use.

### Expression parsing and evaluation

| Library | Language | License | Size | Notes |
|---------|----------|---------|------|-------|
| **TinyExpr** | C99 | zlib | 1 file (~600 LOC) | Proven on PicoCalc (Coyote OS). Supports custom functions/variables. No symbolic math. |
| **tinyexpr++** | C++11 | zlib | Fork of TinyExpr with lambdas, `std::function` | Drop-in upgrade, better C++ integration. |
| **muParser** | C++ | MIT | ~15 files | Faster than TinyExpr for repeated evaluation (bytecode compilation). Supports user-defined operators. |
| **ExprTk** | C++ | MIT | 1 header (~40K LOC) | Extremely fast (JIT-like compilation). Very large — may strain Pico 1 flash. |

**Recommendation**: start with **tinyexpr++** for rapid prototyping. If evaluation speed becomes a bottleneck (unlikely for single-expression graphing, possible for table generation with hundreds of rows), graduate to **muParser**'s bytecode compilation.

### Statistics

No single lightweight C++ statistics library dominates the embedded space. The TI-83/84 statistics feature set is well-bounded:

- 1-Var and 2-Var stats (mean, median, std dev, quartiles, min/max)
- Regression models: linear, quadratic, cubic, quartic, exponential, logarithmic, power, logistic, sinusoidal
- Probability distributions: normal (PDF, CDF, inverse), $t$, $\chi^2$, $F$, binomial, Poisson, geometric
- Statistical plots: histogram, box-and-whisker, scatter with regression overlay

This is approximately 30–50 functions. Implementing from scratch using standard formulas is likely **less effort** (2–3 weeks) than finding, vetting, and adapting an existing library. Standard references (Numerical Recipes, NIST Digital Library of Mathematical Functions) provide well-tested algorithms.

For **distribution functions** specifically, the `cephes` library (public domain, C) provides high-accuracy implementations of all standard distributions, and its individual function files can be cherry-picked without pulling in the entire library.

### Matrix operations

The TI-83/84 supports matrices up to $99\times99$ with operations: arithmetic, determinant, inverse, transpose, row echelon form, eigenvalues (limited). A minimal dense matrix class in C++ with Gaussian elimination covers 90% of use cases. For eigenvalues, a QR algorithm implementation adds ~500 LOC.

Memory constraint: a $99\times99$ `float` matrix is ~38 KB. On Pico 1 this is feasible only one at a time (with framebuffer in PSRAM); on Pico 2, two or three can coexist. Storing matrices on SD card and loading on demand is the practical solution for the full TI-compatible "10 matrix variables" model.

---

## 4. Architecture proposal

The following layered architecture supports both Pico modules, separates concerns cleanly, and allows incremental development.

```
┌─────────────────────────────────────────┐
│            App Framework                │  ← Loadable apps from SD card
│  (Calculator, Graph, Table, Stats,      │
│   Matrix, Program Editor, Settings)     │
├─────────────────────────────────────────┤
│            UI Framework                 │  ← Screen manager, widgets, themes
│  (Views, menus, softkeys, dialogs,      │
│   text rendering, natural math display) │
├─────────────────────────────────────────┤
│            Math Engine                  │  ← Expression parser, evaluator,
│  (tinyexpr++, stats, matrix, solver)    │     statistics, regression, matrices
├─────────────────────────────────────────┤
│         Platform Abstraction Layer      │  ← Display, keyboard, SD, PSRAM,
│  (HAL for ST7365P, STM32 kbd, SPI,     │     timers, audio, power management
│   PSRAM, SD/FAT32, dual-core dispatch)  │
├─────────────────────────────────────────┤
│         Pico SDK  (C)                   │  ← Hardware registers, PIO, DMA,
│         + Board Support Package         │     flash XIP, multicore, USB
└─────────────────────────────────────────┘
```

### 4.1 Platform Abstraction Layer (PAL)

**Purpose**: isolate all hardware access behind C++ interfaces so the upper layers never touch raw registers.

Key abstractions:

- `Display`: `init()`, `setPixel()`, `fillRect()`, `drawLine()`, `blit()`, `flush()`. Internally uses PIO+DMA on core 1 for SPI transfer. On Pico 1, framebuffer lives in PSRAM; on Pico 2, it can live in SRAM.
- `Keyboard`: `poll() → KeyEvent`. The STM32 co-processor handles scanning; the PAL reads key state over I2C and translates to logical key codes. Must handle key repeat, modifier combos (2nd, Alpha, Shift).
- `Storage`: FAT32 on SD card via `ff.h` (FatFs, already used by PicoCalc community). Provides file I/O for programs, app bundles, variable save/restore, and settings.
- `Memory`: PSRAM allocator for large buffers (framebuffer, datasets, matrix storage). Exposes `psram_alloc()` / `psram_free()` with a simple arena or pool allocator.
- `System`: clock speed control (overclock to 200–250 MHz on Pico 1 for compute bursts), sleep/wake, battery level, USB connectivity.

### 4.2 UI Framework

**Purpose**: provide a TI-inspired but modernized screen system with clean navigation.

Core concepts:

- **Screen stack**: push/pop model (Home → Graph → Trace, or Home → Stats → Edit). Each screen owns its rendering and input handling.
- **Widget set**: text labels, input fields (with cursor), menus (horizontal softkey bar at bottom like TI-84, vertical context menus), scrollable lists, toggle switches, number spinners.
- **Natural math renderer**: renders expression trees as 2D typeset math (fractions with horizontal bars, superscript exponents, radical symbols, absolute value bars). This is the single most complex UI component. Delta Pico's `rbop` engine is an excellent design reference — it uses a tree of "layout nodes" where each node knows its bounding box and renders recursively.
- **Theme / style system**: a small set of compile-time constants controlling colors, fonts, spacing. Allows a "TI Classic" dark-on-light theme and a "Modern" theme.
- **Font rendering**: bitmap fonts (no TTF — too heavy for Pico 1). A $8\times16$ monospace font for the home screen and program editor; a proportional font for menus and labels. Font data stored in flash as `const` arrays.

The TI-83/84's display is $96\times64$ monochrome. PicoCalc's $320\times320$ color display offers dramatically more space. The "modernized" approach should use this space for:

- Larger, clearer graph viewport
- Status bar (battery, mode indicators, 2nd/Alpha state)
- Softkey labels along the bottom (context-sensitive, like TI-84's function keys)
- Optional split-screen (e.g., graph + table side by side)

### 4.3 Math Engine

**Purpose**: parse, evaluate, and manipulate mathematical expressions.

Components:

- **Parser**: tinyexpr++ initially. Parses infix expressions into an AST/evaluation tree. Extended with custom functions (statistical distributions, matrix indexing).
- **Evaluator**: walks the tree, returns `double` (or `float` on Pico 1 for speed-critical paths like graphing). Supports variable bindings ($X$, $Y$, list variables $L_1 \ldots L_6$, matrix variables $[A] \ldots [J]$).
- **Graphing engine**: iterates $x$ across pixel columns, evaluates $f(x)$, maps to screen coordinates, draws connected line segments. Handles discontinuities (vertical asymptotes) by checking for large $\Delta y$ between adjacent points. Supports:
  - Multiple simultaneous functions ($Y_1 \ldots Y_0$ like TI)
  - Parametric mode: $(X(T), Y(T))$
  - Polar mode: $r(\theta)$
  - Window settings: $X_{\min}$, $X_{\max}$, $Y_{\min}$, $Y_{\max}$, $X_{\text{scl}}$, $Y_{\text{scl}}$
  - Trace cursor, zoom in/out, zoom fit
- **Table engine**: evaluates $f(x)$ for a range of $x$ values, displays in a scrollable two-column table. Trivial to implement once the evaluator exists.
- **Statistics module**: ~30–50 functions, implemented from standard algorithms. Regression uses least-squares fitting (linear algebra for polynomial regression, Levenberg-Marquardt for nonlinear models like logistic/sinusoidal).
- **Equation solver**: numeric root-finding (bisection + Newton's method) for `0 = f(x)`. TI-83's solver is straightforward — no symbolic algebra needed.
- **Matrix module**: dense matrix class with basic operations. Gaussian elimination for inverse/determinant/rref. Stored in PSRAM or SD card for large matrices.

### 4.4 App Framework

**Purpose**: allow first-party "apps" (Stats, Matrix Editor, Finance, Probability) and eventually user-loadable apps from SD card.

Two tiers:

1. **Built-in apps**: compiled into firmware. Each app is a C++ class inheriting from `App` with `onActivate()`, `onDeactivate()`, `onKey()`, `onRender()` methods. The app framework manages a registry and the app selection menu (like TI-84's APPS key).

2. **User programs**: an embedded scripting runtime executes programs stored as text files on the SD card. Two viable options:

   - **MicroPython**: mature, large stdlib, familiar syntax. Interpreter consumes ~60–80 KB RAM on RP2040. The `micropython-embed` mode allows embedding as a C library within a larger firmware. Programs access calculator functions (plot, stat, matrix) through a custom Python module backed by C++ bindings.
   - **Lua** (via MicroLua or a minimal Lua 5.4 port): lighter footprint (~8–20 KB baseline RAM), faster interpreter, easier C/C++ interop via the Lua C API. Less familiar to the TI-BASIC audience but more practical on Pico 1.

   **Recommendation**: Lua for Pico 1 (lower RAM overhead), MicroPython as an optional add-on on Pico 2 (leveraging the extra 256 KB SRAM). Both load `.lua` / `.py` files from SD card, providing a natural "program loading" experience.

3. **Loadable native apps** (stretch goal): `.uf2` or custom binary format loaded from SD card into PSRAM and executed. This is significantly more complex (requires a loader, relocation, and a stable ABI) and is best deferred to a later phase. The existing **uf2loader** bootloader project demonstrates SD-card-based firmware switching, which could serve as a simpler alternative — each "app" is a complete firmware image, and the user reboots to switch.

---

## 5. Memory budget

### Pico 1 H (264 KB SRAM, 2 MB flash, 8 MB PSRAM)

| Allocation | Location | Size |
|-----------|----------|------|
| Framebuffer ($320\times320$ RGB565) | PSRAM | 200 KB |
| Line-buffer DMA (20 scanlines) | SRAM | 12.5 KB |
| Stack (both cores) | SRAM | 16 KB |
| Math engine (parser, evaluator, state) | SRAM | 20–30 KB |
| UI framework (screen stack, widgets) | SRAM | 15–25 KB |
| Keyboard + I2C buffers | SRAM | 2 KB |
| Statistics data (lists $L_1$–$L_6$, up to 999 entries each) | PSRAM | ~24 KB per list |
| Matrix storage (up to 10 matrices) | PSRAM / SD | variable |
| Lua interpreter | SRAM | 8–20 KB |
| Lua heap (programs, user data) | PSRAM | up to 4 MB |
| **Total SRAM usage** | | **~100–120 KB** |
| **SRAM headroom** | | **~140–160 KB free** |
| **Flash usage** (firmware + fonts + constants) | | **~500 KB – 1.2 MB** |

This is tight but workable. The key insight is that the framebuffer never needs to be in SRAM — line-buffer rendering (processing 10–20 scanlines at a time via DMA to the display) keeps SRAM usage minimal while still achieving 30–50 fps. The PSRAM handles bulk storage (framebuffer, datasets, script heap).

### Pico 2 H (520 KB SRAM, 4 MB flash, 8 MB PSRAM)

Everything is more comfortable. The framebuffer can optionally move to SRAM (200 KB) with 320 KB remaining — enough for the math engine, UI, a MicroPython interpreter, and generous heap space. Flash doubles to 4 MB, easily accommodating a richer set of built-in apps and a MicroPython runtime alongside Lua.

---

## 6. Starting point comparison (emulation excluded)

| Factor | Extend Coyote OS | Port Delta Pico concepts | Adapt DB48X engine | Build from scratch |
|--------|-----------------|-------------------------|-------------------|-------------------|
| **Language match** | C (usable from C++) | Rust → C++ rewrite | C++ (direct match) | C++ |
| **PicoCalc drivers** | Already done | Must write | Must write | Must write (or borrow from Coyote OS) |
| **Graphing** | Basic (single $f(x)$) | Full (multi-function, trace, zoom) | Full (with symbolic) | Must implement |
| **Statistics** | None | None | Partial (basic stats) | Must implement |
| **Programmability** | None | None | Full (RPL) | Must implement |
| **Natural math display** | None | Full (`rbop` engine) | Full (EQW renderer) | Must implement |
| **UX paradigm** | Minimal/custom | Algebraic (TI-like) | RPN/RPL (HP-like) | TI-like (our choice) |
| **License** | Unknown | MIT | GPL v3 | Own |
| **Effort to MVP** | 6–10 weeks | 8–12 weeks | 10–16 weeks | 12–20 weeks |
| **Effort to full feature parity** | 4–6 months | 5–7 months | 6–9 months | 6–9 months |
| **Risk** | Low (proven base) | Medium (language rewrite) | Medium-high (paradigm mismatch) | Medium (scope creep) |

---

## 7. Recommended approach: hybrid bootstrap

The highest-leverage path combines the best elements of existing work rather than committing entirely to one starting point.

### Phase 0 — Bootstrap (weeks 1–2)

Fork the PicoCalc reference firmware or Coyote OS's HAL layer. Establish a C++ project structure with:

- CMake build supporting both `pico` and `pico2` targets
- Platform Abstraction Layer wrapping display (ST7365P), keyboard (STM32 I2C), SD card (FatFs), and PSRAM
- Line-buffer rendering pipeline on core 1 (PIO + DMA)
- Basic screen manager (push/pop)

**Deliverable**: blank screen with keyboard input working, text rendering, and a blinking cursor.

### Phase 1 — Calculator core (weeks 3–6)

- Integrate **tinyexpr++** as the expression engine
- Build the **Home Screen**: expression input line, scrollable history, result display
- Implement **natural math renderer** (study Delta Pico's `rbop` design, reimplement in C++ with a layout-node tree)
- Add variable storage ($A$–$Z$, $\theta$, $Ans$) persisted to SD card
- Basic **function graphing**: single $Y=$ entry, plot on keypress, axis labels

**Deliverable**: a usable scientific calculator with basic graphing. Comparable to Coyote OS but with better UI and natural math display.

### Phase 2 — Graphing & table (weeks 7–10)

- Multi-function graphing ($Y_1$ through $Y_0$, 10 slots)
- Window settings screen ($X_{\min}$, $X_{\max}$, etc.)
- Trace cursor, zoom (box zoom, zoom in/out/fit/standard/trig)
- Parametric and polar graphing modes
- Table view (auto and ask modes)
- Graph ↔ table split screen

**Deliverable**: graphing feature parity with TI-83/84.

### Phase 3 — Statistics & data (weeks 11–15)

- List editor (like TI's STAT → Edit)
- 1-Var and 2-Var statistics
- Regression models (all 10 TI types)
- Statistical plots (histogram, box plot, scatter + regression)
- Probability distributions (normalcdf, invNorm, binompdf, etc.)
- List operations (sort, seq, cumSum, etc.)

**Deliverable**: statistics feature parity with TI-83/84.

### Phase 4 — Matrix & programming (weeks 16–20)

- Matrix editor and operations (det, inverse, rref, transpose, arithmetic)
- Equation solver (numeric)
- Embed **Lua** interpreter with calculator API bindings
- Program editor (on-device text editor with syntax highlighting)
- Program execution from SD card (`.lua` files)
- On Pico 2: optionally embed **MicroPython** as an alternative runtime

**Deliverable**: matrix operations and user programmability.

### Phase 5 — App framework & polish (weeks 21–26)

- App selection menu (APPS key equivalent)
- Built-in apps: Finance (TVM solver), Probability Simulator, Polynomial Root Finder, Unit Converter
- App manifest format for SD-card-loaded apps (metadata + Lua/Python entry point)
- Settings screen (display brightness, contrast, key repeat rate, theme)
- Power management (sleep on idle, battery indicator)
- Comprehensive testing, edge cases, UX polish

**Deliverable**: release-quality firmware.

---

## 8. Key technical risks and mitigations

### Risk 1: PSRAM latency for framebuffer

**Problem**: SPI PSRAM is ~30x slower than SRAM for random-access writes. A full-framebuffer approach with frequent random pixel updates could bottleneck rendering.

**Mitigation**: use **line-buffer rendering** — render 10–20 scanlines into a small SRAM buffer, DMA them to the display, then render the next batch. This avoids a full framebuffer entirely. Core 1 handles the DMA pipeline while core 0 computes the next batch. Proven at 30–50 fps in multiple PicoCalc projects. Fall back to PSRAM framebuffer only if complex overlapping UI elements require random-access compositing.

### Risk 2: Softfloat performance on Pico 1 for heavy computation

**Problem**: operations like matrix inversion, nonlinear regression, and distribution CDF evaluation are compute-heavy. A $20\times20$ matrix inverse involves ~$O(n^3)$ floating-point operations — ~24,000 multiply-adds — which at softfloat speeds could take ~50–100 ms on Pico 1.

**Mitigation**: 50–100 ms is acceptable (TI-83 itself took similar or longer times for these operations at 6 MHz). For truly expensive computations (large matrices, iterative solvers), use `#ifdef PICOCALC_PICO2` to enable optimized paths leveraging the hardware FPU, and accept slower performance on Pico 1 as a known tradeoff. Consider fixed-point arithmetic for graphing specifically (where $\pm10$ bits of precision suffice for pixel mapping).

### Risk 3: Flash space on Pico 1 (2 MB)

**Problem**: firmware + fonts + Lua runtime + built-in app data could approach 2 MB.

**Mitigation**: the Pico SDK's XIP (execute-in-place) means code runs directly from flash without copying to SRAM — flash is effectively "free" ROM. At 500 KB – 1.2 MB for the full firmware (based on DB48X fitting in 700 KB and Coyote OS being much smaller), 2 MB is sufficient. If it gets tight, Lua scripts and large data tables live on SD card, not in flash. On Pico 2, the 4 MB flash provides comfortable headroom.

### Risk 4: Natural math rendering complexity

**Problem**: building a correct, visually appealing 2D math renderer (fractions, nested exponents, radicals, summations) is a substantial engineering effort with many edge cases.

**Mitigation**: start minimal — fractions and exponents cover 80% of TI-83 use cases. Study Delta Pico's `rbop` and DB48X's equation writer for proven layout algorithms. Use a recursive layout model: each AST node computes its bounding box from its children, then renders relative to a parent-provided origin. Add more constructs (radicals, absolute value, integrals) incrementally in later phases.

### Risk 5: Scope creep

**Problem**: "TI-83/84-like" encompasses a vast feature surface that has been refined over 30 years.

**Mitigation**: define a strict MVP feature set (Phase 1–3: calculator + graphing + statistics) and resist adding features until the core is solid. Use the TI-83 (not TI-84 CE) as the baseline — it has fewer features but defines the essential workflow. "Modernized" UI improvements (color, higher resolution, split screen) add value without expanding the feature footprint.

---

## 9. Summary of effort estimates

| Milestone | Solo developer | 2-person team |
|-----------|---------------|---------------|
| Bootstrap + HAL | 2 weeks | 1 week |
| Scientific calculator + basic graph | 4 weeks | 2–3 weeks |
| Full graphing (multi-function, parametric, polar, trace) | 4 weeks | 2–3 weeks |
| Statistics (all TI-83 features) | 4–5 weeks | 3 weeks |
| Matrix + programming (Lua) | 4–5 weeks | 3 weeks |
| App framework + polish | 5–6 weeks | 3–4 weeks |
| **Total to release quality** | **~24–28 weeks** | **~14–17 weeks** |

These estimates assume an experienced C++ embedded developer working part-time (~20 hrs/week). Full-time cuts roughly in half.

---

## 10. References

1. ClockworkPi PicoCalc product page — https://www.clockworkpi.com/picocalc
2. ClockworkPi PicoCalc GitHub repo — https://github.com/clockworkpi/PicoCalc
3. Coyote OS forum thread — https://forum.clockworkpi.com/t/coyote-os-calculator-firmware-for-picocalc/21130
4. Delta Pico GitHub — https://github.com/AaronC81/delta-pico
5. Delta Pico writeup — https://aaronc.cc/2022/10/23/delta-pico.html
6. DB48X GitHub — https://github.com/c3d/db48x
7. DB48X homepage — https://48calc.org/
8. TinyExpr GitHub — https://github.com/codeplea/tinyexpr
9. Free42 (HP-42S simulator) — https://thomasokken.com/free42/
10. uf2loader (SD card bootloader) — https://github.com/pelrun/uf2loader
11. awesome-pico-calc — https://github.com/jblanked/awesome-pico-calc
12. MicroLua (Lua for RP2040) — https://github.com/MicroLua/MicroLua
13. RP2040 SPI PSRAM discussion — https://forums.raspberrypi.com/viewtopic.php?t=316012
14. Pico SDK floating point documentation — https://www.raspberrypi.com/documentation/pico-sdk/runtime.html
15. TI-83/84 hardware history (WikiTI) — https://wikiti.brandonw.net/index.php?title=83Plus:History_of_TI-8x_hardware
