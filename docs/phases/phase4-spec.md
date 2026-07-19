# Phase 4 Spec: Matrix, Graph Analysis, Complex, CAS & Programming

**Prerequisite phases**: Phase 1 (HAL + calculator + graphing), Phase 2 (table view, parametric/polar, split-screen), Phase 3 (statistics, data lists, the shared `Array` primitive).

**Scope**: Add matrix operations, interactive graph analysis (the TI-84 CALC menu), a complex-number subsystem, a symbolic math engine (CAS), and a MicroPython programming environment. This phase transforms the calculator from a numerical/statistical tool into an algebraic one with interactive analysis and programmability.

**End state**: the calculator can manipulate matrices; analyze graphs interactively (roots, extrema, intersections, numeric derivative and integral); compute with complex numbers throughout; differentiate, simplify, factor, and solve expressions symbolically (including complex roots); and run user-written Python programs with access to all calculator functions.

---

## 1. Overview and phasing within Phase 4

Phase 4 is the largest and most varied phase. It splits into five sub-phases, developed in order. Each occupies a different part of the codebase, but there are deliberate dependencies: graph analysis reuses 4A's numeric solver, and CAS's complex-aware solving depends on the 4C complex subsystem.

| Sub-phase | Weeks | Content |
|-----------|-------|---------|
| 4A: Matrix operations | 26–27 | Matrix editor, arithmetic, det, inverse, rref, eigenvalues; numeric solver |
| 4B: Graph analysis (CALC) | 28–29 | value, zero, min/max, intersect, dy/dx, fnInt — numeric + interactive on the graph screen |
| 4C: Complex numbers | 30–31 | `Complex` type, complex-aware arithmetic and functions, a+bi mode, rect/polar display |
| 4D: CAS engine | 32–36 | Symbolic tree, differentiation, simplification, factoring, solving (complex-aware), integration |
| 4E: MicroPython | 37–39 | Embedded interpreter, `calc` module, on-device editor, SD card scripts |

**Total estimated effort**: ~14 weeks part-time (~320 hours).

The ordering matters:

- **4B (graph analysis) after 4A** because zero/intersect reuse 4A's numeric root-finder, and the CALC operations are numeric in this phase.
- **4C (complex) before 4D (CAS)** because CAS equation solving is complex-aware — `solve(x^2 + 1 = 0)` returns `{i, -i}`, which requires the complex representation to exist first.
- **4D (CAS) is the dominant effort** and highest risk. It's specced to be built incrementally: a minimal-but-correct core extended feature by feature.
- **4E (MicroPython) last** so its `calc` bindings can expose everything built in 4A–4D.

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
│   ├── expr_tree.hpp / .cpp       # NEW (4D): symbolic expression tree (AST)
│   ├── cas/
│   │   ├── simplify.hpp / .cpp    # Algebraic simplification rules
│   │   ├── derivative.hpp / .cpp  # Symbolic differentiation
│   │   ├── integrate.hpp / .cpp   # Symbolic integration (table-based)
│   │   ├── factor.hpp / .cpp      # Polynomial factoring
│   │   ├── solve.hpp / .cpp       # Symbolic equation solver (complex-aware)
│   │   ├── expand.hpp / .cpp      # Distribution / expansion
│   │   └── rules.hpp             # Shared rewriting utilities
│   └── types.hpp                  # Updated: complex type, matrix view alias
├── graph/
│   ├── analysis.hpp / .cpp        # NEW (4B): CALC operations engine
│   └── analysis_cursor.hpp / .cpp # NEW (4B): interactive bound/guess cursor
├── apps/
│   ├── matrix_editor.hpp / .cpp   # NEW (4A): matrix editor screen
│   ├── solver_screen.hpp / .cpp   # NEW (4A): numeric & symbolic solver UI
│   ├── calc_menu.hpp / .cpp       # NEW (4B): graph analysis menu + interaction
│   ├── cas_screen.hpp / .cpp      # NEW (4D): CAS worksheet / symbolic mode
│   └── program_screen.hpp / .cpp  # NEW (4E): MicroPython editor + runner
├── scripting/
│   ├── micropython_embed.hpp/.cpp # NEW (4E): MicroPython interpreter wrapper
│   ├── calc_module.hpp / .cpp     # NEW (4E): Python 'calc' module (C++ bindings)
│   └── script_runner.hpp / .cpp   # NEW (4E): load + execute .py from SD card
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

**Memory strategy**: unchanged from the Array primitive's model. The calculator exposes 10 matrix variables ($[A]$ through $[J]$) of up to 99$\times$99. A 99$\times$99 `double` matrix is ~76 KB, living in PSRAM (Array chooses PSRAM above its size threshold). Small matrices (up to ~16$\times$16) stay in the SRAM pool for speed. With 8 MB PSRAM, even ten full-size matrices use under 1 MB.

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

A complex-number subsystem that flows through the numeric evaluator and, in 4D, into CAS solving. `sqrt(-1)` yields `i`; expressions evaluate over ℂ when a complex value appears; results display in rectangular ($a + bi$) or polar ($r\angle\theta$) form.

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

### 5.5 Hooks for CAS (4D) and matrices

- **CAS solve** (4D): the symbolic solver's quadratic and polynomial paths use complex arithmetic when the discriminant is negative, emitting roots like `i`, `-i`, `2 + 3i`. The `Complex` type here is the numeric backing; in the symbolic tree, `i` is represented as a reserved symbolic constant (see §6.1) and complex literals become `a + b*i` subtrees. Conversion between numeric `Complex` and symbolic form lives in `cas/solve.cpp`.
- **Matrix eigenvalues** (4A): 4A computes real eigenvalues only. With the complex type available, a follow-up (deferred, noted in open questions) could return complex-conjugate eigenvalue pairs. Not in Phase 4 scope unless time permits.

---
## 6. Sub-phase 4D: CAS engine (weeks 32–36)

