# Phase 5 Spec: CAS (Symbolic Math Engine)

**Prerequisite phases**: Phase 1 (HAL + calculator + graphing), Phase 3
(the shared `Array` primitive), Phase 4A–4C (matrices, graph analysis,
and — critically — the `Complex` type and number-mode subsystem that
CAS's complex-aware solving depends on).

**Scope**: A symbolic math engine — simplify, expand, factor, differentiate,
solve (including complex roots), a bounded form of symbolic integration,
and exact-value (surd/pi) display for home-screen results (§10.1). This
phase transforms the calculator from a numerical/algebraic tool (Phase
4A–4C) into one that manipulates expressions symbolically, the capability
that separates a TI-84+ from a TI-Nspire CAS.

**Status**: Specced, not started. Split out of Phase 4 on 2026-07-21 —
this content was originally Phase 4 sub-phase 4D (weeks 32–36 of that
phase's plan); it's promoted to its own phase because of its size and
risk relative to the rest of what was Phase 4 (it was already, on its
own, the single largest and highest-risk sub-phase — see §13 below).
**MicroPython** (the former sub-phase 4E) is *not* part of this phase —
it moved to [phase6-spec.md](phase6-spec.md) sub-phase 6B (D33).

**Reference reading**: [ti-parity.md](../notes/ti-parity.md)
§8 positions this phase against TI-Nspire CX II CAS feature-for-feature —
read that first for what's explicitly *not* being attempted here (systems
of equations, limits/series, unit/dimensional arithmetic — see §13
"Non-goals" below, which restates the scope boundary from that
comparison). Plain exact-value display (`√2` staying `√2`) is a related
but *separate* question and **is** in scope — see §10.1.
[design-departures-matrix-complex.md](../notes/design-departures-matrix-complex.md)
has unbuilt ideas — complex-valued variables/lists/matrices, a unified
tagged-value evaluator — that would change how CAS's complex-number
handling (§4.1's `i` reservation, §7's complex roots) integrates with the
rest of the engine if picked up first; none are assumed by this spec, but
§4.1 and §7 are the seams to revisit if they land.

---

## 1. Overview

This is the direct continuation of what was Phase 4 sub-phase 4D. The CAS
operates on a new symbolic expression tree (`ExprTree`) distinct from the
numerical evaluation path. The numerical evaluator (tinyexpr++) remains
for graphing and immediate numeric results — this is a firm architectural
rule (see §13 Risk 3, and `AGENTS.md`'s hot-path guidance). The CAS is
invoked explicitly: the user types an expression and either selects a CAS
operation from a menu, or calls a CAS function by name (`diff`, `integ`,
`solve`, `factor`, `expand`, `simplify`) inline.

CAS also provides the *symbolic* counterparts to the numeric `dy/dx` and
`fnInt` built in Phase 4B. Where 4B computes a numeric derivative at a
point or a definite integral over bounds, this phase's `differentiate`
(§6) and `integrate` (§9) return symbolic expressions. On the graph
screen, the CALC menu's `dy/dx` and `∫` stay numeric (fast, always
available); a user wanting the symbolic form uses the CAS menu on the
function's expression.

**Why complex-before-CAS held**: 4C (complex numbers) shipped before this
phase specifically so CAS solving can be complex-aware —
`solve(x^2 + 1 = 0)` returns `{i, -i}`, which requires the complex
representation (D30) to already exist. That ordering dependency is now
satisfied; this phase can start whenever it's scheduled.

**Estimated effort**: ~124 hours (see §11), the same estimate carried over
from the original 4D task breakdown — nothing about the split changes the
work itself, only its phase boundary.

---

## 2. Symbolic expression tree

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

## 3. Memory management

Symbolic manipulation creates many short-lived intermediate trees. A
**pool allocator** sized for CAS operations avoids heap fragmentation:

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

Each top-level CAS operation (differentiate, simplify, solve) calls
`g_cas_pool.reset()` at the start, works entirely within the pool, then
copies the final result to a persistent output buffer before the pool is
reclaimed. This means intermediate garbage is never individually freed —
the entire pool is bulk-reclaimed.

**Sizing**: a single `Expr` node is ~32 bytes. A 64 KB pool holds ~2000
nodes. This is sufficient for expressions of practical complexity — even
a 50-term polynomial after expansion produces ~150 nodes. Deeply
recursive operations (repeated integration) may need the pool to spill
into PSRAM on Pico 1, which is why the pool lives there by default.

**Note (Pico 1 headroom)**: this budget was set when Pico 1 bss was well
under half of the 264 KB SRAM budget. As of the most recent Phase 4
sessions (D28–D31), Pico 1 bss sits at ~188.8 KB with roughly 76 KB of
stack/heap headroom left, a recurring watch item across sessions. **Re-verify
this pool's SRAM-vs-PSRAM placement against actual headroom before
implementation starts**, not just at the estimate above — the 320 KB
PSRAM-spill ceiling mentioned in Risk 2 (§13) should probably be the
default on Pico 1 rather than a fallback, given how little SRAM margin
remains.

## 4. Expression parser (string → `Expr` tree)

The CAS needs its own parser because tinyexpr++ produces an opaque
evaluation tree, not a manipulable AST. The CAS parser handles the same
infix syntax but produces `Expr` nodes:

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

The parser is a standard recursive-descent parser for mathematical
expressions. Operator precedence: `=` < `+`/`-` < `*`/`/` < unary `-` <
`^` < function application. Implicit multiplication (e.g., `2x` → `2*x`,
`xy` → `x*y`) is supported for CAS mode since it's natural for algebraic
entry.

### 4.1 Imaginary unit in the symbolic tree

`i` is a reserved `VAR` whose algebraic rule is $i^2 = -1$. The
simplifier (§5) knows this rule, so `i*i` simplifies to `-1`, `i^3` to
`-i`, and `i^4` to `1`. Complex literals appear as ordinary subtrees,
e.g., `3 + 2*i`. When a symbolic result containing `i` is evaluated
numerically or stored, it converts to a `math::Complex` (Phase 4C). This
shared representation is what lets CAS `solve` (§8) return complex roots.

This is the seam the design-departures doc flags: today `i` is reserved
the same way in both the numeric complex engine (D30) and this symbolic
tree, but complex-valued *storage* doesn't exist yet in either (D30 §5:
"Complex results can't be stored"). If that gap closes first (departures
doc idea B), this section's conversion story gets simpler, not harder —
worth checking the departures doc before implementing §4.1/§8's
`complex_to_expr`/`expr_to_complex` helpers.

