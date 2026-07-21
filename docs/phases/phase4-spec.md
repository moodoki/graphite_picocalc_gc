# Phase 4 Spec: Matrix, Graph Analysis, Complex Numbers & GC Completeness

**Prerequisite phases**: Phase 1 (HAL + calculator + graphing), Phase 2 (table view, parametric/polar, split-screen), Phase 3 (statistics, data lists, the shared `Array` primitive).

**Scope**: Add matrix operations, interactive graph analysis (the TI-84 CALC menu), a complex-number subsystem, and a closing pass that brings the remaining TI-83/84+ parity gaps up to par. **Phase 4 is the pre-release milestone**: when it's done, the calculator is feature-complete as a graphing calculator — everything past this point (CAS, MicroPython, other non-calculator apps) is additive, not required to call the core product finished.

**End state**: the calculator can manipulate matrices; analyze graphs interactively (roots, extrema, intersections, numeric derivative and integral); compute with complex numbers throughout; and stands on its own as a complete TI-83/84+-class graphing calculator (sequence graphing, full zoom/shading set, scientific constants, unit conversions, list↔matrix interop, device power/settings polish — see sub-phase 4D).

> **2026-07-21 update (phase restructure)**: the CAS engine (originally
> sub-phase 4D below) has been split out into its own **Phase 5** — see
> [phase5-spec.md](phase5-spec.md) and [decisions.md](../notes/decisions.md)
> D32. **MicroPython (originally sub-phase 4E) has moved to the new
> Phase 6** ("non-calculator functions") as sub-phase 6B — see
> [phase6-spec.md](phase6-spec.md) and D33. The `4D` label, vacated by
> CAS, is reused here for a new closing sub-phase — **GC completeness** —
> that rounds out Phase 4 into the "full graphing calculator" pre-release
> milestone the project is now organized around: Phase 4 = complete GC,
> Phase 5 = CAS, Phase 6 = non-calculator apps/extensibility, in that
> order, with 6's sub-phases (unlike 4/5/6 themselves) designed to be
> completable in any order once Phase 6 starts.

---

## 1. Overview and phasing within Phase 4

Phase 4 splits into sub-phases, developed in order. Each occupies a different part of the codebase, but there are deliberate dependencies: graph analysis reuses 4A's numeric solver, 4C's complex subsystem is a prerequisite for both CAS (Phase 5) and parts of 4D, and 4D itself reuses matrix/complex/graphing machinery from 4A–4C to close out remaining gaps.

| Sub-phase | Weeks | Content |
|-----------|-------|---------|
| 4A: Matrix operations | 26–27 | Matrix editor, arithmetic, det, inverse, rref, eigenvalues; numeric solver |
| 4B: Graph analysis (CALC) | 28–29 | value, zero, min/max, intersect, dy/dx, fnInt — numeric + interactive on the graph screen |
| 4C: Complex numbers | 30–31 | `Complex` type, complex-aware arithmetic and functions, a+bi mode, rect/polar display |
| 4D: GC completeness | 32–35 | Sequence graphing, zoom/shading, list↔matrix bridge, sci constants, unit conversions, home-screen matrix literals, complex storage, device polish — closes remaining TI-83/84+ parity gaps |

*(4A–4C already shipped under these labels — see D28–D30 — so their numbering is unchanged. `4D` is a new definition, reusing the label CAS vacated when it became Phase 5; the old 4E, MicroPython, is gone from this document entirely — see phase6-spec.md §2.)*

**Total estimated effort (this document)**: ~46 + 51 + 38 + [4D estimate, see §8] hours across 4A–4D.

The ordering matters:

- **4B (graph analysis) after 4A** because zero/intersect reuse 4A's numeric root-finder, and the CALC operations are numeric in this phase.
- **4C (complex)** precedes 4D because two of 4D's items (home-screen matrix literals interacting with complex scalars, complex-valued variable/Ans storage) build directly on the `Complex` type and number-mode subsystem. 4C was also completed before CAS (Phase 5) work could start, since CAS equation solving is complex-aware — that dependency is satisfied and Phase 5 can proceed independently of this document from here.
- **4D (GC completeness) last** because it's explicitly a closing/mop-up pass — it pulls together loose ends across graphing, matrices, complex numbers, and stats/lists that only make sense to scope once 4A–4C exist to extend.

---

## 2. New source files

Additions to the project tree from Phases 1–3:

```
src/
├── math/
│   ├── engine.hpp / .cpp          # Extended: complex eval path, symbolic mode
│   ├── array.hpp / .cpp           # From Phase 3 — matrices build on this
│   ├── matrix.hpp / .cpp          # NEW: linear-algebra view over Array (2-D)
│   ├── numeric_solve.hpp / .cpp   # NEW: root-finder, extrema, numeric integ/deriv
│   ├── complex.hpp / .cpp         # NEW: Complex type + complex-aware functions
│   ├── seq_expr.hpp / .cpp        # NEW (4D): sequence-mode (u/v/w) evaluator
│   ├── frac.hpp / .cpp            # NEW (4D): decimal→fraction conversion (▶Frac)
│   ├── constants.hpp / .cpp       # NEW (4D): scientific constants catalog
│   ├── units.hpp / .cpp           # NEW (4D): unit-conversion catalog + conversion
│   └── types.hpp                  # Updated: complex type, matrix view alias
│   # expr_tree.hpp/.cpp and cas/ (simplify, derivative, integrate, factor,
│   # solve, expand, rules) moved to Phase 5 — see phase5-spec.md.
├── graph/
│   ├── analysis.hpp / .cpp        # NEW (4B): CALC operations engine
│   ├── analysis_cursor.hpp / .cpp # NEW (4B): interactive bound/guess cursor
│   └── seq_points.hpp / .cpp      # NEW (4D): sequence-mode point source
├── apps/
│   ├── matrix_editor.hpp / .cpp   # NEW (4A): matrix editor screen
│   ├── solver_screen.hpp / .cpp   # NEW (4A): numeric & symbolic solver UI
│   ├── calc_menu.hpp / .cpp       # NEW (4B): graph analysis menu + interaction
│   ├── seq_editor.hpp / .cpp      # NEW (4D): sequence-mode editor screen
│   └── constants_screen.hpp / .cpp # NEW (4D): constants/units browse-and-insert menu
│   # cas_screen.hpp/.cpp (CAS worksheet / symbolic mode) moved to Phase 5;
│   # program_screen.hpp/.cpp (MicroPython editor) moved to Phase 6 §2.
├── platform/
│   └── power.hpp / .cpp           # NEW (4D): auto power-off / standby (APD)
```

---

## 3. Sub-phase 4A: Matrix operations (weeks 26–27)