This is the core of Phase 4. The CAS operates on a new symbolic expression tree (`ExprTree`) that is distinct from the numerical evaluation path. The numerical evaluator (tinyexpr++) remains for graphing and immediate numeric results. The CAS is invoked explicitly — when the user types an expression and presses a CAS-specific key or selects a CAS operation from a menu.

CAS also provides the *symbolic* counterparts to the numeric `dy/dx` and `fnInt` built in Sub-phase 4B. Where 4B computes a numeric derivative at a point or a definite integral over bounds, CAS `differentiate` (§6.5) and `integrate` (§6.8) return symbolic expressions. On the graph screen, the CALC menu's `dy/dx` and `∫` stay numeric (fast, always-available); a user wanting the symbolic form uses the CAS menu on the function's expression.

### 6.1 Symbolic expression tree

```cpp
namespace math {

// Node types for the symbolic expression tree.
enum class ExprType : uint8_t {
    // Atoms
    NUM,        // Numeric literal: 3, -2.5, pi
    VAR,        // Variable: x, y, t, a, b (and reserved 'i' = imaginary unit)
    
    // Binary operations
    ADD,        // a + b (n-ary via linked children: a + b + c)
    MUL,        // a * b (n-ary)
    POW,        // a ^ b
    
    // Unary operations
    NEG,        // -a (separate from SUB for tree clarity)
    
    // Functions
    FUNC,       // Named function application: sin(x), ln(x), etc.
    
    // Structural
    EQ,         // Equation: lhs = rhs
};

struct Expr {
    ExprType type;
    
    union {
        calc_t num_val;            // For NUM
        char var_name;             // For VAR: single char 'a'-'z'
        char func_name[12];        // For FUNC: "sin", "cos", "ln", etc.
    };
    
    // Children: singly-linked list via next, tree via child.
    // ADD and MUL are n-ary: children linked as a chain.
    //   ADD(a, b, c) → child=a, a->next=b, b->next=c
    // POW is binary: child=base, child->next=exponent
    // FUNC is unary: child=argument
    Expr* child = nullptr;
    Expr* next  = nullptr;
    
    // ---- Convenience constructors (use pool allocator) ----
    static Expr* num(calc_t val);
    static Expr* var(char name);
    static Expr* add(Expr* a, Expr* b);
    static Expr* mul(Expr* a, Expr* b);
    static Expr* pow(Expr* base, Expr* exp);
    static Expr* neg(Expr* a);
    static Expr* func(const char* name, Expr* arg);
    static Expr* eq(Expr* lhs, Expr* rhs);
    
    // ---- Predicates ----
    bool is_num() const { return type == ExprType::NUM; }
    bool is_var() const { return type == ExprType::VAR; }
    bool is_zero() const { return is_num() && num_val == 0.0; }
    bool is_one() const  { return is_num() && num_val == 1.0; }
    bool is_neg_one() const { return is_num() && num_val == -1.0; }
    bool is_add() const { return type == ExprType::ADD; }
    bool is_mul() const { return type == ExprType::MUL; }
    bool is_pow() const { return type == ExprType::POW; }
    bool is_func() const { return type == ExprType::FUNC; }
    
    // Does this expression contain variable v?
    bool contains(char v) const;
    
    // Deep equality
    bool equals(const Expr* other) const;
    
    // Deep clone (into current pool)
    Expr* clone() const;
    
    // Count of child nodes (for n-ary ADD/MUL)
    int child_count() const;
};

} // namespace math
```

### 6.2 Memory management

Symbolic manipulation creates many short-lived intermediate trees. A **pool allocator** sized for CAS operations avoids heap fragmentation:

```cpp
namespace math {

class ExprPool {
public:
    // On Pico 1: pool lives in PSRAM (64 KB default allocation)
    // On Pico 2: pool lives in SRAM (32 KB) with PSRAM overflow
    void init(size_t pool_size = 65536);
    
    Expr* alloc();            // Get a fresh Expr node
    void reset();             // Free everything (between operations)
    
    size_t used() const;
    size_t capacity() const;
    
private:
    uint8_t* pool_ = nullptr;
    size_t offset_ = 0;
    size_t capacity_ = 0;
};

// Global CAS pool — reset before each top-level CAS operation
extern ExprPool g_cas_pool;

} // namespace math
```

Each top-level CAS operation (differentiate, simplify, solve) calls `g_cas_pool.reset()` at the start, works entirely within the pool, then copies the final result to a persistent output buffer before the pool is reclaimed. This means intermediate garbage is never individually freed — the entire pool is bulk-reclaimed.

**Sizing**: a single `Expr` node is ~32 bytes. A 64 KB pool holds ~2000 nodes. This is sufficient for expressions of practical complexity — even a 50-term polynomial after expansion produces ~150 nodes. Deeply recursive operations (repeated integration) may need the pool to spill into PSRAM on Pico 1, which is why the pool lives there by default.

### 6.3 Expression parser (string → `Expr` tree)

The CAS needs its own parser because tinyexpr++ produces an opaque evaluation tree, not a manipulable AST. The CAS parser handles the same infix syntax but produces `Expr` nodes:

```cpp
namespace math {

// Parse an infix expression string into an Expr tree.
// Returns nullptr on parse error; sets *error to a message.
Expr* parse_expr(const char* input, const char** error = nullptr);

// Serialize an Expr tree back to a human-readable string.
// Writes into buf, returns number of chars written.
int expr_to_string(const Expr* expr, char* buf, size_t buf_len);

// Serialize an Expr tree into a layout tree for pretty-print rendering.
// Reuses the render::LayoutNode system from Phase 1.
render::LayoutNode* expr_to_layout(const Expr* expr);

} // namespace math
```

The parser is a standard recursive-descent parser for mathematical expressions. Operator precedence: `=` < `+`/`-` < `*`/`/` < unary `-` < `^` < function application. Implicit multiplication (e.g., `2x` → `2*x`, `xy` → `x*y`) is supported for CAS mode since it's natural for algebraic entry.