## 5. Simplification (`cas/simplify.hpp`)

The simplifier applies a set of rewriting rules repeatedly until no rule
fires (fixed-point iteration). Rules are applied bottom-up — children are
simplified before parents.

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

**Implementation approach**: each rule is a function
`Expr* try_rule(const Expr* e)` that returns a new tree if the rule
applies, or `nullptr` if it doesn't. The simplifier loops through the
rule set applying every applicable rule, then recurs into children,
repeating until a full pass produces no changes. A hard iteration cap
(e.g., 50 passes) prevents infinite loops from malformed rule
interactions.

## 6. Symbolic differentiation (`cas/derivative.hpp`)

```cpp
namespace math::cas {

// Differentiate expr with respect to variable var.
// Returns a simplified result.
Expr* differentiate(const Expr* expr, char var);

} // namespace math::cas
```

Differentiation is the most straightforward CAS operation — it's a
direct structural recursion over the tree with well-defined rules:

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

The raw derivative output is typically bloated (e.g., $\frac{d}{dx}[x^2 \cdot \sin(x)]$ produces `2*x*sin(x) + x^2*cos(x)` which is already simplified, but $\frac{d}{dx}[\frac{x}{x+1}]$ produces `(1*(x+1) - x*1) / (x+1)^2`). Every differentiation call ends with `simplify()` (§5) to clean up the result.

**Higher-order derivatives**:
`differentiate(differentiate(expr, 'x'), 'x')` — just recurse. A
`differentiate_n(expr, var, n)` convenience wrapper applies $n$ times.

**Scope note**: single-variable only, no partial derivatives and no
implicit differentiation. TI-Nspire CAS supports both — explicitly out of
scope here, per the parity doc §8.

## 7. Expansion and factoring (`cas/expand.hpp`, `cas/factor.hpp`)

**Expand** distributes multiplication over addition:

```cpp
namespace math::cas {

// Expand products and powers of sums.
// E.g., (x+1)*(x-1) → x^2 - 1
//       (x+1)^3 → x^3 + 3x^2 + 3x + 1
Expr* expand(const Expr* expr);

} // namespace math::cas
```

Implementation: recursively expand `MUL(ADD(...), ADD(...))` using the
FOIL-like distribution algorithm. For `POW(ADD(...), n)` where $n$ is a
positive integer, expand via binomial theorem or repeated
multiplication. Cap exponent at a reasonable limit (e.g., $n \leq 20$) to
prevent combinatorial explosion.

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

1. **Common factor extraction**: pull out the GCD of all term
   coefficients and the lowest power of each variable. E.g.,
   $6x^3 + 4x^2 \to 2x^2(3x + 2)$.
2. **Difference of squares**: $a^2 - b^2 \to (a+b)(a-b)$.
3. **Quadratic**: for degree-2 polynomials, use the discriminant. If $\Delta = b^2 - 4ac$ is a perfect square, produce $(x - r_1)(x - r_2)$. Otherwise, return as-is or use the quadratic formula symbolically.
4. **Rational root theorem**: for degree 3–4, test rational roots
   $\pm \frac{p}{q}$ where $p$ divides the constant term and $q$ divides
   the leading coefficient. If a root is found, divide out the factor
   and recurse.
5. **Grouping**: for four-term expressions, try grouping pairs and
   factoring common sub-expressions.

**Scope boundary** (restated from the parity doc, worth keeping visible
in the spec itself): this handles the vast majority of factoring
problems encountered in a high-school / early-college calculus context.
It will not factor quintics or higher-degree polynomials with irrational
roots — that requires Galois theory and is well beyond what even the
HP-50G or TI-Nspire CAS attempt.

## 8. Symbolic equation solving (`cas/solve.hpp`)

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

1. **Linear isolation**: if the equation is linear in `var`, algebraically
   isolate it. E.g., $3x + 5 = 17 \to x = 4$.
2. **Quadratic formula**: if the equation is quadratic in `var`, apply
   $x = \frac{-b \pm \sqrt{b^2-4ac}}{2a}$ symbolically. When the
   discriminant $b^2 - 4ac < 0$, the roots are complex; the solver emits
   them using the symbolic imaginary unit `i` (§4.1). For example,
   `solve(x^2 + 1 = 0)` returns `{i, -i}` and
   `solve(x^2 - 2x + 5 = 0)` returns `{1 + 2*i, 1 - 2*i}`. This requires
   the complex subsystem from Phase 4C.
3. **Polynomial root-finding**: for degree 3–4, try the rational root
   theorem + synthetic division to reduce to a quadratic.
4. **Inverse function isolation**: for equations like $\sin(x) = 0.5$,
   apply $\arcsin$ to both sides. Handles the standard trig, exp, and
   log inversions.
5. **Numeric fallback**: if symbolic methods fail, fall back to
   `numeric_solve()` (Phase 4A) and present the result as an approximate
   decimal.

**Complex roots**: the quadratic path (and, where reducible, the
cubic/quartic paths) produce complex roots when real roots don't exist.
Complex solutions are honored only when the number mode (Phase 4C) is
RECTANGULAR or POLAR; in REAL mode the solver reports "no real solution"
instead. The symbolic `i` in a solution tree converts to a numeric
`math::Complex` when the result is evaluated or stored.

**Scope boundary**: single-equation, single-variable only. No systems of
equations — Nspire's `solve` handles simultaneous equations natively;
that's out of scope here (parity doc §8).

## 9. Symbolic integration (`cas/integrate.hpp`)

This is the hardest CAS operation and the one with the most limited
scope. A general-purpose symbolic integrator (like the Risch algorithm)
is infeasible on this hardware. Instead, implement a **table-based
integrator** with a few heuristic methods.

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

1. **Linearity**: $\int (af + bg) \, dx = a\int f \, dx + b\int g \, dx$.
   Always applied first — split sums and pull out constants.
2. **Linear substitution**: if the integrand matches a table entry with
   $x$ replaced by $(ax + b)$, apply the substitution rule:
   $\int f(ax+b) \, dx = \frac{1}{a} F(ax+b)$. E.g.,
   $\int \sin(3x+1) \, dx = -\frac{1}{3}\cos(3x+1)$.
