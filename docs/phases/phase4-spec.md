# Phase 4 Spec: CAS, Matrix Operations & Programming

**Prerequisite phases**: Phase 1 (HAL + calculator + graphing), Phase 2 (table view, parametric/polar), Phase 3 (statistics & data lists).

**Scope**: Add symbolic math (CAS), matrix operations, a numeric equation solver, and a MicroPython programming environment. This phase transforms the calculator from a numerical tool into an algebraic one and makes it programmable.

**End state**: the calculator can differentiate, simplify, factor, and solve expressions symbolically; manipulate matrices; and run user-written Python programs from the SD card with access to all calculator functions.

---

## 1. Overview and phasing within Phase 4

Phase 4 is the largest and most varied phase. It splits into three sub-phases that can be developed somewhat independently since they occupy different parts of the codebase:

| Sub-phase | Weeks | Content |
|-----------|-------|---------|
| 4A: Matrix operations | 16–17 | Matrix editor, arithmetic, det, inverse, rref, eigenvalues |
| 4B: CAS engine | 18–22 | Symbolic expression trees, differentiation, simplification, factoring, equation solving, basic integration |
| 4C: MicroPython | 23–25 | Embedded interpreter, calculator API bindings, on-device editor, SD card script execution |

**Total estimated effort**: ~10 weeks part-time (~200 hours).

CAS is the dominant effort. It is also the highest-risk component — symbolic math has a long tail of edge cases. The strategy is to implement a minimal-but-correct core and extend incrementally. "Correct on a narrow domain" beats "broken on a wide domain."

---

## 2. New source files

Additions to the project tree from Phase 1:

```
src/
├── math/
│   ├── engine.hpp / .cpp          # Extended: symbolic evaluation mode
│   ├── expr_tree.hpp / .cpp       # NEW: symbolic expression tree (AST)
│   ├── cas/
│   │   ├── simplify.hpp / .cpp    # Algebraic simplification rules
│   │   ├── derivative.hpp / .cpp  # Symbolic differentiation
│   │   ├── integrate.hpp / .cpp   # Symbolic integration (table-based)
│   │   ├── factor.hpp / .cpp      # Polynomial factoring
│   │   ├── solve.hpp / .cpp       # Symbolic equation solver (isolation)
│   │   ├── expand.hpp / .cpp      # Distribution / expansion
│   │   └── rules.hpp             # Shared rewriting utilities
│   ├── matrix.hpp / .cpp          # Dense matrix class + operations
│   ├── numeric_solve.hpp / .cpp   # Numeric root-finder (bisection + Newton)
│   └── types.hpp                  # Updated: matrix type, symbolic expr type
├── apps/
│   ├── matrix_editor.hpp / .cpp   # Matrix editor screen
│   ├── solver_screen.hpp / .cpp   # Numeric & symbolic solver UI
│   ├── cas_screen.hpp / .cpp      # CAS worksheet / symbolic mode
│   └── program_screen.hpp / .cpp  # MicroPython editor + runner
├── scripting/
│   ├── micropython_embed.hpp/.cpp # MicroPython interpreter wrapper
│   ├── calc_module.hpp / .cpp     # Python 'calc' module (C++ bindings)
│   └── script_runner.hpp / .cpp   # Load + execute .py from SD card
```

---

## 3. Sub-phase 4A: Matrix operations (weeks 16–17)

### 3.1 Matrix data structure

```cpp
namespace math {

class Matrix {
public:
    Matrix() = default;
    Matrix(int rows, int cols);              // Zero-initialized
    Matrix(int rows, int cols, calc_t* data); // From existing data
    
    int rows() const { return rows_; }
    int cols() const { return cols_; }
    
    calc_t& at(int r, int c);
    calc_t  at(int r, int c) const;
    calc_t& operator()(int r, int c) { return at(r, c); }
    
    // Storage: row-major, allocated from PSRAM for large matrices,
    // SRAM for small ones (threshold: 16x16 = 1KB for floats)
    calc_t* data() { return data_; }
    
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
    calc_t determinant() const;         // LU-based for n>3, direct for 2x2/3x3
    Matrix inverse() const;             // Via Gauss-Jordan
    Matrix rref() const;                // Reduced row echelon form
    Matrix ref() const;                 // Row echelon form (no back-sub)
    int rank() const;                   // Via rref
    
    // Eigenvalues (real only, QR algorithm for small matrices)
    // Returns eigenvalues in a column vector. Limited to n <= 10.
    Matrix eigenvalues() const;
    
    // Identity and zero constructors
    static Matrix identity(int n);
    static Matrix zeros(int rows, int cols);

private:
    int rows_ = 0;
    int cols_ = 0;
    calc_t* data_ = nullptr;  // Owned, allocated via PSRAM or SRAM pool
};

} // namespace math
```