> **Reconciliation with Phase 3**: Phase 3 introduced the `Array` primitive (`math/array.hpp`) as the shared n-dimensional backing store for lists and matrices. Phase 4's matrix support is therefore a **linear-algebra view over a 2-D `Array`**, not a separate storage class. The `Matrix` type below wraps an `Array` and adds linear-algebra methods; storage, allocation (SRAM pool vs PSRAM), and element access come from `Array`. This supersedes the standalone `Matrix` design in earlier drafts of this spec.

### 3.1 Matrix as a view over Array

```cpp
namespace math {

// Linear-algebra view over a 2-D Array. Owns or references an Array;
// storage/allocation is handled by Array (SRAM pool or PSRAM per size).
class Matrix {
public:
    Matrix() = default;
    explicit Matrix(Array&& backing);         // Take ownership of a 2-D Array
    Matrix(int rows, int cols);               // Allocate a fresh zero matrix

    int rows() const { return arr_.dim(0); }
    int cols() const { return arr_.dim(1); }

    calc_t& at(int r, int c)       { return arr_.at(r, c); }
    calc_t  at(int r, int c) const { return arr_.at(r, c); }
    calc_t& operator()(int r, int c) { return at(r, c); }

    Array&       array()       { return arr_; }
    const Array& array() const { return arr_; }

    // Core operations (return new matrices, originals unchanged)
    Matrix transpose() const;
    Matrix operator+(const Matrix& rhs) const;
    Matrix operator-(const Matrix& rhs) const;
    Matrix operator*(const Matrix& rhs) const;
    Matrix operator*(calc_t scalar) const;

    // In-place row operations (for rref)
    void swap_rows(int r1, int r2);
    void scale_row(int r, calc_t factor);
    void add_scaled_row(int dst, int src, calc_t factor);

    // Decompositions and derived quantities
    calc_t determinant() const;   // LU-based for n>3, direct for 2x2/3x3
    Matrix inverse() const;       // Via Gauss-Jordan
    Matrix rref() const;          // Reduced row echelon form
    Matrix ref() const;           // Row echelon form (no back-sub)
    int    rank() const;          // Via rref

    // Eigenvalues (real only in 4A; complex eigenvalues deferred — see 4C notes).
    // QR algorithm for small matrices, n <= 10.
    Matrix eigenvalues() const;

    static Matrix identity(int n);
    static Matrix zeros(int rows, int cols);

private:
    Array arr_;  // 2-D backing store from Phase 3
};

}  // namespace math
```

**Memory strategy**: unchanged from the Array primitive's model. The calculator exposes 10 matrix variables ($[A]$ through $[J]$) of up to $99\times99$. A $99\times99$ `double` matrix is ~76 KB, living in PSRAM (Array chooses PSRAM above its size threshold). Small matrices (up to ~$16\times16$) stay in the SRAM pool for speed. With 8 MB PSRAM, even ten full-size matrices use under 1 MB.

Matrix variables are stored in a `MatrixStore` parallel to Phase 3's `ListStore`, both consuming `Array`:

```cpp
namespace math {

class MatrixStore {
public:
    Matrix& matrix(int index);          // 0 = [A], ..., 9 = [J]
    const Matrix& matrix(int index) const;
    bool save(platform::Storage& storage) const;   // /picocalc/matrices.dat
    bool load(platform::Storage& storage);
private:
    Matrix matrices_[10];
};

extern MatrixStore g_matrices;

}  // namespace math
```

### 3.2 Matrix editor screen

A spreadsheet-like grid editor:

```
┌──────────────────────────────────┐
│  Matrix [A]  3x3                  │
├──────────────────────────────────┤
│     C1        C2        C3        │
│ R1 [ 1.00 ] [ 2.00 ] [ 3.00 ]   │
│ R2 [ 4.00 ] [ 5.00 ] [ 6.00 ]   │
│ R3 [ 7.00 ] [ 8.00 ] [ 9.00 ]   │
│                                    │
│ Input: _                           │
├──────────────────────────────────┤
│ F1:EDIT F2:NAME F3:DIM F4:OPS     │
└──────────────────────────────────┘
```

**Behavior**:

- Arrow keys navigate the grid, highlighting the active cell.
- Typing enters edit mode for the current cell; `ENTER` confirms and advances (right, then down).
- `F2` (NAME): select which matrix variable $[A]$–$[J]$ to edit.
- `F3` (DIM): change dimensions (prompts for rows and cols; existing data preserved or truncated via `Array::resize`).
- `F4` (OPS): context menu — Transpose, Inverse, Determinant, RREF, Fill, Augment.
- Persistence via `MatrixStore::save` to `/picocalc/matrices.dat`.

### 3.3 Matrix functions from the home screen

Matrices are referenced by name in expressions:

```
det([A])              -> scalar result
inverse([A])          -> matrix result, shown in matrix viewer
rref([A])             -> matrix result
[A] * [B]             -> matrix multiplication
[A] + [B]             -> matrix addition
[A]^-1                -> inverse (alias)
[A]^T                 -> transpose
dim([A])              -> {rows, cols} as a list
[A](2,3)              -> element at row 2, col 3
identity(4)           -> 4x4 identity matrix
augment([A],[B])      -> horizontal concatenation
```

Matrix results too large for one line push the user to the matrix viewer (scrollable, read-only).

### 3.4 Numeric equation solver

The numeric solver added here is reused by 4B's zero/intersect operations.

```cpp
namespace math {

struct SolveResult {
    bool   converged;
    calc_t root;
    int    iterations;
    calc_t residual;     // |f(root)|
};

// Solve f(x) = 0 for x in [a, b] using bisection + Newton refinement.
// Requires f(a), f(b) opposite signs for bracketed solve; falls back
// to Newton from a guess otherwise.
SolveResult numeric_solve(const char* expr, char var,
                          calc_t lower, calc_t upper,
                          calc_t tolerance = 1e-10, int max_iter = 100);

// Solve f(x) = g(x) by solving f(x) - g(x) = 0.
SolveResult numeric_solve_equation(const char* lhs, const char* rhs,
                                   char var, calc_t lower, calc_t upper);

}  // namespace math
```

The solver screen prompts for an equation, the variable, and a guess/bounds; it displays the root, residual, and iteration count.

---
## 4. Sub-phase 4B: Graph analysis / CALC menu (weeks 28–29)

The interactive graph-analysis toolkit — the TI-84 CALC menu — operating on the graph screen across all three graph modes (function, parametric, polar). All operations in this phase are **numeric**; §6 (CAS) later adds symbolic `dy/dx` and `∫` as an alternative path.

### 4.1 Operations