3. **Power rule generalization**: $\int u^n \cdot u' \, dx = \frac{u^{n+1}}{n+1}$ where $u$ is a sub-expression of $x$. Check if the integrand has the form $f(u) \cdot u'$ for common $f$.
4. **Integration by parts** (one level only): if the integrand is a
   product $u \cdot v$, try $\int u \, dv = uv - \int v \, du$ with
   heuristic choice of $u$ and $dv$ (LIATE rule: Logarithmic, Inverse
   trig, Algebraic, Trig, Exponential). Attempt this once — don't
   recurse IBP, as that risks infinite loops and memory exhaustion.

If all methods fail, `integrate()` returns `nullptr` and the UI reports
that symbolic integration was unsuccessful, offering to compute a numeric
definite integral via Simpson's rule or Gauss-Legendre quadrature
instead (or, per Phase 4B, the existing `numeric_integral` adaptive
Gauss-Kronrod path).

## 10. CAS user interface

CAS operations are accessible from the home screen via a menu. The
interaction model:

1. Type an expression on the home screen (e.g., `x^3 - 3*x + 2`)
2. Press a CAS key (mapped to a function key or key combo — e.g., `2nd` +
   `ENTER` for CAS menu)
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

4. Result appears pretty-printed in the history, tagged as "symbolic"
   with a different accent color (e.g., dark blue vs. black for numeric
   results)

Alternatively, CAS functions are callable directly as expression syntax:

```
diff(x^3, x)          → 3x²
integ(sin(x), x)      → -cos(x)
solve(x^2 - 4 = 0, x) → {-2, 2}
factor(x^2 - 4)       → (x-2)(x+2)
expand((x+1)^3)       → x³ + 3x² + 3x + 1
simplify(2x/4x^2)     → 1/(2x)
```

These CAS functions are registered in the expression parser alongside the
numeric functions, but routed to the symbolic engine instead of
tinyexpr++. Detection is simple: if the expression contains a CAS keyword
(`diff`, `integ`, `solve`, `factor`, `expand`, `simplify`), the entire
expression is parsed by the CAS parser and processed symbolically.

**Open question** (carried over — see §14, P5-3): does `solve()` used
here collide with the Phase 4A *numeric* `solve()` inline syntax
(`solve(f, var, lo, hi)`, D28)? Both share the name `solve` today. This
needs resolving before implementation — likely by argument-count/shape
disambiguation (numeric solve always takes bounds or a guess; CAS solve
takes an equation with `=`) or a distinct CAS keyword.

### 10.1 Exact-form (surd) display

Folded into core Phase 5 scope on 2026-07-21 (graduated from the
wishlist's "symbolic display" item — see
[wishlist.md](../notes/wishlist.md)) rather than left as a permanent
orphan: the reason this was stuck in wishlist limbo is that it needs
exactly the machinery Phase 5 builds anyway (the `Expr` tree and
`simplify()`), so it's cheap to add here and has nowhere sensible to live
before Phase 5 exists.

**Goal**: home-screen results that have a clean closed form — `sqrt(2)`,
`sqrt(8)` (→ `2*sqrt(2)`), `pi/2`, `1/3` — display that form instead of a
truncated decimal, the way TI-Nspire CAS and every desktop CAS do by
default.

**Mechanism**: this is a display-path feature, not an always-on symbolic
evaluator — it must not touch `evaluate_real()` (the graphing/table/stats
hot path is untouched, same firm rule as everywhere else in this spec,
§13 Risk 3). On the home screen only, after the ordinary numeric result
is computed, run a **side-effect-free CAS probe**: parse the same input
through the CAS parser (§4) and `simplify()` (§5). This mirrors the
pattern Phase 4C's `complexexpr` REAL-mode probe already established
(D30 §4) — a second, cheap, Enter-rate-only parse that upgrades the
display without double-committing `Ans`/store effects.

