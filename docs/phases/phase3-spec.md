# Phase 3 Spec: Statistics, Data Lists & Inference

**Prerequisite**: Phase 1 (calculator + graphing), Phase 2 (table view, parametric/polar, split-screen).

**Scope**: A full statistics suite — a shared n-dimensional array primitive backing data lists, a list editor, descriptive statistics, all ten TI-style regression models, probability distributions, statistical plots, and the complete inferential test suite (hypothesis tests, confidence intervals, ANOVA). Function names are modernized; navigation is modernized (no TI STAT/DISTR/TESTS menu structure).

**End state**: enter and edit data in lists, compute 1-var and 2-var statistics, fit regressions, evaluate and invert distributions, run hypothesis tests and confidence intervals, and visualize data with histograms, box plots, and scatter plots overlaid on the Phase 1/2 graphing engine.

**Estimated effort**: ~8 weeks part-time (~170 hours).

---

## 1. Overview and sub-phases

Phase 3 is large. It splits into four sub-phases, developed in order because each builds on the previous:

| Sub-phase | Weeks | Content |
|-----------|-------|---------|
| 3A: Array primitive + lists | 17–18 | `Array` type, shared backing store, list editor |
| 3B: Descriptive stats + regression | 19–21 | 1-var/2-var stats, 10 regression models |
| 3C: Distributions | 22–23 | PDF/CDF/inverse for all standard distributions |
| 3D: Inference + stat plots | 24–25 | Hypothesis tests, CIs, ANOVA; histogram/box/scatter |

The single most important architectural piece is the **`Array` primitive** (3A). It is the shared backing store for data lists ($L_1 \ldots L_6$) in this phase and for matrices in Phase 4. Getting it right here means Phase 4's matrix work consumes it rather than defining a parallel structure.

---

## 2. The `Array` primitive (`math/array.hpp`)

> **As built (Session 11, 2026-07-19 — see D22):** the sketch below predates
> one hardware fact — the PSRAM is SPI-attached and **not memory-mapped** —
> so the shipped API has **no `calc_t& at()` and no `data()` pointer**.
> Element access is `get(i)`/`set(i, v)` (routed through the D21 dtype tag)
> plus bulk `read_range`/`write_range` (chunked DMA). `ArrayStore` is a
> fixed-size recycling store: 12 x 2 KB SRAM slabs and up to 12 x 80 KB
> PSRAM regions on a free-list over the platform bump allocator. §3.2's
> home-screen syntax is implemented by `math::listexpr` (vector lift over
> the engine; see D22 for the exact grammar and its limits).

### 2.1 Design

A single n-dimensional numeric array type. Lists are 1-D ($1 \times n$); matrices (Phase 4) are 2-D ($m \times n$). Designed for both.

```cpp
namespace math {

// N-dimensional numeric array. Row-major storage.
// Lists are 1-D (shape {n}); matrices are 2-D (shape {rows, cols}).
// Backing store is PSRAM for large arrays, SRAM pool for small.
class Array {
public:
    static constexpr int kMaxDims = 2;  // Phase 3/4 need at most 2-D

    Array() = default;
    Array(int d0);                 // 1-D: a list of length d0
    Array(int d0, int d1);         // 2-D: d0 rows x d1 cols

    // Shape
    int  ndim() const { return ndim_; }
    int  dim(int axis) const { return shape_[axis]; }
    int  size() const;             // Total element count
    bool is_list() const   { return ndim_ == 1; }
    bool is_matrix() const { return ndim_ == 2; }

    // Element access
    calc_t& at(int i);                 // 1-D
    calc_t& at(int r, int c);          // 2-D
    calc_t  at(int i) const;
    calc_t  at(int r, int c) const;

    // Bulk operations
    void fill(calc_t value);
    void resize(int d0);               // 1-D resize (preserves data where possible)
    void resize(int d0, int d1);       // 2-D resize
    void clear();                      // Empty the array (size 0)

    // Raw data (row-major)
    calc_t* data() { return data_; }
    const calc_t* data() const { return data_; }

    // Storage location
    bool in_psram() const { return in_psram_; }

private:
    int    ndim_ = 0;
    int    shape_[kMaxDims] = {0, 0};
    calc_t* data_ = nullptr;
    bool   in_psram_ = false;

    // Threshold: arrays <= 256 elements use SRAM pool, larger use PSRAM.
    static constexpr int kSramThreshold = 256;
};

}  // namespace math
```