| Operation | Function mode | Parametric mode | Polar mode |
|-----------|---------------|-----------------|------------|
| **value** | $Y_n(x)$ at a chosen $x$ | $(X_{nT}(t), Y_{nT}(t))$ at a chosen $t$ | $r_n(\theta)$ and $(x,y)$ at a chosen $\theta$ |
| **zero** | root of $Y_n(x)$ in a bracket | $t$ where $Y_{nT}(t)=0$ (and/or $X_{nT}=0$) | $\theta$ where $r_n(\theta)=0$ |
| **minimum** | local min of $Y_n(x)$ in a bracket | local min of $Y_{nT}$ w.r.t. $t$ | local min of $r_n$ |
| **maximum** | local max, same bracket approach | local max | local max |
| **intersect** | $Y_a(x) = Y_b(x)$ | curve intersection (numeric) | curve intersection |
| **dy/dx** | numeric derivative at a point | $\frac{dy/dt}{dx/dt}$ at a point | $\frac{dy}{dx}$ via polar derivative formula |
| **fnInt** ($\int$) | $\int_a^b Y_n(x)\,dx$ | $\int_a^b Y_{nT}\,\frac{dX_{nT}}{dt}\,dt$ | $\int \frac{1}{2}r^2\,d\theta$ (area) |

Parametric and polar variants use the appropriate calculus. For parametric `dy/dx`, the slope is $\frac{dy/dt}{dx/dt}$. For polar, $\frac{dy}{dx} = \frac{r'\sin\theta + r\cos\theta}{r'\cos\theta - r\sin\theta}$. The polar `fnInt` computes enclosed area $\frac{1}{2}\int r^2\,d\theta$ (the conventional polar "integral" on TI). These formulas are documented inline in `analysis.cpp`.

### 4.2 Numeric methods

```cpp
namespace graph {

struct AnalysisResult {
    bool   ok;
    double x;          // Independent value at the result (x, t, or theta)
    double y;          // Dependent value (or f-value)
    double aux;        // Secondary value: derivative, integral, r, etc.
    const char* error; // Non-null on failure
};

// value: evaluate the active function at a given independent value.
AnalysisResult analyze_value(const GraphState& gs, int slot, double indep);

// zero: bracketed root find. Reuses math::numeric_solve on the active
// function expression over [lo, hi]. Requires a sign change or a guess.
AnalysisResult analyze_zero(const GraphState& gs, int slot,
                            double lo, double hi);

// minimum / maximum: golden-section search refined by parabolic
// interpolation (Brent's method) within [lo, hi].
AnalysisResult analyze_extremum(const GraphState& gs, int slot,
                                double lo, double hi, bool find_max);

// intersect: solve Y_a - Y_b = 0 near a guess. Reuses
// numeric_solve_equation.
AnalysisResult analyze_intersect(const GraphState& gs, int slot_a,
                                 int slot_b, double lo, double hi);

// dy/dx: central-difference numeric derivative with Richardson
// extrapolation. Step h scales with the x-range for conditioning.
AnalysisResult analyze_derivative(const GraphState& gs, int slot,
                                  double at);

// fnInt: adaptive Gauss-Kronrod (G7-K15) numeric integration over
// [a, b]. Falls back to adaptive Simpson if Kronrod stalls.
AnalysisResult analyze_integral(const GraphState& gs, int slot,
                                double a, double b);

}  // namespace graph
```

**Algorithm choices**:

- **zero / intersect**: reuse 4A's `numeric_solve` / `numeric_solve_equation` (bisection + Newton). The interactive UI supplies the bracket by having the user set left and right bounds on the graph.
- **minimum / maximum**: Brent's method (golden-section + parabolic interpolation) — robust and derivative-free, appropriate for softfloat on Pico 1.
- **dy/dx**: central difference $\frac{f(x+h) - f(x-h)}{2h}$ with Richardson extrapolation for accuracy; $h$ chosen relative to the x-range ($h \approx 10^{-4}(x_{\max}-x_{\min})$) to balance truncation and rounding error.
- **fnInt**: adaptive Gauss-Kronrod (7-point Gauss with 15-point Kronrod error estimate), subdividing intervals where the error estimate exceeds tolerance. This matches the accuracy expectations set by TI's `fnInt`.

### 4.3 Interactive cursor (`graph/analysis_cursor.hpp`)

The defining feature of the CALC menu is interactivity: you pick an operation, then set points/bounds by moving a cursor on the graph.

```cpp
namespace graph {

enum class AnalysisOp {
    VALUE, ZERO, MINIMUM, MAXIMUM, INTERSECT, DERIVATIVE, INTEGRAL
};

// Drives the multi-step interaction for a CALC operation.
// Most operations need 1-3 user-placed points (e.g., zero needs a
// left bound, right bound, and optionally a guess).
class AnalysisCursor {
public:
    void begin(AnalysisOp op, const GraphState& gs, int active_slot);

    // Handle a key. Moving arrows slides the cursor along the curve;
    // ENTER commits the current point as the next required input.
    // Returns true when the operation has all inputs and a result
    // is ready (fetch via result()).
    bool on_key(const platform::KeyEvent& ev);

    // Current cursor position (on the active curve) for rendering.
    double cursor_x() const;
    double cursor_y() const;

    // Prompt text for the current step ("Left Bound?", "Guess?", etc.)
    const char* prompt() const;

    // Whether the operation is complete and result() is valid.
    bool done() const;
    const AnalysisResult& result() const;

private:
    AnalysisOp op_;
    int  active_slot_;
    int  step_;                 // Which input we're collecting
    double collected_[3];       // Up to 3 placed independent values
    int  collected_count_;
    AnalysisResult result_;
};

}  // namespace graph
```

**Interaction flow** (matching TI-84):