**Imaginary unit in the symbolic tree**: `i` is a reserved `VAR` whose algebraic rule is $i^2 = -1$. The simplifier (§6.4) knows this rule, so `i*i` simplifies to `-1`, `i^3` to `-i`, and `i^4` to `1`. Complex literals appear as ordinary subtrees, e.g., `3 + 2*i`. When a symbolic result containing `i` is evaluated numerically or stored, it converts to a `math::Complex` (Sub-phase 4C). This shared representation is what lets CAS `solve` return complex roots.

### 6.4 Simplification (`cas/simplify.hpp`)

The simplifier applies a set of rewriting rules repeatedly until no rule fires (fixed-point iteration). Rules are applied bottom-up — children are simplified before parents.

```cpp
namespace math::cas {

// Simplify an expression. Returns a new tree (in the current pool).
// The original is not modified.
Expr* simplify(const Expr* expr);

} // namespace math::cas
```

**Rule categories and representative rules**:

**Identity and annihilation rules** (trivial, apply first):

| Rule | Example |
|------|---------|
| $x + 0 \to x$ | `ADD(x, 0)` → `x` |
| $x \cdot 0 \to 0$ | `MUL(x, 0)` → `0` |
| $x \cdot 1 \to x$ | `MUL(x, 1)` → `x` |
| $x^0 \to 1$ | `POW(x, 0)` → `1` |
| $x^1 \to x$ | `POW(x, 1)` → `x` |
| $0^x \to 0$ (for $x > 0$) | `POW(0, x)` → `0` |
| $1^x \to 1$ | `POW(1, x)` → `1` |
| $-(-x) \to x$ | `NEG(NEG(x))` → `x` |

**Constant folding** — evaluate numeric sub-expressions:

| Rule | Example |
|------|---------|
| $a + b \to c$ where $a$, $b$ are numbers | `ADD(2, 3)` → `5` |
| $a \cdot b \to c$ | `MUL(4, 5)` → `20` |
| $a^b \to c$ where result is exact integer | `POW(2, 10)` → `1024` |
| $\sin(0) \to 0$, $\cos(0) \to 1$, etc. | Exact values at known points |

**Like-term collection**:

| Rule | Example |
|------|---------|
| $ax + bx \to (a+b)x$ | `3x + 5x` → `8x` |
| $x + x \to 2x$ | Special case of above |
| $x^a \cdot x^b \to x^{a+b}$ | `x^2 * x^3` → `x^5` |
| $\frac{x^a}{x^b} \to x^{a-b}$ | Handled via `x^a * x^{-b}` |

**Canonical ordering** — sort commutative operands for consistent matching:

- In ADD nodes: numeric terms last, variables alphabetically
- In MUL nodes: numeric coefficients first, then variables alphabetically, then functions

**Fraction simplification**:

| Rule | Example |
|------|---------|
| $\frac{a}{1} \to a$ | |
| $\frac{0}{a} \to 0$ | |
| $\frac{a}{a} \to 1$ | When $a$ is structurally identical |
| $\frac{na}{nb} \to \frac{a}{b}$ | GCD reduction of integer coefficients |

**Implementation approach**: each rule is a function `Expr* try_rule(const Expr* e)` that returns a new tree if the rule applies, or `nullptr` if it doesn't. The simplifier loops through the rule set applying every applicable rule, then recurs into children, repeating until a full pass produces no changes. A hard iteration cap (e.g., 50 passes) prevents infinite loops from malformed rule interactions.

### 6.5 Symbolic differentiation (`cas/derivative.hpp`)

```cpp
namespace math::cas {

// Differentiate expr with respect to variable var.
// Returns a simplified result.
Expr* differentiate(const Expr* expr, char var);

} // namespace math::cas
```

Differentiation is the most straightforward CAS operation — it's a direct structural recursion over the tree with well-defined rules:

| Rule | Derivative |
|------|-----------|
| $\frac{d}{dx} c = 0$ | Constant (number or variable $\neq x$) |
| $\frac{d}{dx} x = 1$ | Identity |
| $\frac{d}{dx} (u + v) = u' + v'$ | Sum rule |
| $\frac{d}{dx} (u \cdot v) = u'v + uv'$ | Product rule |
| $\frac{d}{dx} \frac{u}{v} = \frac{u'v - uv'}{v^2}$ | Quotient rule (derived from product + power) |
| $\frac{d}{dx} u^n = n \cdot u^{n-1} \cdot u'$ | Power rule (constant exponent) |
| $\frac{d}{dx} u^v = u^v(v' \ln u + v \frac{u'}{u})$ | General power rule |
| $\frac{d}{dx} \sin(u) = \cos(u) \cdot u'$ | Chain rule for sin |
| $\frac{d}{dx} \cos(u) = -\sin(u) \cdot u'$ | Chain rule for cos |
| $\frac{d}{dx} \tan(u) = \sec^2(u) \cdot u'$ | Or: $(1 + \tan^2(u)) \cdot u'$ |
| $\frac{d}{dx} e^u = e^u \cdot u'$ | Chain rule for exp |
| $\frac{d}{dx} \ln(u) = \frac{u'}{u}$ | Chain rule for ln |
| $\frac{d}{dx} \sqrt{u} = \frac{u'}{2\sqrt{u}}$ | Via power rule: $u^{1/2}$ |
| $\frac{d}{dx} \arcsin(u) = \frac{u'}{\sqrt{1 - u^2}}$ | Inverse trig |
| $\frac{d}{dx} \arccos(u) = \frac{-u'}{\sqrt{1 - u^2}}$ | Inverse trig |
| $\frac{d}{dx} \arctan(u) = \frac{u'}{1 + u^2}$ | Inverse trig |

The raw derivative output is typically bloated (e.g., $\frac{d}{dx}[x^2 \cdot \sin(x)]$ produces `2*x*sin(x) + x^2*cos(x)` which is already simplified, but $\frac{d}{dx}[\frac{x}{x+1}]$ produces `(1*(x+1) - x*1) / (x+1)^2`). Every differentiation call ends with `simplify()` to clean up the result.

**Higher-order derivatives**: `differentiate(differentiate(expr, 'x'), 'x')` — just recurse. A `differentiate_n(expr, var, n)` convenience wrapper applies $n$ times.

### 6.6 Expansion and factoring (`cas/expand.hpp`, `cas/factor.hpp`)

**Expand** distributes multiplication over addition:

```cpp
namespace math::cas {

// Expand products and powers of sums.
// E.g., (x+1)*(x-1) → x^2 - 1
//       (x+1)^3 → x^3 + 3x^2 + 3x + 1
Expr* expand(const Expr* expr);

} // namespace math::cas
```

Implementation: recursively expand `MUL(ADD(...), ADD(...))` using the FOIL-like distribution algorithm. For `POW(ADD(...), n)` where $n$ is a positive integer, expand via binomial theorem or repeated multiplication. Cap exponent at a reasonable limit (e.g., $n \leq 20$) to prevent combinatorial explosion.

**Factor** performs polynomial factoring:

```cpp
namespace math::cas {

// Factor a polynomial expression in variable var.
// Handles: common factor extraction, difference of squares,
// quadratic formula, and rational root theorem for degree <= 4.
// Returns the factored form, or the original if no factoring found.
Expr* factor(const Expr* expr, char var = 'x');

} // namespace math::cas
```

Factoring strategy (in order of attempt):

1. **Common factor extraction**: pull out the GCD of all term coefficients and the lowest power of each variable. E.g., $6x^3 + 4x^2 \to 2x^2(3x + 2)$.
2. **Difference of squares**: $a^2 - b^2 \to (a+b)(a-b)$.
3. **Quadratic**: for degree-2 polynomials, use the discriminant. If $\Delta = b^2 - 4ac$ is a perfect square, produce $(x - r_1)(x - r_2)$. Otherwise, return as-is or use the quadratic formula symbolically.
4. **Rational root theorem**: for degree 3–4, test rational roots $\pm \frac{p}{q}$ where $p$ divides the constant term and $q$ divides the leading coefficient. If a root is found, divide out the factor and recurse.
5. **Grouping**: for four-term expressions, try grouping pairs and factoring common sub-expressions.

This handles the vast majority of factoring problems encountered in a high-school / early-college calculus context. It will not factor quintics or higher-degree polynomials with irrational roots — that requires Galois theory and is well beyond what even the HP-50G or TI-Nspire CAS attempt.

### 6.7 Symbolic equation solving (`cas/solve.hpp`)

```cpp
namespace math::cas {

// Solve an equation for variable var.
// Returns a list of solutions (as Expr* array).
// max_solutions caps the output.
struct SolveResult {
    Expr* solutions[8];
    int count;
    bool exact;    // true if all solutions are symbolic/exact
    bool complex;  // true if any solution has a nonzero imaginary part
};

SolveResult solve(const Expr* equation, char var);

} // namespace math::cas
```

Solving strategy (in order of attempt):

1. **Linear isolation**: if the equation is linear in `var`, algebraically isolate it. E.g., $3x + 5 = 17 \to x = 4$.
2. **Quadratic formula**: if the equation is quadratic in `var`, apply $x = \frac{-b \pm \sqrt{b^2-4ac}}{2a}$ symbolically. When the discriminant $b^2 - 4ac < 0$, the roots are complex; the solver emits them using the symbolic imaginary unit `i` (see §6.1). For example, `solve(x^2 + 1 = 0)` returns `{i, -i}` and `solve(x^2 - 2x + 5 = 0)` returns `{1 + 2*i, 1 - 2*i}`. This requires the complex subsystem from Sub-phase 4C.
3. **Polynomial root-finding**: for degree 3–4, try the rational root theorem + synthetic division to reduce to a quadratic.
4. **Inverse function isolation**: for equations like $\sin(x) = 0.5$, apply $\arcsin$ to both sides. Handles the standard trig, exp, and log inversions.
5. **Numeric fallback**: if symbolic methods fail, fall back to `numeric_solve()` and present the result as an approximate decimal.

**Complex roots**: the quadratic path (and, where reducible, the cubic/quartic paths) produce complex roots when real roots don't exist. Complex solutions are honored only when the number mode (Sub-phase 4C) is RECTANGULAR or POLAR; in REAL mode the solver reports "no real solution" instead. The symbolic `i` in a solution tree converts to a numeric `math::Complex` when the result is evaluated or stored.

### 6.8 Symbolic integration (`cas/integrate.hpp`)

This is the hardest CAS operation and the one with the most limited scope. A general-purpose symbolic integrator (like Risch algorithm) is infeasible on this hardware. Instead, implement a **table-based integrator** with a few heuristic methods.

```cpp
namespace math::cas {

// Attempt symbolic integration of expr with respect to var.
// Returns the antiderivative (without +C), or nullptr if unable.
Expr* integrate(const Expr* expr, char var);

// Definite integral: evaluate antiderivative at bounds.
// Returns a numeric result if bounds are numeric.
struct DefIntResult {
    bool has_symbolic;   // Was symbolic antiderivative found?
    Expr* antideriv;     // Symbolic antiderivative (nullable)
    bool has_numeric;    // Was numeric result computed?
    calc_t numeric_val;  // Numeric result of definite integral
};

DefIntResult definite_integrate(const Expr* expr, char var,
                                 const Expr* lower, const Expr* upper);

} // namespace math::cas
```

**Integration table** (direct matches):

| Pattern | Result |
|---------|--------|
| $\int x^n \, dx$ ($n \neq -1$) | $\frac{x^{n+1}}{n+1}$ |
| $\int x^{-1} \, dx$ | $\ln\lvert x\rvert$ |
| $\int e^x \, dx$ | $e^x$ |
| $\int a^x \, dx$ | $\frac{a^x}{\ln a}$ |
| $\int \sin(x) \, dx$ | $-\cos(x)$ |
| $\int \cos(x) \, dx$ | $\sin(x)$ |
| $\int \sec^2(x) \, dx$ | $\tan(x)$ |
| $\int \frac{1}{1+x^2} \, dx$ | $\arctan(x)$ |
| $\int \frac{1}{\sqrt{1-x^2}} \, dx$ | $\arcsin(x)$ |

**Heuristic methods** (applied when table lookup fails):

1. **Linearity**: $\int (af + bg) \, dx = a\int f \, dx + b\int g \, dx$. Always applied first — split sums and pull out constants.
2. **Linear substitution**: if the integrand matches a table entry with $x$ replaced by $(ax + b)$, apply the substitution rule: $\int f(ax+b) \, dx = \frac{1}{a} F(ax+b)$. E.g., $\int \sin(3x+1) \, dx = -\frac{1}{3}\cos(3x+1)$.
3. **Power rule generalization**: $\int u^n \cdot u' \, dx = \frac{u^{n+1}}{n+1}$ where $u$ is a sub-expression of $x$. Check if the integrand has the form $f(u) \cdot u'$ for common $f$.
4. **Integration by parts** (one level only): if the integrand is a product $u \cdot v$, try $\int u \, dv = uv - \int v \, du$ with heuristic choice of $u$ and $dv$ (LIATE rule: Logarithmic, Inverse trig, Algebraic, Trig, Exponential). Attempt this once — don't recurse IBP, as that risks infinite loops and memory exhaustion.

If all methods fail, `integrate()` returns `nullptr` and the UI reports that symbolic integration was unsuccessful, offering to compute a numeric definite integral via Simpson's rule or Gauss-Legendre quadrature instead.

### 6.9 CAS user interface

CAS operations are accessible from the home screen via a menu. The interaction model:

1. Type an expression on the home screen (e.g., `x^3 - 3*x + 2`)
2. Press a CAS key (mapped to a function key or key combo — e.g., `2nd` + `ENTER` for CAS menu)
3. A menu appears:

```
  CAS Operations
  ─────────────────
  1: Simplify
  2: Expand
  3: Factor
  4: Differentiate → d/dx
  5: Integrate → ∫dx
  6: Solve for x
```

4. Result appears pretty-printed in the history, tagged as "symbolic" with a different accent color (e.g., dark blue vs. black for numeric results)

Alternatively, CAS functions are callable directly as expression syntax:

```
diff(x^3, x)          → 3x²
integ(sin(x), x)      → -cos(x)
solve(x^2 - 4 = 0, x) → {-2, 2}
factor(x^2 - 4)       → (x-2)(x+2)
expand((x+1)^3)       → x³ + 3x² + 3x + 1
simplify(2x/4x^2)     → 1/(2x)
```

These CAS functions are registered in the expression parser alongside the numeric functions, but routed to the symbolic engine instead of tinyexpr++. Detection is simple: if the expression contains a CAS keyword (`diff`, `integ`, `solve`, `factor`, `expand`, `simplify`), the entire expression is parsed by the CAS parser and processed symbolically.

---

## 7. Sub-phase 4E: MicroPython programming (weeks 37–39)

### 7.1 Embedding strategy

MicroPython provides an `embed` port specifically designed for hosting MicroPython inside a larger C/C++ application. The firmware includes the MicroPython interpreter as a library, not a standalone runtime.

```cpp
namespace scripting {

class PythonInterpreter {
public:
    // Initialize the MicroPython runtime.
    // heap_size: bytes allocated for the Python heap.
    //   Pico 1: 48 KB from SRAM (leaves ~80KB for app + stack)
    //   Pico 2: 96 KB from SRAM
    // PSRAM is NOT directly usable as Python heap (too slow for
    // GC scanning), but individual large allocations can be proxied.
    bool init(size_t heap_size);
    
    void shutdown();
    
    // Execute a Python string (single statement or block).
    // Returns true if execution succeeded.
    // Output is captured and passed to output_callback.
    bool exec(const char* code);
    
    // Execute a .py file from the SD card.
    bool exec_file(const char* path);
    
    // Register a C function as a Python built-in.
    // Used to expose calculator APIs to scripts.
    void register_function(const char* module, const char* name,
                           void* c_func);
    
    // Check if interpreter is initialized.
    bool is_running() const;
    
    // Set callback for print() output.
    using OutputCallback = void (*)(const char* text);
    void set_output_callback(OutputCallback cb);
    
    // Set callback for input() requests.
    using InputCallback = const char* (*)(const char* prompt);
    void set_input_callback(InputCallback cb);
    
private:
    bool initialized_ = false;
};

} // namespace scripting
```

### 7.2 Calculator API bindings (`calc` Python module)

User scripts import a `calc` module that exposes calculator functionality:

```python
import calc

# Expression evaluation
result = calc.eval("2 + 3 * sin(pi/4)")

# Variables
calc.store("A", 42)
val = calc.recall("A")

# Graphing
calc.plot("sin(x)", color="blue")
calc.plot("cos(x)", color="red")
calc.window(-10, 10, -2, 2)
calc.show_graph()

# CAS operations
d = calc.diff("x^3 - 2*x", "x")    # Returns "3*x^2 - 2"
i = calc.integ("sin(x)", "x")       # Returns "-cos(x)"
s = calc.solve("x^2 - 4 = 0", "x") # Returns ["-2", "2"]
s2 = calc.solve("x^2 + 1 = 0", "x")# Returns ["i", "-i"] (complex-aware)
f = calc.factor("x^2 - 4")          # Returns "(x-2)*(x+2)"

# Complex numbers
z = calc.complex(3, 2)              # 3 + 2i
mag = calc.c_abs(z)                 # 3.606...
ang = calc.c_arg(z)                 # 0.588...
cj = calc.c_conj(z)                 # 3 - 2i

# Graph analysis (numeric)
root = calc.graph_zero("Y1", -5, 5)      # numeric root in bracket
mx   = calc.graph_max("Y1", 0, 10)       # local maximum
area = calc.graph_integral("Y1", 0, 3.14)# numeric definite integral
slope= calc.graph_deriv("Y1", 2.0)       # numeric dy/dx at x=2

# Matrix operations
m = calc.matrix([[1, 2], [3, 4]])
det = calc.det(m)
inv = calc.inverse(m)

# Lists (from Phase 3)
calc.set_list(1, [1, 2, 3, 4, 5])
mean = calc.stat_mean(1)

# Display
calc.clear_screen()
calc.draw_text(10, 10, "Hello from Python!")
calc.draw_line(0, 0, 319, 319, "white")
calc.draw_rect(50, 50, 100, 80, "blue", fill=True)

# Input
key = calc.wait_key()
text = calc.input("Enter value: ")

# File I/O (SD card)
calc.write_file("/picocalc/data.txt", "hello")
content = calc.read_file("/picocalc/data.txt")
```

Each `calc.*` function is a thin C++ wrapper that calls into the existing `math::Engine`, `math::cas::*`, `math::Matrix`, `platform::Display`, `platform::Keyboard`, and `platform::Storage` classes.

### 7.3 Program editor screen

A simple on-device text editor for writing and editing Python scripts:

```
┌──────────────────────────────────┐
│  Edit: program.py                 │
├──────────────────────────────────┤
│  1│ import calc                    │
│  2│                                │
│  3│ for i in range(10):            │
│  4│   y = calc.eval(f"sin({i})")   │
│  5│   calc.draw_text(10, i*20, str │
│  6│ |                              │
│  7│                                │
│                                    │
├──────────────────────────────────┤
│ F1:RUN F2:SAVE F3:LOAD F4:NEW    │
└──────────────────────────────────┘
```

**Features**:

- Line-numbered display with cursor (row + column)
- Arrow key navigation, `ENTER` inserts newline
- `BACKSPACE` / `DEL` work as expected
- Auto-indent on `ENTER` after `:` (match Python expectations)
- `F1` (RUN): save current buffer, execute via `PythonInterpreter::exec_file()`
- `F2` (SAVE): write to `/picocalc/programs/<name>.py`
- `F3` (LOAD): file browser for `/picocalc/programs/`, select a `.py` file
- `F4` (NEW): clear buffer, prompt for filename
- Syntax highlighting is a stretch goal — basic keyword coloring (`import`, `def`, `for`, `if`, `while`, `return`) if time permits

**Output capture**: when a script runs, `print()` output goes to a scrollable output pane that replaces the editor temporarily. Press `ESCAPE` to return to the editor. Errors display with the line number and exception message.

### 7.4 Memory budget for MicroPython

| Component | Pico 1 (SRAM) | Pico 2 (SRAM) |
|-----------|---------------|---------------|
| MicroPython interpreter + stdlib | ~60 KB flash | ~60 KB flash |
| Python heap (GC-managed) | 48 KB | 96 KB |
| C stack for Python calls | 8 KB | 8 KB |
| **Total SRAM impact** | **~56 KB** | **~104 KB** |

On Pico 1, this leaves ~80 KB SRAM for the rest of the application (HAL, UI, math engine, line buffers). This is tight but workable because the CAS pool lives in PSRAM and the framebuffer uses line-buffer rendering. The MicroPython interpreter is only initialized when the user enters the programming screen — it doesn't consume memory when doing normal calculator work.

On Pico 2, it's comfortable — 520 KB SRAM minus ~104 KB for Python minus ~200 KB for framebuffer still leaves ~216 KB.

---

## 8. Task breakdown

Solo developer, part-time (~20 hrs/week).

### Sub-phase 4A: Matrix operations (weeks 26–27)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 4A.1 | `Matrix` view over 2-D `Array`; `MatrixStore` ([A]–[J]) | 6 | Create 10$\times$10 matrix, read/write elements |
| 4A.2 | Matrix arithmetic (add, subtract, multiply, scalar) | 4 | `[A]*[B]` correct for known cases |
| 4A.3 | Transpose, determinant (LU) | 4 | `det(identity(5))` = 1.0; known 3$\times$3 correct |
| 4A.4 | Inverse (Gauss-Jordan), rref | 6 | `[A]*inverse([A])` ≈ identity |
| 4A.5 | Eigenvalues (QR, real, $n \leq 10$) | 6 | Diagonal/symmetric eigenvalues correct |
| 4A.6 | Matrix editor screen | 8 | Enter 3$\times$3, save, reload, verify |
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

### Sub-phase 4D: CAS engine (weeks 32–36)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 4D.1 | `Expr` tree + pool allocator | 6 | Construct, clone, compare trees |
| 4D.2 | CAS expression parser (string → `Expr`) | 10 | `2*x^2 + 3*x - 1` → correct tree |
| 4D.3 | `expr_to_string` | 4 | Round-trip parse→serialize |
| 4D.4 | `expr_to_layout` (natural-math render) | 4 | CAS results pretty-printed |
| 4D.5 | Simplify: identity/annihilation/constant fold | 6 | `x + 0` → `x`; `2 + 3` → `5` |
| 4D.6 | Simplify: like-terms, canonical order, `i^2=-1` | 8 | `3x + 5x` → `8x`; `i*i` → `-1` |
| 4D.7 | Simplify: fraction reduction | 4 | `2x/(4x^2)` → `1/(2x)` |
| 4D.8 | Symbolic differentiation (all rules) | 8 | `diff(x^3*sin(x))` correct |
| 4D.9 | Higher-order differentiation | 2 | `diff(diff(x^4))` → `12x^2` |
| 4D.10 | Expand (distribute, binomial) | 6 | `(x+1)^3` expanded |
| 4D.11 | Factor (common, diff-of-squares, quadratic) | 8 | `x^2 - 4` → `(x-2)(x+2)` |
| 4D.12 | Factor (rational root, degree 3–4) | 6 | `x^3-6x^2+11x-6` → 3 factors |
| 4D.13 | Solve: linear + quadratic (real) | 6 | `x^2-5x+6=0` → `{2,3}` |
| 4D.14 | Solve: complex roots (needs 4C) | 4 | `x^2+1=0` → `{i,-i}` |
| 4D.15 | Solve: inverse-function isolation | 4 | `sin(x)=1/2` → `pi/6` |
| 4D.16 | Integrate: table + linearity | 8 | `integ(x^3)` → `x^4/4` |
| 4D.17 | Integrate: linear substitution, power rule | 6 | `integ(sin(3x+1))` correct |
| 4D.18 | Integrate: one-level integration by parts | 6 | `integ(x*e^x)` correct |
| 4D.19 | Definite integration (symbolic + numeric fallback) | 4 | $\int_0^\pi \sin x\,dx$ = 2 |
| 4D.20 | CAS menu UI + expression routing | 4 | CAS ops from home-screen menu |
| 4D.21 | CAS function syntax (`diff()`, `integ()`, etc.) | 4 | Callable inline from input |
| 4D.22 | Stress testing + edge cases | 6 | Nested exprs, complex roots, guards |
| | **Subtotal** | **~124 hrs** | |

### Sub-phase 4E: MicroPython programming (weeks 37–39)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 4E.1 | Build MicroPython embed lib (both boards) | 8 | `print(1+1)` → "2" on serial |
| 4E.2 | `PythonInterpreter` wrapper | 4 | Init/exec/shutdown clean |
| 4E.3 | `calc` module: eval, variables, store/recall | 6 | `calc.eval("sin(pi/4)")` correct |
| 4E.4 | `calc` module: CAS bindings (incl. complex solve) | 4 | `calc.solve("x^2+1=0","x")` → `["i","-i"]` |
| 4E.5 | `calc` module: complex bindings | 3 | `calc.c_abs(calc.complex(3,4))` = 5 |
| 4E.6 | `calc` module: graph-analysis bindings | 4 | `calc.graph_zero`, `graph_integral` work |
| 4E.7 | `calc` module: matrix bindings | 3 | Create/multiply/invert from Python |
| 4E.8 | `calc` module: display primitives | 4 | Script draws graphics |
| 4E.9 | `calc` module: keyboard input | 3 | Read keys, text input |
| 4E.10 | `calc` module: file I/O | 2 | Read/write SD files |
| 4E.11 | Program editor screen | 10 | Write a 20-line script on-device |
| 4E.12 | Execution: output capture, error display | 4 | print output + line-numbered errors |
| 4E.13 | Load/save scripts to SD | 3 | Save, power cycle, reload, run |
| 4E.14 | Memory management: lazy init, cleanup | 3 | Heap freed on leaving program screen |
| | **Subtotal** | **~61 hrs** | |

### Summary

| Sub-phase | Weeks | Hours | Deliverable |
|-----------|-------|-------|-------------|
| 4A: Matrix operations | 26–27 | ~46 | Matrix editor, arithmetic, rref/det/inverse/eigen, numeric solver |
| 4B: Graph analysis (CALC) | 28–29 | ~51 | value/zero/min/max/intersect/dy-dx/fnInt, interactive on graph |
| 4C: Complex numbers | 30–31 | ~38 | Complex type, functions, a+bi/polar mode |
| 4D: CAS engine | 32–36 | ~124 | Symbolic diff/simplify/factor/solve (complex)/integrate |
| 4E: MicroPython | 37–39 | ~61 | Interpreter, `calc` module, editor, SD scripts |
| **Total Phase 4** | **~14 weeks** | **~320 hrs** | |

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

Complex arithmetic is 2–6$\times$ the cost of real (a complex multiply is 4 real multiplies + 2 adds). Since complex evaluation is confined to the home-screen path (never graphing), this is imperceptible for single expressions. Complex elementary functions (`c_exp`, `c_ln`) cost ~2–3$\times$ their real counterparts.

### CAS operation benchmarks (estimated)

| Operation | Typical size | Pico 1 | Pico 2 |
|-----------|--------------|--------|--------|
| Simplify (basic polynomial) | ~30 nodes | 1–5 ms | <1 ms |
| Differentiate | ~30 → ~80 nodes | 2–10 ms | <2 ms |
| Factor (degree 3) | ~15 nodes | 5–20 ms | 1–5 ms |
| Expand $(x+1)^{10}$ | ~5 → ~55 nodes | 10–50 ms | 2–10 ms |
| Solve (quadratic, incl. complex) | ~15 nodes | 2–10 ms | <2 ms |
| Integrate (table match) | ~20 nodes | 1–5 ms | <1 ms |
| Integrate (IBP, one level) | ~30 nodes | 10–50 ms | 2–10 ms |
| Simplify (200+ nodes, multi-pass) | ~200 nodes | 50–500 ms | 10–100 ms |

Worst-case complex simplification approaches ~0.5 s on Pico 1 — perceptible but acceptable (the TI-89 often took longer at 10–12 MHz).

### Matrix benchmarks (estimated)

| Operation | Size | Pico 1 | Pico 2 |
|-----------|------|--------|--------|
| Multiply | 10$\times$10 | ~10 ms | ~2 ms |
| Determinant | 10$\times$10 | ~5 ms | ~1 ms |
| Inverse | 10$\times$10 | ~15 ms | ~3 ms |
| Eigenvalues (QR) | 10$\times$10 | ~200 ms | ~40 ms |
| Inverse | 50$\times$50 | ~8 s | ~1.5 s |

Large matrices ($>$ 20$\times$20) are slow on Pico 1 — acceptable, as these are uncommon in handheld use.

---

## 10. Risks and mitigations

### Risk 1: CAS simplification infinite loops

Poorly ordered rewriting rules can cycle. **Mitigation**: hard cap on simplification passes (50 iterations); canonical ordering of commutative operands for deterministic matching; test suite of tricky expressions ($\frac{x}{x}$, $0^0$, $(x+y)^{20}$, `i^4`).

### Risk 2: CAS pool memory exhaustion

Expanding $(x+y+z)^{15}$ produces thousands of nodes. **Mitigation**: check pool capacity before expansion; abort with an error above 80% capacity; pool can grow into PSRAM (up to 512 KB) for specific operations.

### Risk 3: Complex evaluation slowing the hot path

If the numeric evaluator became complex-by-default, graphing would slow 2–6$\times$. **Mitigation**: dual entry points — `evaluate_real()` (fast, used by graphing/tables/stats) and `evaluate_complex()` (home screen only). Graphing never touches the complex path. This is a firm architectural rule, documented in `AGENTS.md`.

### Risk 4: Numeric integration accuracy vs. speed (4B fnInt)

Adaptive Gauss-Kronrod can over-subdivide on oscillatory or singular integrands, becoming slow. **Mitigation**: cap subdivision depth; report the error estimate alongside the result; fall back to a coarser fixed rule if the depth cap is hit rather than hanging.

### Risk 5: CAS ↔ complex representation seams

The symbolic `i` (a reserved VAR) and the numeric `Complex` type are two representations of the same concept; conversion bugs are likely. **Mitigation**: centralize conversion in one place (`cas/solve.cpp` helper functions `complex_to_expr` / `expr_to_complex`); unit-test the round trip; keep the rule $i^2 = -1$ in exactly one simplification rule.

### Risk 6: MicroPython heap too small on Pico 1

48 KB Python heap limits script complexity. **Mitigation**: document the limit; store large data in `calc`-module lists/matrices (PSRAM, outside the Python heap); Pico 2 doubles the heap.

### Risk 7: Building MicroPython for both RP2040 and RP2350

**Mitigation**: MicroPython officially supports both. Build the embed lib as a separate CMake external project per board target. Test the embed build early (4E.1) before writing bindings.

---

## 11. Open questions for Phase 4

| # | Question | Options | When |
|---|----------|---------|------|
| P4-1 | Store CAS results in the same history as numeric results? | Unified vs. separate CAS history | Week 32 |
| P4-2 | Represent symbolic results in variables (e.g., `A = x^2+1`)? | Allow expression-valued variables vs. numeric-only | Week 32 |
| P4-3 | Implicit multiplication globally, or CAS-mode only? | CAS-only (safer) vs. global (natural) | Week 32 |
| P4-4 | Python heap: static at boot or lazy on first use? | Lazy saves ~56 KB when unused | Week 37 |
| P4-5 | `calc.plot()` from Python: immediate graph switch or buffered? | Immediate vs. buffered | Week 38 |
| P4-6 | CALC intersect with >2 curves: pick two via cursor, or list? | Cursor-cycle vs. explicit picker | Week 28, task 4B.5 |
| P4-7 | Complex eigenvalues for matrices (4A produces real only)? | Add conjugate-pair support in 4C, or defer | Week 30, task 4C — likely defer |
| P4-8 | Polar `fnInt`: area ($\frac{1}{2}\int r^2 d\theta$) only, or also arc length? | Area matches TI; arc length is a nice extra | Week 29, task 4B.8 |
| P4-9 | Number-mode default on first boot: REAL or RECTANGULAR? | REAL matches TI default; RECT is friendlier | Week 31, task 4C.9 |

---

## 12. Reconciliation notes

- **Matrix on `Array`** (§3): Phase 4's `Matrix` is now a linear-algebra view over Phase 3's 2-D `Array`, not a standalone storage class. This resolves the reconciliation flagged in Phase 3 §10. Storage, allocation, and element access come from `Array`; `Matrix` adds only linear-algebra methods.
- **Numeric vs. symbolic calculus**: 4B's `dy/dx` and `fnInt` are numeric and live on the graph screen; 4D's `differentiate` and `integrate` are symbolic and live in the CAS menu. They are complementary, not duplicative — documented in §6's intro.
- **Complex before CAS**: 4C precedes 4D so CAS `solve` can return complex roots. The symbolic `i` and numeric `Complex` share a conversion layer (§5.5, Risk 5).
- **Dual evaluation path**: the complex subsystem must not slow graphing. `evaluate_real()` stays the hot path; `evaluate_complex()` is home-screen only (§5.2, Risk 3).

---

## 13. References

1. Phase 1 spec — [phase1-spec.md](phase1-spec.md)
2. Phase 2 spec — [phase2-spec.md](phase2-spec.md)
3. Phase 3 spec (Array primitive, stat-plot layer) — [phase3-spec.md](phase3-spec.md)
4. DB48X source (CAS reference) — https://github.com/c3d/db48x
5. DB48X symbolic operations — https://github.com/c3d/db48x/releases/tag/v0.8.1
6. KhiCAS / Giac (full CAS on calculators) — https://github.com/nwagyu/khicas
7. MicroPython embed port — https://docs.micropython.org/en/latest/develop/embed.html
8. MicroPython RP2040/RP2350 support — https://micropython.org/download/RPI_PICO/
9. tinyexpr++ (numeric evaluation) — https://github.com/Blake-Madden/tinyexpr-plus-plus
10. Brent's method (extrema) — Brent, "Algorithms for Minimization without Derivatives"
11. Gauss-Kronrod quadrature — Piessens et al., QUADPACK
12. NIST DLMF (integration tables, special functions) — https://dlmf.nist.gov/
13. TI-84 Plus CALC menu guidebook — https://education.ti.com/en/guidebook/details/en/6152F7C2E0B9491482D4CF5C3EEB6EB1/84plce
14. QR algorithm for eigenvalues — Golub & Van Loan, "Matrix Computations"