### 2.2 Backing store

A single allocator manages array storage. Small arrays (lists up to 256 elements, small matrices) live in an SRAM pool for speed; larger arrays go to PSRAM. This decision is internal to `Array` — callers don't manage it.

> **D21 (2026-07-18, amended same day): ships as specced above — SRAM pool
> for <= 256 elements, PSRAM tier for larger, cap 10000.** The initial
> SRAM-only call was made under the D10 bulk-PSRAM quarantine; the D10 fix
> landed the same day (chunked transfers, ~6.8 MB/s HW-verified), so the
> PSRAM tier is live for Phase 3 lists and Phase 4 matrices. Cold-boot
> caveat (D14, unresolved, non-blocking): PSRAM can be late by a few
> seconds on a cold power-on — list load just waits for late-init; nothing
> needs PSRAM at boot. `Array` additionally carries a **dtype tag**
> (double-only today) persisted in `lists.dat`. The tag is committed, not
> speculative: **complex-valued lists/matrices ship with the future
> complex-numbers feature** (see D21) — Phase 3 code must route element
> access through the tag-aware API so that addition is non-breaking.

```cpp
namespace math {

class ArrayStore {
public:
    void init();

    // Allocate storage for an array of `count` calc_t values.
    // Chooses SRAM pool or PSRAM based on size.
    calc_t* alloc(int count, bool* out_in_psram);
    void    free(calc_t* ptr, bool in_psram);

    size_t sram_used() const;
    size_t psram_used() const;
};

// Singleton accessor (project convention — matches math::engine(),
// platform::storage(); no extern globals).
ArrayStore& array_store();

}  // namespace math
```

### 2.3 List variables

Six named list slots, following TI's $L_1 \ldots L_6$ but stored as `Array` instances:

```cpp
namespace math {

class ListStore {
public:
    // Access list by index (0 = L1, 5 = L6)
    Array& list(int index);
    const Array& list(int index) const;

    // Persistence to SD card (/picocalc/lists.dat)
    bool save(platform::Storage& storage) const;
    bool load(platform::Storage& storage);

private:
    Array lists_[6];
};

// Singleton accessor (project convention).
ListStore& lists();

}  // namespace math
```

**Phase 4 note**: matrices ($[A] \ldots [J]$) will be another set of `Array` instances (2-D). Phase 4's spec currently defines a separate `Matrix` class; the reconciliation is to have Phase 4 use `Array` (2-D) instead, or wrap `Array` in a thin `Matrix` view providing linear-algebra methods (determinant, inverse, etc.). See §11.

---

## 3. Sub-phase 3A: List editor (weeks 17–18)

### 3.1 List editor screen

Spreadsheet-style, like TI's STAT→Edit but modernized:

```
┌──────────────────────────────────┐
│  List Editor                      │
├──────────────────────────────────┤
│      L₁       L₂       L₃         │
│  1   12.0     45.0     ---        │
│  2   15.0     52.0     ---        │
│  3   18.0     48.0     ---        │
│  4   22.0     61.0     ---        │
│  5   ▏        ---      ---        │
│                                    │
│  L₁(5) = _                        │
├──────────────────────────────────┤
│ F1:STATS F2:SORT F3:CLEAR F4:PLOT│
└──────────────────────────────────┘
```

**Behavior**:

- Arrow keys navigate cells; the active list/index shows in the entry line.
- Typing a value and pressing Enter stores it and advances down.
- Entering a value in the first empty row of a list appends to it.
- `LEFT`/`RIGHT` at column edges scrolls horizontally (6 lists, ~3 visible).
- Editing a cell mid-list replaces that value.
- Deleting a value (clear + Enter) removes the row and shifts up.
- Lists persist to `/picocalc/lists.dat` on edit.

### 3.2 List operations

Callable from the home screen and list editor:

```
sort_asc(L1)              — sort ascending in place
sort_desc(L1)             — sort descending
seq(expr, var, lo, hi, step)  — generate a sequence into a list
cumsum(L1)                — cumulative sums
delta_list(L1)            — consecutive differences
sum(L1)                   — sum of elements
prod(L1)                  — product of elements
length(L1)                — element count
L1 + L2, L1 * 2, etc.     — element-wise arithmetic
```

List arithmetic is element-wise and returns a new list. Length-mismatched operations report an error.

---

## 4. Sub-phase 3B: Descriptive stats & regression (weeks 19–21)

### 4.1 One-variable statistics

```cpp
namespace math::stats {

struct OneVarStats {
    calc_t mean;
    calc_t sum;           // Σx
    calc_t sum_sq;        // Σx²
    calc_t sample_stddev; // s (n-1 denominator)
    calc_t pop_stddev;    // σ (n denominator)
    int    n;
    calc_t min_val;
    calc_t q1;            // First quartile
    calc_t median;        // Q2
    calc_t q3;            // Third quartile
    calc_t max_val;
};

OneVarStats one_var(const Array& data);
// Optional frequency-weighted variant:
OneVarStats one_var_weighted(const Array& data, const Array& freq);

}  // namespace math::stats
```

### 4.2 Two-variable statistics

```cpp
namespace math::stats {

struct TwoVarStats {
    calc_t mean_x, mean_y;
    calc_t sum_x, sum_y, sum_xy, sum_x2, sum_y2;
    calc_t sample_stddev_x, sample_stddev_y;
    calc_t pop_stddev_x, pop_stddev_y;
    int    n;
    calc_t min_x, max_x, min_y, max_y;
};

TwoVarStats two_var(const Array& x, const Array& y);

}  // namespace math::stats
```

### 4.3 Regression models

All ten TI regression types:

```cpp
namespace math::stats {

enum class RegressionType {
    LINEAR,        // y = ax + b
    QUADRATIC,     // y = ax² + bx + c
    CUBIC,         // y = ax³ + bx² + cx + d
    QUARTIC,       // y = ax⁴ + ... + e
    LOGARITHMIC,   // y = a + b·ln(x)
    EXPONENTIAL,   // y = a·bˣ
    POWER,         // y = a·xᵇ
    LOGISTIC,      // y = c / (1 + a·e^(−bx))
    SINUSOIDAL,    // y = a·sin(bx + c) + d
    MEDIAN_MEDIAN, // resistant line (Tukey)
};

struct RegressionResult {
    RegressionType type;
    calc_t coeffs[5];   // Up to 5 coefficients (quartic)
    int    coeff_count;
    calc_t r;           // Correlation coefficient (where defined)
    calc_t r_squared;   // Coefficient of determination
    bool   converged;   // For iterative fits (logistic, sinusoidal)
};

RegressionResult regress(const Array& x, const Array& y, RegressionType type);

// Store the fitted model into a Y-slot for graphing overlay.
bool store_regression_to_y(const RegressionResult& r, int y_slot);

}  // namespace math::stats
```

**Implementation notes**:

- **Polynomial fits** (linear through quartic) use least-squares via normal equations, solved with Gaussian elimination on the `Array`/matrix machinery. This is a natural first consumer of the shared array store.
- **Log/exp/power** fits linearize the model (take logs) then apply linear regression, transforming coefficients back.
- **Logistic and sinusoidal** are nonlinear — use iterative Levenberg-Marquardt or Gauss-Newton. These set `converged` and may fail on poorly-conditioned data.
- **Median-median** is a resistant (outlier-robust) line using the Tukey three-group method.