- **value**: prompt "X?" (or "T?"/"$\theta$?"); type a value or move the cursor; ENTER shows the point and its coordinates.
- **zero**: "Left Bound?" → move cursor left of the root, ENTER → "Right Bound?" → move right, ENTER → "Guess?" → ENTER → result. A marker (▸/◂) shows the bounds.
- **minimum/maximum**: same three-step bound/guess flow.
- **intersect**: "First curve?" (ENTER to accept the highlighted one) → "Second curve?" → "Guess?" → result.
- **dy/dx**: "X?" → place the point → result shows the numeric slope; a tangent line is drawn.
- **fnInt ($\int$)**: "Lower Limit?" → "Upper Limit?" → result shows the value; the region between the curve and the axis is shaded (reusing the shaded-region primitive from Phase 3's distribution draw).

Selecting the active function/curve when multiple are enabled: `UP`/`DOWN` cycles which $Y_n$ / pair / $r_n$ the cursor rides, before the operation begins.

### 4.4 CALC menu screen (`apps/calc_menu.hpp`)

Invoked from the graph screen (a softkey or key combo). Presents the operation list, then hands control to `AnalysisCursor`:

```
┌──────────────────────────────────┐
│  Analyze                          │
│  ───────────────                  │
│  1: Value                         │
│  2: Zero                          │
│  3: Minimum                       │
│  4: Maximum                       │
│  5: Intersect                     │
│  6: dy/dx                         │
│  7: ∫ f(x) dx                     │
└──────────────────────────────────┘
```

After selection, the menu closes and the graph screen enters analysis mode: the prompt line appears at the top, the cursor rides the curve, and softkeys become `F1:ACCEPT F2:CANCEL`. The result displays in a readout (coordinates, slope, or integral value) and stays on screen until dismissed. Results can be stored to a variable (e.g., the found root → `X`, the integral → `Ans`).

### 4.5 Reuse and layering

Graph analysis is a thin layer over existing machinery:

- Curve evaluation and cursor positioning reuse Phase 2's `PointSource` / `Viewport`.
- zero/intersect reuse 4A's numeric solver.
- Region shading reuses Phase 3's shaded-region primitive.
- The tangent-line draw (dy/dx) is a normal line segment in the graph viewport.

No new math engine capability is required beyond the numeric methods in §4.2.

---

## 5. Sub-phase 4C: Complex numbers (weeks 30–31)

A complex-number subsystem that flows through the numeric evaluator and, in Phase 5, into CAS solving. `sqrt(-1)` yields `i`; expressions evaluate over ℂ when a complex value appears; results display in rectangular ($a + bi$) or polar ($r\angle\theta$) form.

### 5.1 Complex type (`math/complex.hpp`)

```cpp
namespace math {

struct Complex {
    calc_t re = 0.0;
    calc_t im = 0.0;

    Complex() = default;
    Complex(calc_t real) : re(real), im(0.0) {}
    Complex(calc_t real, calc_t imag) : re(real), im(imag) {}

    bool is_real(calc_t eps = 1e-12) const { return fabs(im) < eps; }

    // Arithmetic
    Complex operator+(const Complex& o) const;
    Complex operator-(const Complex& o) const;
    Complex operator*(const Complex& o) const;
    Complex operator/(const Complex& o) const;
    Complex operator-() const;

    // Polar decomposition
    calc_t modulus() const;      // |z| = sqrt(re^2 + im^2)
    calc_t argument() const;     // arg(z) in radians (or degrees per mode)

    static Complex from_polar(calc_t r, calc_t theta);
};

// Complex-aware elementary functions
Complex c_sqrt(const Complex& z);
Complex c_exp(const Complex& z);
Complex c_ln(const Complex& z);      // Principal branch
Complex c_pow(const Complex& base, const Complex& exp);
Complex c_sin(const Complex& z);
Complex c_cos(const Complex& z);
Complex c_tan(const Complex& z);
Complex c_asin(const Complex& z);
Complex c_acos(const Complex& z);
Complex c_atan(const Complex& z);

// Component/utility functions
calc_t  c_abs(const Complex& z);      // modulus
calc_t  c_arg(const Complex& z);      // argument
Complex c_conj(const Complex& z);     // conjugate
calc_t  c_real(const Complex& z);
calc_t  c_imag(const Complex& z);

}  // namespace math
```

### 5.2 Number-mode and the evaluator

The engine gains a **number mode** analogous to the angle mode from Phase 1:

```cpp
namespace math {

enum class NumberMode {
    REAL,          // Complex results from real inputs raise a domain error
                   // (e.g., sqrt(-1) -> "non-real result")
    RECTANGULAR,   // a + bi  (complex allowed, shown rectangular)
    POLAR,         // r∠θ     (complex allowed, shown polar)
};

}  // namespace math
```

The evaluator's numeric path becomes complex-capable. Rather than duplicate the whole engine, the value type used internally during evaluation is `Complex`; real inputs are `Complex` with `im == 0`. In `REAL` mode, any operation producing a non-zero imaginary part raises a domain error (matching TI's behavior in Real mode). In `RECTANGULAR`/`POLAR` mode, complex values propagate.

**Performance note**: making the default numeric path complex would double arithmetic cost on the hot graphing loop (which is real-only). To avoid this, keep two evaluation entry points:

- `evaluate_real()` — the existing real-only fast path, used by graphing, tables, and stats (unchanged from Phases 1–3).
- `evaluate_complex()` — the complex path, used on the home screen when number mode is RECTANGULAR/POLAR or when a complex literal/`i` appears.

The home-screen evaluator detects whether complex evaluation is needed (presence of `i`, complex-producing functions on negative domains, or non-REAL mode) and routes accordingly. Graphing never uses the complex path.

### 5.3 Parsing and display

**Input**: `i` is recognized as the imaginary unit (a reserved constant). Expressions like `3 + 2i`, `(1+i)^2`, `sqrt(-4)`, `e^(i*pi)` parse and evaluate.

**Display formatting**:

```cpp
namespace math {

// Format a complex value per the current number mode.
//   RECTANGULAR: "3 + 2i", "-1.5 - 0.5i", "4" (pure real), "2i" (pure imag)
//   POLAR:       "2 ∠ 1.047" (r ∠ theta), respecting angle mode for theta
int format_complex(const Complex& z, NumberMode mode, char* buf, size_t len);

}  // namespace math
```

Pure-real results display as plain numbers (no `+ 0i`). Pure-imaginary as `bi`. The `∠` symbol is rendered by the natural-math renderer (a new glyph); polar display respects the degree/radian angle mode for the argument.

### 5.4 Complex functions from the home screen

```
abs(3+4i)        -> 5           (modulus)
arg(1+i)         -> 0.7854      (argument, radians or degrees per mode)
conj(3+2i)       -> 3 - 2i
real(3+2i)       -> 3
imag(3+2i)       -> 2
sqrt(-4)         -> 2i
(1+i)^2          -> 2i
e^(i*pi)         -> -1          (Euler; within display tolerance)
```

### 5.5 Hooks for CAS (Phase 5) and matrices

- **CAS solve** (Phase 5): the symbolic solver's quadratic and polynomial paths use complex arithmetic when the discriminant is negative, emitting roots like `i`, `-i`, `2 + 3i`. The `Complex` type here is the numeric backing; in the symbolic tree, `i` is represented as a reserved symbolic constant and complex literals become `a + b*i` subtrees. Conversion between numeric `Complex` and symbolic form lives in `cas/solve.cpp` — see [phase5-spec.md](phase5-spec.md) §4.1 and §8.
- **Matrix eigenvalues** (4A): as-built, this now returns complex-conjugate eigenvalue pairs too (D30 §7, resolving open question P4-7) — the "real only" note below is historical, kept for context on the original 4A decision.