**Memory strategy**: the TI-83 supports 10 matrix variables ($[A]$ through $[J]$) of up to 99$\times$99. A 99$\times$99 `double` matrix is ~76 KB. These live in PSRAM. The working set for a single matrix operation (e.g., inverse needs the original plus a scratch copy) temporarily doubles this. With 8 MB PSRAM and the framebuffer taking ~200 KB, there's ample room — even ten full-size matrices use under 1 MB.

For Pico 1's SRAM, small matrices (up to ~16$\times$16, ~2 KB) are allocated on SRAM for speed. Anything larger goes to PSRAM, with operations reading/writing through the SPI PSRAM interface. This is slower but avoidable for typical classroom-scale matrices (3$\times$3 to 10$\times$10).

### 3.2 Matrix editor screen

A spreadsheet-like grid editor:

```
┌──────────────────────────────────┐
│  Matrix [A]  3×3                  │
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

- Arrow keys navigate the grid, highlighting the active cell
- Typing enters edit mode for the current cell; `ENTER` confirms and advances to the next cell (right, then down)
- `F2` (NAME): select which matrix variable $[A]$–$[J]$ to edit
- `F3` (DIM): change dimensions (prompts for rows and cols, existing data preserved or truncated)
- `F4` (OPS): context menu with: Transpose, Inverse, Determinant, RREF, Fill (constant or sequence), Augment
- Matrices persist to SD card as `/picocalc/matrices/A.dat` through `J.dat` (binary format: 4 bytes rows, 4 bytes cols, then row-major `double` array)

### 3.3 Matrix functions accessible from home screen

From the home screen, matrices are referenced by name in expressions. The engine recognizes matrix literals and operations:

```
det([A])              → scalar result
inverse([A])          → matrix result, displayed in matrix view
rref([A])             → matrix result
[A] * [B]             → matrix multiplication
[A] + [B]             → matrix addition
[A]^-1                → inverse (alias)
[A]^T                 → transpose
dim([A])              → {rows, cols} as a list
[A](2,3)              → element at row 2, col 3
identity(4)           → 4×4 identity matrix
augment([A],[B])       → horizontal concatenation
```

Matrix results that don't fit a single display line push the user to the matrix viewer (a scrollable grid showing the result, read-only).

### 3.4 Numeric equation solver

An enhancement to the existing `math::Engine`:

```cpp
namespace math {

struct SolveResult {
    bool converged;
    calc_t root;
    int iterations;
    calc_t residual;     // |f(root)|
};

// Solve f(x) = 0 for x in [a, b] using bisection + Newton refinement.
// Requires f(a) and f(b) to have opposite signs for bisection.
// Falls back to Newton from midpoint if signs are same.
SolveResult numeric_solve(const char* expr, char var,
                          calc_t lower, calc_t upper,
                          calc_t tolerance = 1e-10,
                          int max_iter = 100);

// Solve f(x) = g(x) by solving f(x) - g(x) = 0.
SolveResult numeric_solve_equation(const char* lhs, const char* rhs,
                                    char var,
                                    calc_t lower, calc_t upper);

} // namespace math
```

The solver screen prompts for: the equation (e.g., `x^3 - 2*x - 5 = 0`), the variable to solve for, and an initial guess or bounds. It displays the root, the residual, and the number of iterations.

---

## 4. Sub-phase 4B: CAS engine (weeks 18–22)

This is the core of Phase 4. The CAS operates on a new symbolic expression tree (`ExprTree`) that is distinct from the numerical evaluation path. The numerical evaluator (tinyexpr++) remains for graphing and immediate numeric results. The CAS is invoked explicitly — when the user types an expression and presses a CAS-specific key or selects a CAS operation from a menu.

### 4.1 Symbolic expression tree

```cpp
namespace math {

// Node types for the symbolic expression tree.
enum class ExprType : uint8_t {
    // Atoms
    NUM,        // Numeric literal: 3, -2.5, pi
    VAR,        // Variable: x, y, t, a, b
    
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

### 4.2 Memory management

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

### 4.3 Expression parser (string → `Expr` tree)

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

### 4.4 Simplification (`cas/simplify.hpp`)

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

### 4.5 Symbolic differentiation (`cas/derivative.hpp`)

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

### 4.6 Expansion and factoring (`cas/expand.hpp`, `cas/factor.hpp`)

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

### 4.7 Symbolic equation solving (`cas/solve.hpp`)

```cpp
namespace math::cas {

// Solve an equation for variable var.
// Returns a list of solutions (as Expr* array).
// max_solutions caps the output.
struct SolveResult {
    Expr* solutions[8];
    int count;
    bool exact;    // true if all solutions are symbolic/exact
};

SolveResult solve(const Expr* equation, char var);

} // namespace math::cas
```

Solving strategy (in order of attempt):

1. **Linear isolation**: if the equation is linear in `var`, algebraically isolate it. E.g., $3x + 5 = 17 \to x = 4$.
2. **Quadratic formula**: if the equation is quadratic in `var`, apply $x = \frac{-b \pm \sqrt{b^2-4ac}}{2a}$ symbolically.
3. **Polynomial root-finding**: for degree 3–4, try the rational root theorem + synthetic division to reduce to a quadratic.
4. **Inverse function isolation**: for equations like $\sin(x) = 0.5$, apply $\arcsin$ to both sides. Handles the standard trig, exp, and log inversions.
5. **Numeric fallback**: if symbolic methods fail, fall back to `numeric_solve()` and present the result as an approximate decimal.

### 4.8 Symbolic integration (`cas/integrate.hpp`)

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

### 4.9 CAS user interface

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

## 5. Sub-phase 4C: MicroPython programming (weeks 23–25)

### 5.1 Embedding strategy

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

### 5.2 Calculator API bindings (`calc` Python module)

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
f = calc.factor("x^2 - 4")          # Returns "(x-2)*(x+2)"

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

### 5.3 Program editor screen

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

### 5.4 Memory budget for MicroPython

| Component | Pico 1 (SRAM) | Pico 2 (SRAM) |
|-----------|---------------|---------------|
| MicroPython interpreter + stdlib | ~60 KB flash | ~60 KB flash |
| Python heap (GC-managed) | 48 KB | 96 KB |
| C stack for Python calls | 8 KB | 8 KB |
| **Total SRAM impact** | **~56 KB** | **~104 KB** |

On Pico 1, this leaves ~80 KB SRAM for the rest of the application (HAL, UI, math engine, line buffers). This is tight but workable because the CAS pool lives in PSRAM and the framebuffer uses line-buffer rendering. The MicroPython interpreter is only initialized when the user enters the programming screen — it doesn't consume memory when doing normal calculator work.

On Pico 2, it's comfortable — 520 KB SRAM minus ~104 KB for Python minus ~200 KB for framebuffer still leaves ~216 KB.

---

## 6. Task breakdown

### Sub-phase 4A: Matrix operations (weeks 16–17)

| # | Task | Est. hours | Acceptance criteria |
|---|------|-----------|-------------------|
| 4A.1 | Implement `Matrix` class (constructors, element access, PSRAM backing) | 6 | Create 10$\times$10 matrix, read/write elements |
| 4A.2 | Matrix arithmetic (add, subtract, multiply, scalar multiply) | 4 | `[A]*[B]` produces correct result for known test cases |
| 4A.3 | Transpose, determinant (LU decomposition) | 4 | `det(identity(5))` returns 1.0, known 3$\times$3 determinants correct |
| 4A.4 | Inverse (Gauss-Jordan), rref | 6 | `[A] * inverse([A])` $\approx$ identity, rref matches known results |
| 4A.5 | Eigenvalues (QR algorithm, $n \leq 10$) | 6 | Eigenvalues of known matrices (diagonal, symmetric) correct |
| 4A.6 | Matrix editor screen (grid UI, cell editing, navigation) | 8 | Enter a 3$\times$3 matrix, save, reload, verify |
| 4A.7 | Matrix functions in home screen expression parser | 4 | `det([A])` evaluates from home screen |
| 4A.8 | Matrix persistence to SD card | 2 | Matrices survive power cycle |
| 4A.9 | Numeric equation solver + solver screen | 6 | Solve $x^3 - 2x - 5 = 0$ yields $x \approx 2.0946$ |
| | **Subtotal** | **~46 hrs** | |

### Sub-phase 4B: CAS engine (weeks 18–22)

| # | Task | Est. hours | Acceptance criteria |
|---|------|-----------|-------------------|
| 4B.1 | `Expr` tree data structure + pool allocator | 6 | Construct, clone, compare expression trees |
| 4B.2 | CAS expression parser (string → `Expr`) | 10 | `parse_expr("2*x^2 + 3*x - 1")` produces correct tree |
| 4B.3 | `expr_to_string()` (serialize back to readable form) | 4 | Round-trip: parse → serialize matches input (modulo formatting) |
| 4B.4 | `expr_to_layout()` (CAS results into natural math renderer) | 4 | CAS results display as pretty-printed 2D math |
| 4B.5 | Simplification: identity/annihilation/constant folding | 6 | `simplify("x + 0")` → `x`, `simplify("2 + 3")` → `5` |
| 4B.6 | Simplification: like-term collection, canonical ordering | 8 | `simplify("3*x + 5*x")` → `8*x` |
| 4B.7 | Simplification: fraction reduction | 4 | `simplify("2*x / (4*x^2)")` → `1/(2*x)` |
| 4B.8 | Symbolic differentiation (all rules from table) | 8 | `diff("x^3*sin(x)", "x")` → `3*x^2*sin(x) + x^3*cos(x)` |
| 4B.9 | Higher-order differentiation | 2 | `diff(diff("x^4", "x"), "x")` → `12*x^2` |
| 4B.10 | Expand (distribute multiplication, binomial expansion) | 6 | `expand("(x+1)^3")` → `x^3 + 3*x^2 + 3*x + 1` |
| 4B.11 | Factor (common factor, difference of squares, quadratic) | 8 | `factor("x^2 - 4")` → `(x-2)*(x+2)` |
| 4B.12 | Factor (rational root theorem, degree 3–4) | 6 | `factor("x^3 - 6*x^2 + 11*x - 6")` → `(x-1)*(x-2)*(x-3)` |
| 4B.13 | Symbolic equation solving (linear + quadratic) | 6 | `solve("x^2 - 5*x + 6 = 0", "x")` → `{2, 3}` |
| 4B.14 | Symbolic equation solving (inverse function isolation) | 4 | `solve("sin(x) = 1/2", "x")` → `pi/6` |
| 4B.15 | Symbolic integration (table lookup + linearity) | 8 | `integ("x^3", "x")` → `x^4/4` |
| 4B.16 | Symbolic integration (linear substitution, power rule) | 6 | `integ("sin(3*x+1)", "x")` → `-cos(3*x+1)/3` |
| 4B.17 | Symbolic integration (one-level integration by parts) | 6 | `integ("x*exp(x)", "x")` → `x*exp(x) - exp(x)` |
| 4B.18 | Definite integration (evaluate antideriv at bounds + numeric fallback) | 4 | $\int_0^\pi \sin(x) \, dx = 2.0$ |
| 4B.19 | CAS menu UI + expression routing | 4 | CAS operations accessible from home screen menu |
| 4B.20 | CAS function syntax (`diff()`, `integ()`, etc.) in parser | 4 | Callable as inline functions from expression input |
| 4B.21 | Stress testing + edge cases | 6 | Nested expressions, chain rule depth, zero-division guards |
| | **Subtotal** | **~118 hrs** | |

### Sub-phase 4C: MicroPython programming (weeks 23–25)

| # | Task | Est. hours | Acceptance criteria |
|---|------|-----------|-------------------|
| 4C.1 | Build MicroPython embed library for Pico SDK (both boards) | 8 | `mp_embed_exec_str("print(1+1)")` produces "2" on serial |
| 4C.2 | `PythonInterpreter` wrapper class | 4 | Init, exec, shutdown cycle works cleanly |
| 4C.3 | `calc` module: expression eval, variables, store/recall | 6 | `calc.eval("sin(pi/4)")` returns correct value |
| 4C.4 | `calc` module: CAS bindings (diff, integ, solve, factor) | 4 | `calc.diff("x^2", "x")` returns `"2*x"` as string |
| 4C.5 | `calc` module: matrix bindings | 3 | Create, multiply, invert matrices from Python |
| 4C.6 | `calc` module: display primitives (text, lines, rects) | 4 | Python script draws graphics on screen |
| 4C.7 | `calc` module: keyboard input (`wait_key`, `input`) | 3 | Script can read key presses and text input |
| 4C.8 | `calc` module: file I/O (SD card read/write) | 2 | Read/write text files from Python |
| 4C.9 | Program editor screen (text editing, line numbers, cursor) | 10 | Write a 20-line script on-device |
| 4C.10 | Script execution: output capture, error display | 4 | `print()` output visible, exceptions show line number |
| 4C.11 | Script load/save to SD card (`/picocalc/programs/`) | 3 | Save script, power cycle, reload and run |
| 4C.12 | Memory management: lazy init, cleanup on exit | 3 | Python heap freed when leaving program screen |
| | **Subtotal** | **~54 hrs** | |

### Summary

| Sub-phase | Weeks | Hours | Deliverable |
|-----------|-------|-------|-------------|
| 4A: Matrix operations | 16–17 | ~46 | Matrix editor, arithmetic, rref, det, inverse, eigenvalues, numeric solver |
| 4B: CAS engine | 18–22 | ~118 | Symbolic differentiation, simplification, factoring, solving, integration |
| 4C: MicroPython | 23–25 | ~54 | Embedded interpreter, `calc` module, on-device editor, SD card scripts |
| **Total Phase 4** | **~10 weeks** | **~218 hrs** | |

---

## 7. Performance expectations

### CAS operation benchmarks (estimated)

These estimates are based on tree traversal operations on a 133 MHz Cortex-M0+ (Pico 1). Each `Expr` node visit involves a pointer dereference + branch + possibly a floating-point comparison. Assume ~50–100 cycles per node visit on average.

| Operation | Typical expression size | Estimated time (Pico 1) | Estimated time (Pico 2) |
|-----------|------------------------|------------------------|------------------------|
| Simplify (basic polynomial) | ~30 nodes | 1–5 ms | <1 ms |
| Differentiate (single variable) | ~30 nodes → ~80 nodes output | 2–10 ms | <2 ms |
| Factor (degree 3 polynomial) | ~15 nodes, rational root test | 5–20 ms | 1–5 ms |
| Expand $(x+1)^{10}$ | ~5 nodes → ~55 nodes output | 10–50 ms | 2–10 ms |
| Solve (quadratic) | ~15 nodes | 2–10 ms | <2 ms |
| Integrate (table match) | ~20 nodes | 1–5 ms | <1 ms |
| Integrate (IBP, one level) | ~30 nodes | 10–50 ms | 2–10 ms |
| Simplify (complex nested, 200+ nodes) | ~200 nodes, multi-pass | 50–500 ms | 10–100 ms |

All of these are well within interactive response times. The worst case (complex simplification) might approach half a second on Pico 1, which is perceptible but acceptable — the TI-89 frequently took longer for equivalent operations at 10–12 MHz.

### Matrix benchmarks (estimated)

| Operation | Size | Estimated time (Pico 1) | Estimated time (Pico 2) |
|-----------|------|------------------------|------------------------|
| Multiply | 10$\times$10 | ~10 ms | ~2 ms |
| Determinant | 10$\times$10 | ~5 ms | ~1 ms |
| Inverse | 10$\times$10 | ~15 ms | ~3 ms |
| RREF | 10$\times$10 | ~10 ms | ~2 ms |
| Eigenvalues (QR) | 10$\times$10 | ~200 ms | ~40 ms |
| Multiply | 50$\times$50 | ~5 s | ~1 s |
| Inverse | 50$\times$50 | ~8 s | ~1.5 s |

Large matrices ($>$ 20$\times$20) will feel slow on Pico 1. This is acceptable — these are uncommon in handheld calculator usage and the TI-83 itself can't handle them at all (its matrix limit is practically ~10$\times$10 before becoming unusable).

---

## 8. Risks and mitigations

### Risk 1: CAS simplification infinite loops

**Problem**: poorly ordered rewriting rules can cycle (rule A produces a form that triggers rule B which produces a form that triggers rule A).

**Mitigation**: hard cap on simplification passes (50 iterations). Canonical ordering of commutative operands ensures deterministic rule matching. Test with a suite of known-tricky expressions (e.g., $\frac{x}{x}$, $0^0$, $\frac{0}{0}$, $(x+y)^{20}$).

### Risk 2: CAS pool memory exhaustion

**Problem**: expanding $(x + y + z)^{15}$ produces thousands of nodes, overflowing the 64 KB pool.

**Mitigation**: check pool capacity before expansion. If an operation would exceed 80% of pool capacity, abort early and return an error message rather than corrupting memory. The pool size can be increased (up to 512 KB from PSRAM) for specific operations via a temporary reallocation.

### Risk 3: MicroPython heap too small for non-trivial programs

**Problem**: 48 KB Python heap on Pico 1 limits what scripts can do (large lists, string manipulation, etc.).

**Mitigation**: document the limitation clearly. For Pico 1, this is a known tradeoff — the interpreter exists for small utility scripts and simple automation, not for running complex programs. Pico 2 doubles the heap. For scripts that need large data, use the `calc` module to store data in matrix variables or lists (which live in PSRAM, outside the Python heap) rather than in Python objects.

### Risk 4: CAS integration with natural math renderer

**Problem**: CAS output expressions can be deeply nested and may produce layout trees that don't fit the screen width.

**Mitigation**: the layout renderer already handles clipping and horizontal scrolling (from Phase 1). For very wide expressions, add a "horizontal scroll" mode where left/right arrow keys pan the expression view. Also, long expressions can be broken at `+`/`-` boundaries across multiple lines, similar to how the TI-89 displays multi-line symbolic results.

### Risk 5: Building MicroPython for both RP2040 and RP2350

**Problem**: MicroPython's embed port may have build quirks specific to each ARM architecture (Cortex-M0+ vs Cortex-M33).

**Mitigation**: MicroPython officially supports both RP2040 and RP2350. The `micropython-embed` build produces a static library (`.a`) that links into the firmware. Build the library as a separate CMake external project with the appropriate compiler flags for each board target. Test early — build the embed library in week 23 before writing any bindings.

---

## 9. Open questions for Phase 4

| Question | Options | When to decide |
|----------|---------|---------------|
| Should CAS results be stored in history alongside numeric results? | Yes (unified history) vs. separate CAS history | Week 18, during CAS UI design |
| How to represent symbolic results in variables? `A = x^2 + 1` (symbolic) vs. `A = 5` (numeric only) | Allow both (expression objects in variable slots) vs. numeric-only variables | Week 18 |
| Implicit multiplication in CAS mode only, or globally? | CAS-only (safer) vs. global (more natural) | Week 18, during CAS parser |
| Python heap: static allocation at boot or lazy allocation on first use? | Lazy saves ~56 KB SRAM when not using Python; static simplifies code | Week 23 |
| Should `calc.plot()` from Python immediately switch to graph screen, or buffer for later? | Immediate (simpler) vs. buffered (more flexible) | Week 24 |

---

## 10. References

1. DB48X source (CAS reference) — https://github.com/c3d/db48x
2. DB48X symbolic operations (differentiation, integration, equation isolation) — https://github.com/c3d/db48x/releases/tag/v0.8.1
3. KhiCAS / Giac (full CAS on NumWorks) — https://github.com/nwagyu/khicas
4. Giac/Xcas CAS documentation — https://xcas.univ-grenoble-alpes.fr/en.html
5. MicroPython embed port documentation — https://docs.micropython.org/en/latest/develop/embed.html
6. MicroPython RP2040/RP2350 support — https://micropython.org/download/RPI_PICO/
7. tinyexpr++ (numerical evaluation, retained from Phase 1) — https://github.com/codeplea/tinyexpr
8. NIST Digital Library of Mathematical Functions (integration tables) — https://dlmf.nist.gov/
9. Symbolic differentiation algorithm overview — Aho, Sethi & Ullman, "Compilers: Principles, Techniques, and Tools" (expression tree transformations)
10. QR algorithm for eigenvalues — Golub & Van Loan, "Matrix Computations"