Regression results can be stored into a graphing Y-slot (§4.3 `store_regression_to_y`) so the fit overlays the scatter plot — bridging to the Phase 1/2 graph engine.

---

## 5. Sub-phase 3C: Probability distributions (weeks 22–23)

Modernized function names (not TI's `normalcdf`/`invNorm`). Each distribution provides PDF/PMF, CDF, and inverse-CDF where applicable.

### 5.1 Continuous distributions

```cpp
namespace math::dist {

// Normal (Gaussian)
calc_t normal_pdf(calc_t x, calc_t mean = 0, calc_t sd = 1);
calc_t normal_cdf(calc_t lo, calc_t hi, calc_t mean = 0, calc_t sd = 1);
calc_t normal_inv(calc_t area, calc_t mean = 0, calc_t sd = 1);

// Student's t
calc_t t_pdf(calc_t x, calc_t df);
calc_t t_cdf(calc_t lo, calc_t hi, calc_t df);
calc_t t_inv(calc_t area, calc_t df);

// Chi-square
calc_t chisq_pdf(calc_t x, calc_t df);
calc_t chisq_cdf(calc_t lo, calc_t hi, calc_t df);
calc_t chisq_inv(calc_t area, calc_t df);

// F distribution
calc_t f_pdf(calc_t x, calc_t df1, calc_t df2);
calc_t f_cdf(calc_t lo, calc_t hi, calc_t df1, calc_t df2);
calc_t f_inv(calc_t area, calc_t df1, calc_t df2);

}  // namespace math::dist
```

### 5.2 Discrete distributions

```cpp
namespace math::dist {

// Binomial
calc_t binomial_pmf(int k, int n, calc_t p);
calc_t binomial_cdf(int k, int n, calc_t p);  // P(X <= k)

// Poisson
calc_t poisson_pmf(int k, calc_t lambda);
calc_t poisson_cdf(int k, calc_t lambda);

// Geometric
calc_t geometric_pmf(int k, calc_t p);
calc_t geometric_cdf(int k, calc_t p);

}  // namespace math::dist
```

### 5.3 Implementation

The distribution functions require special functions: the error function (`erf`) for normal, the incomplete beta function for $t$ and $F$, and the incomplete gamma function for $\chi^2$ and Poisson. Rather than implement these from scratch, **cherry-pick from the cephes library** (public domain C) — its `ndtr`, `incbet`, `igam`, `igamc` functions are well-tested and portable. Vendor only the needed source files into `drivers/cephes/` (or `src/math/cephes/`).

Inverse CDFs use Newton's method or bisection on the CDF. Note there is **no Phase 1 numeric solver to reuse** — the general equation solver arrives in Phase 4 (§3.4). 3C implements a small bracketed bisection locally; CDFs are monotone, so bisection is robust and a few lines of code. Phase 4's solver can replace it later if worthwhile.

Parser-facing note: tinyexpr binds **fixed-arity** functions only (`TE_FUNCTION0..7`) — the C++ default arguments above exist only for C++ callers. Home-screen registration exposes the full-arity forms (e.g. `normal_cdf(lo, hi, mu, sd)`); if standard-normal shorthands are wanted, register them as separate names.

**Precision note**: on Pico 1 (softfloat `double`), these are accurate but slow — a single `normal_cdf` involves an `erf` evaluation (~1000+ cycles). Acceptable for interactive use (single evaluations), but avoid calling them in tight loops. On Pico 2 (FPU) they're substantially faster.

---

## 6. Sub-phase 3D: Inference & statistical plots (weeks 24–25)

### 6.1 Hypothesis tests

Full test suite. Each returns a test statistic, p-value, and supporting quantities.

```cpp
namespace math::stats {

struct TestResult {
    calc_t statistic;   // z, t, chi², or F value
    calc_t p_value;
    calc_t df;          // Degrees of freedom (where applicable)
    calc_t df2;         // Second df (F-test / ANOVA)
    // Additional fields per test (means, proportions, CI bounds)
    calc_t estimate;    // Point estimate (mean diff, prop diff, etc.)
    calc_t ci_low, ci_high;  // If a CI is computed alongside
    bool   ok;
    const char* error;
};

// --- z-tests (population sd known) ---
TestResult z_test_1samp(calc_t x_bar, calc_t mu0, calc_t sigma, int n);
TestResult z_test_2samp(calc_t x1, calc_t s1, int n1,
                        calc_t x2, calc_t s2, int n2);

// --- t-tests (population sd unknown) ---
TestResult t_test_1samp(const Array& data, calc_t mu0);
TestResult t_test_1samp_summary(calc_t x_bar, calc_t s, int n, calc_t mu0);
TestResult t_test_2samp(const Array& d1, const Array& d2, bool pooled);
TestResult t_test_paired(const Array& before, const Array& after);

// --- proportion tests ---
TestResult prop_test_1samp(int successes, int n, calc_t p0);
TestResult prop_test_2samp(int x1, int n1, int x2, int n2);

// --- chi-square tests ---
TestResult chisq_gof(const Array& observed, const Array& expected);
TestResult chisq_independence(const Array& contingency_table);  // 2-D Array

// --- ANOVA (one-way) ---
TestResult anova_oneway(const Array* groups, int group_count);

// --- linear regression t-test (slope significance) ---
TestResult linreg_test(const Array& x, const Array& y);

}  // namespace math::stats
```

### 6.2 Confidence intervals

```cpp
namespace math::stats {

struct Interval {
    calc_t point_estimate;
    calc_t low, high;
    calc_t margin_of_error;
    calc_t confidence;   // e.g., 0.95
    bool   ok;
};

Interval ci_mean_z(calc_t x_bar, calc_t sigma, int n, calc_t conf);
Interval ci_mean_t(const Array& data, calc_t conf);
Interval ci_mean_t_summary(calc_t x_bar, calc_t s, int n, calc_t conf);
Interval ci_diff_means(const Array& d1, const Array& d2, calc_t conf, bool pooled);
Interval ci_proportion(int successes, int n, calc_t conf);
Interval ci_diff_proportions(int x1, int n1, int x2, int n2, calc_t conf);

}  // namespace math::stats
```

### 6.3 Inference UI

A modernized inference screen (replacing TI's TESTS menu). The user selects a test/interval from a categorized list, then fills a form:

```
┌──────────────────────────────────┐
│  Inference: 2-Sample t-Test       │
├──────────────────────────────────┤
│  Data source:  ● Lists  ○ Stats   │
│  List 1:  L₁                      │
│  List 2:  L₂                      │
│  Pooled:  ○ Yes  ● No             │
│  H₁:  ● ≠  ○ <  ○ >               │
│  Confidence:  0.95                │
├──────────────────────────────────┤
│  F1:CALCULATE  F2:DRAW  F3:BACK   │
└──────────────────────────────────┘
```

Results display the statistic, p-value, df, and (optionally) a visualization of the distribution with the critical region shaded (`F2:DRAW` uses the graph engine to plot the distribution's PDF with the test statistic marked).

### 6.4 Statistical plots (StatPlot layer)

Per the design decision, statistical plots **reuse the Phase 1/2 graph engine** with a dedicated `StatPlot` layer on top. Each stat plot is a `graph::PointSource`-like producer, but for markers/bars rather than connected curves.

```cpp
namespace graph {

enum class StatPlotType {
    SCATTER,      // (x, y) points from two lists
    XY_LINE,      // scatter with connecting lines
    HISTOGRAM,    // frequency bars from one list
    BOX_PLOT,     // box-and-whisker (with or without outliers)
    NORMAL_PROB,  // normal probability plot
};

struct StatPlot {
    StatPlotType type;
    int  x_list;         // Source list index (0-5)
    int  y_list;         // For scatter/xy-line (-1 if unused)
    int  freq_list;      // Frequency weights (-1 if unused)
    platform::Color color;
    char marker;         // '.', '+', 'x' for scatter
    bool enabled;
    // Histogram-specific:
    calc_t bin_width;    // 0 = auto
};

// StatPlot renderer — draws into the graph viewport alongside
// (or instead of) function plots.
class StatPlotRenderer {
public:
    void render(gfx::Framebuffer& fb, const Viewport& vp,
                const StatPlot& plot, const math::Array& data_x,
                const math::Array& data_y);
};

}  // namespace graph
```

**Plot types**:

- **Scatter / XY-line**: plot $(L_x[i], L_y[i])$ as markers; XY-line connects them in order. Regression overlays (from 3B) draw as a normal function on top.
- **Histogram**: bin the data, draw bars. Auto or manual bin width. Y-axis is frequency.
- **Box plot**: draw the five-number summary (min, Q1, median, Q3, max) as a box with whiskers. Modified box plot flags outliers (beyond 1.5·IQR) as separate marks.
- **Normal probability plot**: plot ordered data against theoretical normal quantiles — a diagnostic for normality.

Stat plots coexist with function plots in the graph viewport. The graph screen gains a "stat plots" toggle set (Plot1/Plot2/Plot3, like TI) alongside the Y-functions. `ZoomStat` auto-scales the window to fit the active stat plot data.

---

## 7. Task breakdown

Solo developer, part-time (~20 hrs/week). ~8 weeks.

### Sub-phase 3A: Array + lists (weeks 17–18)

| # | Task | Est. hrs | Notes |
|---|------|---|---|
| 3A.1 | `Array` primitive (1-D + 2-D, element access, resize) | 8 | Foundation for lists and Phase 4 matrices |
| 3A.2 | `ArrayStore` (SRAM pool + PSRAM backing) | 6 | |
| 3A.3 | `ListStore` (L1–L6) + SD persistence | 4 | |
| 3A.4 | List editor screen (grid, navigation, edit) | 10 | |
| 3A.5 | List operations (sort, seq, cumsum, arithmetic) | 6 | |
| | **Subtotal** | **~34 hrs** | |

### Sub-phase 3B: Descriptive + regression (weeks 19–21)

| # | Task | Est. hrs | Notes |
|---|------|---|---|
| 3B.1 | 1-var stats (mean, sd, quartiles, weighted) | 5 | |
| 3B.2 | 2-var stats | 3 | |
| 3B.3 | Polynomial regression (linear–quartic) via normal equations | 8 | First matrix-math consumer |
| 3B.4 | Log/exp/power regression (linearized) | 4 | |
| 3B.5 | Logistic + sinusoidal (iterative fit) | 8 | Highest-risk; convergence handling |
| 3B.6 | Median-median line | 3 | |
| 3B.7 | `r`, `r²` computation | 2 | |
| 3B.8 | Store regression → Y-slot for overlay | 3 | Bridges to graph engine |
| 3B.9 | Stats results UI (display OneVar/TwoVar/Regression) | 6 | |
| | **Subtotal** | **~42 hrs** | |

### Sub-phase 3C: Distributions (weeks 22–23)

| # | Task | Est. hrs | Notes |
|---|------|---|---|
| 3C.1 | Vendor cephes special functions (erf, igam, incbet) | 4 | |
| 3C.2 | Normal (pdf/cdf/inv) | 3 | |
| 3C.3 | Student t (pdf/cdf/inv) | 4 | |
| 3C.4 | Chi-square (pdf/cdf/inv) | 3 | |
| 3C.5 | F (pdf/cdf/inv) | 3 | |
| 3C.6 | Binomial, Poisson, geometric | 5 | |
| 3C.7 | Distribution function registration in expression parser | 3 | Full-arity only — tinyexpr has no default args (§5.3). Extend `math::catalog` (Phase 2 §10) so help stays complete |
| 3C.8 | Distribution UI / helper (guided entry) | 4 | |
| | **Subtotal** | **~29 hrs** | |

### Sub-phase 3D: Inference + plots (weeks 24–25)

| # | Task | Est. hrs | Notes |
|---|------|---|---|
| 3D.1 | z-tests (1/2 sample) | 4 | |
| 3D.2 | t-tests (1-sample, 2-sample, paired) | 6 | |
| 3D.3 | Proportion tests (1/2 sample) | 4 | |
| 3D.4 | Chi-square GOF + independence | 5 | Independence uses 2-D Array |
| 3D.5 | One-way ANOVA | 5 | |
| 3D.6 | Linear regression t-test | 3 | |
| 3D.7 | Confidence intervals (all variants) | 6 | |
| 3D.8 | Inference UI (test selector + forms) | 8 | |
| 3D.9 | StatPlot layer: scatter, xy-line | 5 | Reuses graph engine |
| 3D.10 | StatPlot: histogram | 4 | |
| 3D.11 | StatPlot: box plot (+ outliers) | 4 | |
| 3D.12 | StatPlot: normal probability plot | 3 | |
| 3D.13 | ZoomStat + stat-plot toggles in graph screen | 4 | |
| 3D.14 | Test on both Pico 1 and Pico 2 | 6 | Pico 1 leg also retires the deferred Phase 2 verification pass (D18): split-pane clipping on the strip renderer + Session 8 fix list |
| | **Subtotal** | **~65 hrs** | |

### Summary

| Sub-phase | Weeks | Hours | Deliverable |
|-----------|-------|-------|-------------|
| 3A: Array + lists | 17–18 | ~34 | Data lists, editor, operations |
| 3B: Descriptive + regression | 19–21 | ~42 | 1/2-var stats, 10 regressions |
| 3C: Distributions | 22–23 | ~29 | All standard distributions |
| 3D: Inference + plots | 24–25 | ~65 | Tests, CIs, ANOVA, stat plots |
| **Total** | **~8 weeks** | **~170 hrs** | Full statistics suite |

---

## 8. Performance considerations

- **Regression on large lists**: polynomial regression builds and solves a normal-equations system. For a quartic fit on 1000 points, that's forming a 5$\times$5 matrix from 1000 summed products (~5000 multiply-adds) then solving 5$\times$5 — trivial. Iterative fits (logistic/sinusoidal) may take 50–200 iterations; on Pico 1 softfloat, budget ~100–500 ms. Acceptable, but show a "computing…" indicator.
- **Distribution evaluations**: each `normal_cdf`/`t_cdf` is a special-function call (~1000+ cycles on Pico 1). Fine for single evaluations in tests; avoid in tight loops. The `F2:DRAW` distribution-shading feature plots a PDF across ~320 columns — that's 320 special-function calls, ~1–3 ms on Pico 2, ~10–30 ms on Pico 1. Acceptable.
- **List storage**: a 1000-element `double` list is 8 KB — comfortably in PSRAM. Six full lists ≈ 48 KB in PSRAM, negligible against 8 MB.
- **Strip-renderer safety (Pico 1 — required, see D18)**: Phase 3 is developed and hardware-tested on the Pico 2 (full framebuffer); the Pico 1 pass is deferred to 3D.14. On the Pico 1, `render()` runs once per 16-scanline strip — up to ~20×/frame — so every new screen's `render()` **must be idempotent**: no lazy cache fills, scroll adjustments, or other state mutation inside the draw path (compute in `on_key`/update, draw in `render`). Bugs here only manifest on Pico 1 hardware — there is no host coverage of the framebuffer. Also budget for strip overdraw: full-scene render logic re-runs per strip, so keep per-draw work (e.g. distribution shading, stat-plot point loops) cheap or dirty-band-scoped.

---

## 9. Open questions for Phase 3

| # | Question | Options | When |
|---|----------|---------|------|
| P3-1 | Max list length cap? | **DECIDED (D21, 2026-07-18, amended same day): 10000, SRAM pool + PSRAM tier** — the D10 bulk-PSRAM fix landed the same day (~6.8 MB/s verified), so §2.2 ships as written: <= 256 elements SRAM, larger PSRAM. | Decided |
| P3-2 | `Array` element type: always `calc_t` (double), or support integer lists? | **DECIDED (D21, 2026-07-18): double-only storage, plus a dtype tag in `Array` and `lists.dat`** reserved for future complex/int elements (Phase 4 Matrix + complex wishlist). | Decided |
| P3-3 | Iterative regression solver: Levenberg-Marquardt or Gauss-Newton? | LM is more robust but more code. Suggest LM. | Week 21, task 3B.5 |
| P3-4 | Distribution function naming: `normal_cdf(lo, hi, ...)` two-tailed like TI, or `normal_cdf(x)` one-tailed standard? | TI's two-arg lower/upper is practical for tests; standard CDF is more conventional. Decide and document. | Week 22, task 3C.2 |
| P3-5 | Should stat plots and function plots share the same enable/disable UI, or separate Plot1-3 vs Y1-7 lists? | TI separates them; unified might be cleaner. | Week 25, task 3D.13 |
| P3-6 | Inference results: always compute the paired CI alongside each test, or only on request? | Computing always is convenient; may clutter. | Week 24, task 3D.8 |

---

## 10. Reconciliation notes

- **Phase 4 matrix store**: Phase 4's spec (`phase4-spec.md`, §3.1) currently defines a standalone `Matrix` class with its own PSRAM allocation. **This should be reconciled** to build on the `Array` primitive defined here (§2). Two options: (a) Phase 4's `Matrix` becomes a thin wrapper around a 2-D `Array` providing linear-algebra methods (determinant, inverse, rref, eigenvalues); or (b) those methods move onto `Array` directly. Recommend (a) — keep `Array` as pure storage, put linear algebra in a `Matrix` view. **This reconciliation is noted here but should be applied when Phase 4 begins, updating `phase4-spec.md` §3.1 accordingly.**
- **Regression → graph overlay**: task 3B.8 stores a fitted regression into a graphing Y-slot. This depends on Phase 1's `GraphState`/Y-function storage (generalized in Phase 2). No new dependency — just reuse.
- **Distribution shading (`F2:DRAW`)**: uses the Phase 1/2 graph engine to plot a PDF and shade a region. This is a `PointSource` producing PDF values plus a fill operation. Reuses existing plotting; adds a shaded-region primitive to `gfx/`.
- **Angle mode**: sinusoidal regression produces a model with a trig term; ensure it respects the global degree/radian mode from Phase 1 when the fit is stored to a Y-slot and graphed.

---

## 11. References

1. Phase 1 spec — [phase1-spec.md](phase1-spec.md)
2. Phase 2 spec — [phase2-spec.md](phase2-spec.md)
3. Phase 4 spec (matrix reconciliation) — [phase4-spec.md](phase4-spec.md)
4. cephes special functions library — https://www.netlib.org/cephes/
5. NIST/SEMATECH e-Handbook of Statistical Methods — https://www.itl.nist.gov/div898/handbook/
6. TI-84 Plus statistics guidebook — https://education.ti.com/en/guidebook/details/en/6152F7C2E0B9491482D4CF5C3EEB6EB1/84plce
7. Numerical Recipes (regression, special functions) — Press, Teukolsky, Vetterling, Flannery
8. Feasibility report (original phase outline) — [../notes/feasibility.md](../notes/feasibility.md)