~~4A computes real eigenvalues only. With the complex type available, a follow-up (deferred, noted in open questions) could return complex-conjugate eigenvalue pairs. Not in Phase 4 scope unless time permits.~~ *(superseded by D30 — see decisions.md)*

---

## 6. CAS engine — moved to Phase 5

The CAS engine (symbolic expression tree, simplification, differentiation,
expansion/factoring, symbolic equation solving, symbolic integration, and
the CAS user interface) was originally specced here as sub-phase 4D. It
has been split out into its own phase given its size (~124 of this
phase's original ~320 estimated hours) and risk profile relative to the
rest of what was Phase 4.

**See [phase5-spec.md](phase5-spec.md) for the full spec.** The content
that used to live in this section (§6.1–6.9) now lives there as
top-level sections §2–§10, unchanged in substance — only the phase
boundary moved. Background: [decisions.md](../notes/decisions.md) D32,
[ti-parity-2026-07-21.md](../notes/ti-parity-2026-07-21.md) §8.

---

## 7. Sub-phase 4D: GC completeness pass (weeks 32–35)

This sub-phase has a different shape from 4A–4C: instead of one coherent
subsystem, it's a grab-bag closing pass that pulls together every gap the
[TI parity stocktake](../notes/ti-parity-2026-07-21.md) found against
TI-83/84+ (excluding CAS-tier items, which are Phase 5) plus the two
lowest-risk ideas from the
[design-departures doc](../notes/design-departures-matrix-complex.md)
(home-screen matrix literals, complex-valued storage). The organizing
question for every item here is the same: **would a TI-83/84+ user
notice this missing?** If yes, it belongs in 4D. If it's Nspire-CAS-tier
(symbolic display, exact-value arithmetic) it stays out — either deferred
to Phase 5 or left as a standalone wishlist item.

### 7.1 Display & number formatting

- **ENG display format**: a third exponent-grouping mode alongside
  FLOAT/FIX/SCI, exponents forced to multiples of 3 (matches TI's
  MODE row). Small, contained change to `math::format`.
- **`▶Frac` / `▶Dec` toggle**: convert a decimal result to a fraction via
  a bounded continued-fraction search (denominator cap, e.g. $\leq 10000$) and
  back. This is arithmetic, not CAS — no symbolic tree needed, just a
  `frac.hpp` helper (`bool decimal_to_fraction(calc_t, int* num, int* den,
  int max_den)`) and a result-formatting hook. Closes the "fraction
  display" parity gap without pulling in Phase 5.
- **Pi-multiple axis tick labels**: when a tick value is a rational
  multiple of $\pi$ within tolerance, label it symbolically (`$\pi/2$`,
  `$3\pi$`) instead of a decimal. Purely a graph-axis rendering rule —
  doesn't need the CAS `Expr` tree, just a rational-multiple-of-$\pi$
  detector reused from the same logic `▶Frac` needs.
- **True subscripts** (`Sₓ`, `σₓ`) in stats/inference output, closing the
  last piece of the wishlist's "Greek/typographic stats display" item
  (D31 shipped the Greek letters; subscripts were explicitly left open).
  Needs a subscript primitive in `render::LayoutNode`, not just a baked
  glyph.
- **Vertical centering for fraction expressions**: the wishlist item —
  stacked numerator/denominator currently top-aligns to the text line
  instead of centering on the fraction bar. Layout-builder fix.

### 7.2 Graphing completeness

- **Sequence graphing mode** (`u`, `v`, `w`): the largest single item in
  this sub-phase. TI's third graph mode alongside function/parametric —
  recursively defined sequences (`u(n) = u(n-1) + 1`, optionally
  referencing a second sequence `v(n-1)`), plotted either as
  time-series (`n` vs. `u(n)`) or web/cobweb plots. Follows the same
  shape as Phase 2's parametric/polar addition: a new `GraphMode::kSeq`,
  a `seq_expr` evaluator (`math/seq_expr.{hpp,cpp}`) that handles
  recursive self-reference with memoization (bounded — a sequence table
  can't recompute from `n=0` every redraw), a `seq_points` point source
  (`graph/seq_points.{hpp,cpp}`), and a dedicated editor
  (`apps/seq_editor.{hpp,cpp}`) for `nMin`, `u(n)`, `u(nMin)` seed
  values. Reuses the existing table/trace/window machinery per-mode, the
  same way Phase 2 did.
- **Zoom preset expansion**: `ZBox` (drag-select a rectangular region —
  needs a two-corner cursor interaction, new but small, similar shape to
  the 4B analysis cursor), `ZDecimal` (window bounds land on exact
  0.1 multiples so trace values are clean), `ZSquare` (adjusts one axis
  so 1 unit is the same pixel distance on both axes, accounting for the
  $320\times320$ panel's aspect ratio — trivial here since it's square,
  unlike TI's non-square LCDs).
- **Shading**: generalize the existing fnInt-only column shading (4B) to
  a standalone `Shade(lower, upper)` graph-screen operation and basic
  inequality shading (`y > f(x)`-style, shade above/below a curve). Reuses
  the column-fill approach 4B already built (D29 §6), extended to a
  user-invoked mode rather than only CALC-menu-triggered.

### 7.3 Data, matrices & complex numbers

- **List↔matrix bridge**: `List►matr(l1, l2, ..., [A])` packs lists into
  matrix columns; `Matr►list([A], l1, l2, ...)` unpacks columns back to
  lists. Straightforward glue between the existing `Array`-backed list
  and matrix storage — no new storage model, just a conversion function
  in `matexpr`.
- **Named/more lists**: raise the fixed 6-list (`l1`..`l6`) cap — either
  more fixed slots or (bigger lift) named user lists. Scope the actual
  cap increase against `ArrayStore` slab headroom (Pico 1 bss is already
  tight — see D28's recurring watch item) before committing to "how
  many"; this item is explicitly capped by that constraint, not by
  design intent.
- **Home-screen matrix literals** (design-departures idea A):
  `[[1,2][3,4]]` typed directly on the home screen, not just built in the
  matrix editor. Closes the D28 tradeoff. Parser-only change in
  `matexpr` — materializes into `MatAns`, no storage-model change.
- **Complex-valued variable and `Ans` storage** (design-departures idea
  B): widen `Variables::vars` and the `Ans` cache to hold either a real
  `calc_t` or a `Complex`, closing the D30 gap (`2i->a` currently
  errors). See the design-departures doc's own scoping notes — this is
  moderate risk (a real storage-type widening, touches every
  `Variables::vars` read site) but explicitly doesn't touch the graphing
  hot path, same dual-path precedent 4C established.
  **Complex-valued lists and matrices (departures ideas C/D) are
  explicitly out of scope for 4D** — the design-departures doc flags
  those as needing a real Pico 1 memory feasibility study first, which
  is bigger than a "closing pass" item. Revisit as a standalone item
  post-4D if wanted.
- **Additional stat plot types**: `xyLine` (scatter with points connected
  in list order) and normal probability plot, rounding out the
  histogram/box/scatter set from Phase 3D to match TI's full stat-plot
  menu.

### 7.4 Scientific extras

- **Scientific constants menu**: a curated catalog (speed of light,
  Planck's constant, Avogadro's number, elementary charge, gas constant,
  etc. — TI-84 CE's constants list is the reference set) insertable from
  a menu or typed by name, backed by `math/constants.{hpp,cpp}` — a
  flat, read-only table, no new evaluator needed.
- **Unit conversions**: length/mass/temperature/volume/etc. conversion
  pairs, either a `convert(value, "from", "to")` function or a dedicated
  conversion screen. Backed by `math/units.{hpp,cpp}` — a conversion
  table keyed by unit-family, plus one multiply/offset per pair. No
  research risk here, just breadth of catalog to decide (scope the unit
  list to TI-84 CE's set rather than trying to be exhaustive).

### 7.5 Device & system polish

- **Auto power-off / standby (APD)**: an inactivity timer that drops to
  a low-power sleep state after N minutes idle, waking on any key press.
  Needs `platform/power.{hpp,cpp}`, an idle timer reset on every input
  event, and confirmation that the display/keyboard HAL actually
  supports a low-power sleep path (feasibility check first — the
  wishlist item flags this as unscoped for exactly this reason).
- **Remember screen brightness / keypad backlight**: persist the current
  brightness/backlight level across power cycles instead of resetting.
  Same feasibility caveat as APD — depends on whether the PicoCalc's
  brightness/backlight controls are readable back or write-only; check
  before committing to a design.
- **Build-identifier diag label**: replace the stale hardcoded
  `"[milestone 1]"` string (`src/main.cpp:213`, unchanged since the
  Phase 1 bootstrap) with the current phase and a build identifier — git
  short hash if the tree is clean, `dev` otherwise. Needs CMake to
  capture `git rev-parse --short HEAD` plus a `git status --porcelain`
  clean/dirty check and pass it through as a compile definition. Small,
  but exactly the kind of loose end a "pre-release milestone" phase
  should close rather than carry forward again.

**Explicitly out of scope for 4D** (per the parity doc's own boundary):
CAS-tier items (symbolic simplify, surd/exact-value display, arbitrary
symbolic differentiation/integration) — Phase 5. MicroPython and any
other non-calculator app — Phase 6. Complex-valued lists/matrices
(departures ideas C/D) and the unified tagged-value evaluator (idea F) —
explicitly deferred past 4D, see §7.3 above.

---

## 8. Task breakdown

Solo developer, part-time (~20 hrs/week).

### Sub-phase 4A: Matrix operations (weeks 26–27)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 4A.1 | `Matrix` view over 2-D `Array`; `MatrixStore` ([A]–[J]) | 6 | Create $10\times10$ matrix, read/write elements |
| 4A.2 | Matrix arithmetic (add, subtract, multiply, scalar) | 4 | `[A]*[B]` correct for known cases |
| 4A.3 | Transpose, determinant (LU) | 4 | `det(identity(5))` = 1.0; known $3\times3$ correct |
| 4A.4 | Inverse (Gauss-Jordan), rref | 6 | `[A]*inverse([A])` ≈ identity |
| 4A.5 | Eigenvalues (QR, real, $n \leq 10$) | 6 | Diagonal/symmetric eigenvalues correct |
| 4A.6 | Matrix editor screen | 8 | Enter $3\times3$, save, reload, verify |
| 4A.7 | Matrix functions in home-screen parser | 4 | `det([A])` evaluates from home screen |
| 4A.8 | Matrix persistence (`MatrixStore::save/load`) | 2 | Survives power cycle |
| 4A.9 | Numeric equation solver + solver screen | 6 | Solve $x^3 - 2x - 5 = 0$ → $x \approx 2.0946$ |
| | **Subtotal** | **~46 hrs** | |

### Sub-phase 4B: Graph analysis / CALC menu (weeks 28–29)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 4B.1 | `AnalysisResult` + analysis engine scaffold | 3 | Framework compiles, dispatches by op |
| 4B.2 | `analyze_value` (all 3 modes) | 3 | Evaluate function/param/polar at a point |
| 4B.3 | `analyze_zero` (reuse numeric_solve) | 3 | Root of $\sin(x)$ near $\pi$ found |
| 4B.4 | `analyze_extremum` (Brent min/max) | 6 | Max of $-x^2+4$ at $x=0$ |
| 4B.5 | `analyze_intersect` | 4 | Intersection of $x$ and $x^2$ at 0 and 1 |
| 4B.6 | `analyze_derivative` (central diff + Richardson) | 3 | dy/dx of $x^2$ at 3 → 6 |
| 4B.7 | `analyze_integral` (adaptive Gauss-Kronrod) | 5 | $\int_0^\pi \sin x\,dx$ ≈ 2.0 |
| 4B.8 | Parametric/polar calculus variants | 6 | Polar area, param slope correct |
| 4B.9 | `AnalysisCursor` interactive bound/guess flow | 8 | Left/right bound + guess UI works |
| 4B.10 | `calc_menu` screen + graph-screen integration | 5 | CALC menu → operation → on-graph result |
| 4B.11 | Region shading (fnInt) + tangent draw (dy/dx) | 3 | Shaded integral region, tangent line |
| 4B.12 | Result readout + store-to-variable | 2 | Root stored to X, integral to Ans |
| | **Subtotal** | **~51 hrs** | |

### Sub-phase 4C: Complex numbers (weeks 30–31)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 4C.1 | `Complex` type + arithmetic operators | 4 | `(1+i)*(1-i)` = 2 |
| 4C.2 | Complex elementary functions (sqrt, exp, ln, pow) | 6 | `sqrt(-4)` = 2i; `e^(i*pi)` ≈ -1 |
| 4C.3 | Complex trig + inverse trig | 4 | `sin(i)` correct (≈ 1.1752i) |
| 4C.4 | Component functions (abs, arg, conj, re, im) | 3 | `abs(3+4i)` = 5 |
| 4C.5 | `NumberMode` + dual eval entry points | 8 | REAL mode errors on sqrt(-1); RECT allows |
| 4C.6 | Parser: `i` recognition, complex literals | 4 | `3 + 2i` parses |
| 4C.7 | `format_complex` (rectangular + polar) | 4 | `2∠1.047` in polar mode |
| 4C.8 | Angle-symbol glyph + renderer integration | 3 | `∠` renders in polar results |
| 4C.9 | Number-mode screen + persistence | 2 | Mode survives power cycle |
| | **Subtotal** | **~38 hrs** | |

*(Sub-phase 4D, CAS engine, ~124 hrs — moved to [phase5-spec.md](phase5-spec.md) §11, task IDs kept as `4D.n` there for continuity.)*

### Sub-phase 4D: GC completeness pass (weeks 32–35)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 4D.1 | ENG display format | 3 | `1234` in ENG shows `1.234E3` |
| 4D.2 | `▶Frac`/`▶Dec` toggle (bounded continued-fraction) | 6 | `0.75 ▶Frac` → `3/4` |
| 4D.3 | Pi-multiple axis tick labels | 4 | Tick at $\pi/2$ shows `$\pi/2$` not `1.5708` |
| 4D.4 | True subscripts in stats/inference render | 5 | `Sₓ`, `σₓ` render with a real subscript |
| 4D.5 | Fraction vertical centering | 2 | Stacked fraction centers on the bar |
| 4D.6 | Sequence graphing: `seq_expr` evaluator + memoized recursion | 10 | `u(n)=u(n-1)+1, u(0)=1` tables correctly |
| 4D.7 | Sequence graphing: point source + time-series/web plot | 8 | Sequence plots on graph screen |
| 4D.8 | Sequence editor screen + window/table integration | 8 | Enter `nMin`, `u(n)`, seed; graph/table work |
| 4D.9 | `ZBox` (drag-select zoom) | 5 | Select region, window updates |
| 4D.10 | `ZDecimal` / `ZSquare` | 3 | Bounds land on clean multiples / equal-aspect |
| 4D.11 | `Shade()` + inequality shading | 5 | `y > x^2` shades correctly |
| 4D.12 | `List►matr` / `Matr►list` | 3 | Round-trips `l1,l2 ↔ [A]` correctly |
| 4D.13 | List cap increase (scope against Pico 1 headroom first) | 4 | New cap set, existing tests pass |
| 4D.14 | Home-screen matrix literals | 4 | `[[1,2][3,4]]` evaluates from home screen |
| 4D.15 | Complex-valued `Variables`/`Ans` storage | 10 | `2i->a` stores; `a` reads back as `2i` |
| 4D.16 | `xyLine` + normal probability stat plots | 5 | Both plot types render correctly |
| 4D.17 | Scientific constants catalog + menu | 4 | Insert `c` (speed of light) from menu |
| 4D.18 | Unit conversions catalog + `convert()` | 6 | `convert(1, "mi", "km")` ≈ 1.609 |
| 4D.19 | Auto power-off (APD) — feasibility check first | 8 | Idles to sleep, wakes on key press |
| 4D.20 | Brightness/backlight persistence — feasibility check first | 4 | Setting survives power cycle |
| 4D.21 | Build-identifier diag label | 2 | Diag screen shows hash or `dev` |
| | **Subtotal** | **~109 hrs** | |

### Summary

| Sub-phase | Weeks | Hours | Deliverable |
|-----------|-------|-------|-------------|
| 4A: Matrix operations | 26–27 | ~46 | Matrix editor, arithmetic, rref/det/inverse/eigen, numeric solver |
| 4B: Graph analysis (CALC) | 28–29 | ~51 | value/zero/min/max/intersect/dy-dx/fnInt, interactive on graph |
| 4C: Complex numbers | 30–31 | ~38 | Complex type, functions, a+bi/polar mode |
| 4D: GC completeness | 32–35 | ~109 | Sequence mode, zoom/shading, list↔matrix, sci constants/units, home-screen matrix literals, complex storage, device polish |
| **Total Phase 4** | **26–35 (~10 weeks)** | **~244 hrs** | Pre-release milestone: full TI-83/84+-class GC functionality |
| *CAS engine — moved to Phase 5* | *32–36* | *~124* | *see [phase5-spec.md](phase5-spec.md) §11* |
| *MicroPython — moved to Phase 6* | | *~61* | *see [phase6-spec.md](phase6-spec.md) §5* |

---

## 9. Performance expectations

### Graph analysis (4B)

| Operation | Method | Pico 1 | Pico 2 |
|-----------|--------|--------|--------|
| value | single eval | <1 ms | <1 ms |
| zero / intersect | bisection+Newton (~20–40 iters) | 5–20 ms | 1–5 ms |
| min / max | Brent (~20–50 iters) | 10–40 ms | 2–10 ms |
| dy/dx | central diff + Richardson (~6 evals) | 1–3 ms | <1 ms |
| fnInt | adaptive Gauss-Kronrod (~15–200 evals) | 10–100 ms | 2–30 ms |

All interactive-grade. fnInt on a hard integrand (many subdivisions) is the worst case; show a brief "computing…" if it exceeds ~100 ms.

### Complex numbers (4C)

Complex arithmetic is 2–$6\times$ the cost of real (a complex multiply is 4 real multiplies + 2 adds). Since complex evaluation is confined to the home-screen path (never graphing), this is imperceptible for single expressions. Complex elementary functions (`c_exp`, `c_ln`) cost ~2–$3\times$ their real counterparts.

*(CAS operation benchmarks moved to [phase5-spec.md](phase5-spec.md) §12.)*

### Matrix benchmarks (estimated)

| Operation | Size | Pico 1 | Pico 2 |
|-----------|------|--------|--------|
| Multiply | $10\times10$ | ~10 ms | ~2 ms |
| Determinant | $10\times10$ | ~5 ms | ~1 ms |
| Inverse | $10\times10$ | ~15 ms | ~3 ms |
| Eigenvalues (QR) | $10\times10$ | ~200 ms | ~40 ms |
| Inverse | $50\times50$ | ~8 s | ~1.5 s |

Large matrices ($>$ $20\times20$) are slow on Pico 1 — acceptable, as these are uncommon in handheld use.

### GC completeness (4D)

Nearly all of 4D is either a lookup (constants/units catalogs, O(1)) or a
UI/parser change with no meaningful runtime cost. The two items worth a
performance note:

| Operation | Method | Pico 1 | Pico 2 |
|-----------|--------|--------|--------|
| Sequence table recompute | memoized recursion, capped depth | 1–10 ms typical | <1 ms |
| `▶Frac` conversion | bounded continued-fraction search | <1 ms | <1 ms |

Sequence mode's memoization needs a cap — an unbounded `u(n-1)` chain
from a large `n` without a memo table is $O(n)$ recursive calls per
redraw, which is the one place in 4D that could actually stutter if
implemented naively.

---

## 10. Risks and mitigations

*(Risks 1, 2, and 5 were CAS-specific — moved to [phase5-spec.md](phase5-spec.md) §13. Numbering below is left as originally assigned, matching cross-references elsewhere in this document and in decisions.md.)*

### Risk 3: Complex evaluation slowing the hot path

If the numeric evaluator became complex-by-default, graphing would slow 2–$6\times$. **Mitigation**: dual entry points — `evaluate_real()` (fast, used by graphing/tables/stats) and `evaluate_complex()` (home screen only). Graphing never touches the complex path. This is a firm architectural rule, documented in `AGENTS.md`.

### Risk 4: Numeric integration accuracy vs. speed (4B fnInt)

Adaptive Gauss-Kronrod can over-subdivide on oscillatory or singular integrands, becoming slow. **Mitigation**: cap subdivision depth; report the error estimate alongside the result; fall back to a coarser fixed rule if the depth cap is hit rather than hanging.

*(Risks 6 and 7 were MicroPython-specific — moved to
[phase6-spec.md](phase6-spec.md) §7. Numbering left as originally
assigned.)*

### Risk 8: 4D's breadth invites scope creep

Unlike 4A–4C, 4D has no single hard technical core — it's ~20 mostly
independent small features. The risk isn't any one item failing, it's
the sub-phase quietly growing as each item picks up "just one more"
extension (e.g., sequence mode acquiring web/cobweb-plot styling
options TI itself treats as a stretch feature). **Mitigation**: hold the
line at the parity-doc-derived scope in §7 above; anything beyond it
goes back to the wishlist rather than expanding 4D's task list
mid-implementation.

### Risk 9: APD / brightness persistence feasibility is unverified

Both device-polish items (§7.5) assume the display/keyboard HAL supports
a low-power sleep path and that brightness/backlight state is readable
back, neither confirmed yet. **Mitigation**: both task rows (4D.19,
4D.20) are explicitly flagged "feasibility check first" in §8 — spend an
hour confirming the HAL surface before scoping the real implementation;
if either turns out to be write-only/unsupported, drop that item back to
the wishlist rather than forcing it.

---

## 11. Open questions for Phase 4

*(P4-1, P4-2, P4-3 were CAS-specific — moved to [phase5-spec.md](phase5-spec.md) §14 as P5-1/P5-2/P5-3. P4-4/P4-5 were MicroPython-specific — moved to [phase6-spec.md](phase6-spec.md) §8 as P6-1/P6-2. Numbering below is left as originally assigned.)*

| # | Question | Options | When |
|---|----------|---------|------|
| P4-6 | CALC intersect with >2 curves: pick two via cursor, or list? | Cursor-cycle vs. explicit picker | Week 28, task 4B.5 |
| P4-7 | Complex eigenvalues for matrices (4A produces real only)? | Add conjugate-pair support in 4C, or defer | Week 30, task 4C — likely defer |
| P4-8 | Polar `fnInt`: area ($\frac{1}{2}\int r^2 d\theta$) only, or also arc length? | Area matches TI; arc length is a nice extra | Week 29, task 4B.8 |
| P4-9 | Number-mode default on first boot: REAL or RECTANGULAR? | REAL matches TI default; RECT is friendlier | Week 31, task 4C.9 |
| P4-10 | List cap increase (4D.13): how many lists, fixed slots or named? | Bounded by Pico 1 `ArrayStore` headroom — scope at implementation time | Week 32-35, task 4D.13 |
| P4-11 | Complex storage (4D.15): do real-only readers (matexpr scalar subterms, listexpr) error or silently truncate on a complex-valued variable? | Error (matches REAL-mode precedent) vs. silent real-part truncation | Week 32-35, task 4D.15 |
| P4-12 | Sequence mode: support two-sequence cross-reference (`u(n)` referencing `v(n-1)`) in v1, or single-sequence only? | TI supports cross-reference; single-sequence is simpler first cut | Week 32-35, task 4D.6 |

---

## 12. Reconciliation notes

- **Matrix on `Array`** (§3): Phase 4's `Matrix` is now a linear-algebra view over Phase 3's 2-D `Array`, not a standalone storage class. This resolves the reconciliation flagged in Phase 3 §10. Storage, allocation, and element access come from `Array`; `Matrix` adds only linear-algebra methods.
- **Numeric vs. symbolic calculus**: 4B's `dy/dx` and `fnInt` are numeric and live on the graph screen; Phase 5's `differentiate` and `integrate` are symbolic and live in the CAS menu. They are complementary, not duplicative — documented in phase5-spec.md §1.
- **Complex before CAS**: 4C shipped before Phase 5 so CAS `solve` can return complex roots. The symbolic `i` and numeric `Complex` share a conversion layer (§5.5; phase5-spec.md §13 Risk 5).
- **Dual evaluation path**: the complex subsystem must not slow graphing. `evaluate_real()` stays the hot path; `evaluate_complex()` is home-screen only (§5.2, Risk 3). This rule now also binds Phase 5's CAS parsing/evaluation, not just this document's own complex subsystem.
- **Sequence mode reuses Phase 2's per-mode pattern**: 4D.6–4D.8 (§7.2) follow the exact shape Phase 2 established for parametric/polar — a mode enum value, a dedicated expression evaluator, a point source, and a mode-aware editor/table/window, all riding the existing trace/split-screen machinery. No new architectural pattern, just a third instance of an existing one.
- **4D depends on 4A–4C, not vice versa**: home-screen matrix literals (4D.14) and complex storage (4D.15) extend `matexpr` (4A, D28) and `Variables`/`NumberMode` (4C, D30) respectively. Nothing in 4A–4C was written anticipating 4D — this is why 4D is scoped last.

---

## 13. References

1. Phase 1 spec — [phase1-spec.md](phase1-spec.md)
2. Phase 2 spec (parametric/polar mode pattern §7.2 reuses) — [phase2-spec.md](phase2-spec.md)
3. Phase 3 spec (Array primitive, stat-plot layer) — [phase3-spec.md](phase3-spec.md)
4. Phase 5 spec (CAS — DB48X/KhiCAS references live there now) — [phase5-spec.md](phase5-spec.md)
5. Phase 6 spec (MicroPython, moved from this document) — [phase6-spec.md](phase6-spec.md)
6. TI parity stocktake (source of 4D's scope) — [ti-parity-2026-07-21.md](../notes/ti-parity-2026-07-21.md)
7. Design departures (source of 4D.14/4D.15) — [design-departures-matrix-complex.md](../notes/design-departures-matrix-complex.md)
8. tinyexpr++ (numeric evaluation) — https://github.com/Blake-Madden/tinyexpr-plus-plus
9. Brent's method (extrema) — Brent, "Algorithms for Minimization without Derivatives"
10. Gauss-Kronrod quadrature — Piessens et al., QUADPACK
11. TI-84 Plus CALC menu guidebook — https://education.ti.com/en/guidebook/details/en/6152F7C2E0B9491482D4CF5C3EEB6EB1/84plce
12. QR algorithm for eigenvalues — Golub & Van Loan, "Matrix Computations"