- If the simplified tree is a bare **integer** `NUM`, there's nothing
  exact to show beyond the decimal already computed — display unchanged.
  (Amended 2026-08-03: a bare *non-integer* rational **is** upgraded, so
  `1/3` shows `1/3` rather than `0.3333333333`. The original bullet said
  every bare `NUM` was left alone; that would have excluded the `1/3`
  case §10.1's own goal statement lists.)
- If the simplified tree contains only rational coefficients plus
  `sqrt`/`pi` sub-expressions in a form `simplify()` couldn't reduce
  further (i.e., it's already in lowest terms — no numeric evaluation
  needed to represent it exactly), pretty-print it instead of the
  decimal. (Amended: via `serialize` → `render::build_layout`, not a
  dedicated `expr_to_layout` — D42 made that call for all CAS results.)
- Anything else (transcendental mixes, results needing numeric
  root-finding, non-exact irrationals) falls back to the existing decimal
  display — this is a narrow "recognize clean closed forms," not a
  general "always show symbolic if possible" mode.

**Guard rails** (added 2026-08-03 with the implementation, D43): the probe
is gated so that always-on recognition cannot change an answer or surprise
a user who typed a decimal. Every numeric literal in the *parsed* input
must be an integer (so `2.5` stays `2.5` and `0.1+0.2` stays `0.3`, rather
than being re-rationalized into `5/2` and `3/10`); no variables may appear
at all (the CAS parser has no `ans` or `e`, so a symbolic reading of them
would be silently wrong); and the recognized form must agree with the
numeric result to $10^{-9}$ relative before it is displayed, which makes
CAS-vs-`tinyexpr` parser divergence unable to alter a shown answer.

**Scope boundary**: this is exact-*value* display for plain numbers only.
It is explicitly **not** the same as CAS `simplify()` on a variable
expression (§10's menu/inline syntax, which already does full symbolic
work) and it is **not** unit/dimensional arithmetic (`3 m/s` staying
symbolic) — that remains a non-goal, see §13.

---

## 11. Task breakdown

Solo developer, part-time (~20 hrs/week). Carried over verbatim from the
original 4D estimate — the numbering below keeps the original `4D.n` task
IDs for continuity with any existing notes/commits that reference them,
rather than renumbering to `5.n`.

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
| 4D.23 | Exact-form recognition (§10.1): rational-coefficient + surd/pi closed-form check on a simplified tree — **done 2026-08-03** (`src/math/cas/exact.cpp`) | 6 | `sqrt(8)` recognized as exact `2*sqrt(2)` form |
| 4D.24 | Home-screen exact-form probe (side-effect-free, D30-§4-style) + display integration — **done 2026-08-03** | 4 | `sqrt(2)` on home screen shows `√2`, not `1.41421` |
| | **Total** | **~134 hrs** | |

---

## 12. Performance expectations

Complex arithmetic context (needed to read the CAS complex-solve rows
below): complex arithmetic is 2–$6\times$ the cost of real (a complex
multiply is 4 real multiplies + 2 adds). Since complex evaluation is
confined to the home-screen path (never graphing — Phase 4C), this is
imperceptible for single expressions.

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

Worst-case complex simplification approaches ~0.5 s on Pico 1 —
perceptible but acceptable (the TI-89 often took longer at 10–12 MHz).

---

## 13. Risks and mitigations

### Risk 1: CAS simplification infinite loops

Poorly ordered rewriting rules can cycle. **Mitigation**: hard cap on
simplification passes (50 iterations); canonical ordering of commutative
operands for deterministic matching; test suite of tricky expressions
($\frac{x}{x}$, $0^0$, $(x+y)^{20}$, `i^4`).

### Risk 2: CAS pool memory exhaustion

Expanding $(x+y+z)^{15}$ produces thousands of nodes. **Mitigation**:
check pool capacity before expansion; abort with an error above 80%
capacity; pool can grow into PSRAM (up to 512 KB) for specific
operations. See §3's headroom note — on Pico 1, defaulting to PSRAM
rather than treating it as fallback-only may be the right call given
current bss pressure; re-check at implementation time.

### Risk 3: Complex evaluation slowing the hot path

If the numeric evaluator became complex-by-default, graphing would slow
2–$6\times$. **Mitigation**: dual entry points — `evaluate_real()` (fast,
used by graphing/tables/stats) and `evaluate_complex()` (home screen
only) — already exist from Phase 4C. This CAS phase must not touch that
boundary either: CAS parsing/simplification/solving stays entirely
off the graphing path. This is a firm architectural rule, documented in
`AGENTS.md`.

### Risk 5: CAS ↔ complex representation seams

*(numbered 5 to match the original phase4-spec risk numbering, which this
section is carried over from — risk 4, numeric-integration accuracy, was
Phase 4B's and stayed there.)* The symbolic `i` (a reserved VAR) and the
numeric `Complex` type are two representations of the same concept;
conversion bugs are likely. **Mitigation**: centralize conversion in one
place (`cas/solve.cpp` helper functions `complex_to_expr` /
`expr_to_complex`); unit-test the round trip; keep the rule $i^2 = -1$ in
exactly one simplification rule. See §4.1 for how this seam interacts
with the (currently unbuilt) complex-storage departure ideas.

### Non-goals (scope boundary, restated from the parity doc)

Explicitly not attempted in this phase, to keep the risk list honest
about what's *not* being mitigated because it's simply out of scope:

- Systems of equations (simultaneous `solve`)
- Partial derivatives, implicit differentiation
- Limits, series, symbolic sums
- General-purpose (Risch-class) symbolic integration
- **Unit/dimensional arithmetic** (`3 m/s` staying symbolic through
  arithmetic) — a materially bigger feature than exact-value display
  (needs a unit-algebra layer, not just recognizing closed forms) and not
  requested anywhere; stays out of scope.

**In scope, not a non-goal**: plain exact-*value* display (`sqrt(2)`
showing as `√2` rather than `1.41421`) — folded into core scope as §10.1
on 2026-07-21 (graduated from the wishlist's "symbolic display" item, see
[wishlist.md](../notes/wishlist.md)), since it needs exactly this phase's
`Expr` tree and `simplify()` and has nowhere else to live. Called out
here because it's easy to assume it's excluded along with the
unit-arithmetic item above — it isn't.

See [ti-parity.md](../notes/ti-parity.md) §8 for
the full comparison this boundary is drawn against.

---

## 14. Open questions

| # | Question | Options | Carried from |
|---|----------|---------|---|
| P5-1 | Store CAS results in the same history as numeric results? | Unified vs. separate CAS history | phase4-spec P4-1 |
| P5-2 | Represent symbolic results in variables (e.g., `A = x^2+1`)? | Allow expression-valued variables vs. numeric-only | phase4-spec P4-2 |
| P5-3 | Implicit multiplication globally, or CAS-mode only? | CAS-only (safer) vs. global (natural) | phase4-spec P4-3 |
| P5-4 | `solve()` naming collision with Phase 4A's numeric inline `solve()` (D28)? | Disambiguate by shape vs. distinct keyword (e.g. `csolve`) | New — flagged in §10 above |
| P5-5 | Exact-form display (§10.1): always-on, or a MODE toggle (like Nspire's Auto/Approximate)? | **RESOLVED 2026-08-03 (D43): always-on.** No MODE row entry; `>dec` is the per-result opt-out that already exists | New — flagged in §10.1 |
| P5-6 | Exact-form display: does `pi` itself (not just `sqrt`) get the closed-form treatment, e.g. does `pi*2` show as `2π` rather than `6.28319`? | **RESOLVED 2026-08-03 (D43): yes.** Matches the pi-tick-label precedent (4D.3) and reads consistently next to a surd in the same result line | New — flagged in §10.1 |

---

## 15. References

1. Phase 1 spec — [phase1-spec.md](phase1-spec.md)
2. Phase 3 spec (Array primitive) — [phase3-spec.md](phase3-spec.md)
3. Phase 4 spec (matrix, graph analysis, complex numbers — prerequisite
   sub-phases 4A–4C) — [phase4-spec.md](phase4-spec.md)
4. TI parity stocktake (§8: this phase vs. TI-Nspire CX II CAS) —
   [ti-parity.md](../notes/ti-parity.md)
5. Design departures (complex/matrix first-class ideas) —
   [design-departures-matrix-complex.md](../notes/design-departures-matrix-complex.md)
6. DB48X source (CAS reference) — https://github.com/c3d/db48x
7. DB48X symbolic operations — https://github.com/c3d/db48x/releases/tag/v0.8.1
8. KhiCAS / Giac (full CAS on calculators) — https://github.com/nwagyu/khicas
9. NIST DLMF (integration tables, special functions) — https://dlmf.nist.gov/
