# Phase 2 Spec: Graph Modes & Table View

**Prerequisite**: Phase 1 (HAL + scientific calculator + Cartesian function graphing with trace/zoom/window).

**Scope**: Extend the graphing system with a table view (auto and ask modes), parametric graphing, polar graphing, and a split-screen graph|table mode. This phase broadens *how* functions are expressed and displayed without adding new math domains — it builds directly on the Phase 1 graph engine.

**End state**: the calculator graphs Cartesian, parametric, and polar functions ($\theta$-based); shows a value table for any graph mode with both automatic and manual $x$/$T$/$\theta$ entry; and can display a graph and its table side by side.

**Estimated effort**: ~6 weeks part-time (~110 hours).

---

## 1. Relationship to Phase 1

Phase 1 delivered a Cartesian graphing engine: `GraphScreen`, `YEditorScreen`, `WindowScreen`, a plotting loop that evaluates $Y_n(x)$ across pixel columns, trace, zoom presets, and discontinuity detection. Phase 2 **generalizes** this rather than replacing it.

The core change is introducing a **graph mode** abstraction. Phase 1 implicitly assumed one mode (function/Cartesian). Phase 2 makes mode explicit and adds two more:

| Mode | Independent var | Functions | Plot form |
|------|:---:|:---:|---|
| Function (Phase 1) | $x$ | $Y_1 \ldots Y_7$ | $(x, Y_n(x))$ |
| Parametric (new) | $t$ | $(X_{nT}, Y_{nT})$ pairs | $(X_{nT}(t), Y_{nT}(t))$ |
| Polar (new) | $\theta$ | $r_1 \ldots r_6$ | $(r_n(\theta)\cos\theta, r_n(\theta)\sin\theta)$ |

All three share the same underlying machinery: expression evaluation (`math::Engine`), the viewport transform (data coords → pixel coords), line-segment rendering, discontinuity detection, trace, and zoom. Phase 2 refactors that machinery to be mode-agnostic, then adds the two new modes on top.

---

## 2. New and modified source files

```
src/
├── apps/
│   ├── graph_screen.hpp / .cpp     # MODIFIED: mode-aware plotting
│   ├── y_editor.hpp / .cpp         # MODIFIED: mode-aware function editor
│   ├── window_screen.hpp / .cpp    # MODIFIED: mode-specific window params
│   ├── table_screen.hpp / .cpp     # NEW: table view (auto + ask)
│   ├── table_setup.hpp / .cpp      # NEW: TblStart, ΔTbl, ask/auto config
│   ├── split_screen.hpp / .cpp     # NEW: graph|table split layout
│   └── help_screen.hpp / .cpp      # NEW: built-in help browser (§10)
├── graph/                          # NEW: extracted graphing subsystem
│   ├── graph_mode.hpp              # Mode enum + mode descriptor
│   ├── plotter.hpp / .cpp          # Mode-agnostic plotting loop
│   ├── viewport.hpp / .cpp         # Data↔pixel coordinate transform
│   ├── trace.hpp / .cpp            # Mode-aware trace cursor
│   └── graph_state.hpp / .cpp      # Active functions, window, mode (persisted)
└── math/
    ├── engine.hpp / .cpp           # MODIFIED: parameterized sweep variable (t, θ)
    └── catalog.hpp / .cpp          # NEW: function descriptor table (drives help, §10)
```

The `graph/` subdirectory is new — Phase 1's plotting logic (currently inside `apps/graph_screen.cpp`) is **extracted** into a reusable subsystem. This is a refactor that should happen first (task 2.1), before new modes are added.

---

## 3. Graph mode abstraction (`graph/graph_mode.hpp`)

```cpp
namespace graph {

enum class Mode : uint8_t {
    FUNCTION,    // y = f(x)
    PARAMETRIC,  // x = f(t), y = g(t)
    POLAR,       // r = f(theta)
};

// Describes how a mode maps parameters to plotted points.
struct ModeDescriptor {
    Mode mode;
    char independent_var;   // 'x' or 't'; polar sweeps the theta slot (see §14)
    int  slot_count;        // 7 (function/polar) or 6 (parametric pairs)
    const char* slot_prefix;// "Y", "r", or "" (parametric uses X/Y pairs)
};

const ModeDescriptor& descriptor_for(Mode m);

}  // namespace graph
```

The mode affects:

- **What the Y-editor shows**: `Y_n =` fields (function), `(X_{nT}, Y_{nT})` field pairs (parametric), or `r_n =` fields (polar).
- **Which window parameters exist**: function/polar need x/y ranges; parametric and polar also need a parameter range ($T_{\min}, T_{\max}, T_{\text{step}}$ / $\theta_{\min}, \theta_{\max}, \theta_{\text{step}}$).
- **How the plotter iterates**: over pixel columns (function) or over the parameter with a step (parametric/polar).

---

## 4. Extracted plotting subsystem (`graph/plotter.hpp`)

Refactor Phase 1's plotting loop into a mode-agnostic form.

```cpp
namespace graph {

struct PlotStyle {
    platform::Color color;
    bool thick = false;     // 2px line for emphasis (trace target)
};

// A source of points to plot. Each mode implements this by
// producing (x_data, y_data) pairs as the parameter advances.
class PointSource {
public:
    virtual ~PointSource() = default;

    // Reset iteration to the start of the parameter range.
    virtual void begin(const Viewport& vp) = 0;

    // Produce the next data-space point. Returns false when the
    // parameter range is exhausted. Sets *defined=false when the
    // function is undefined at this parameter (e.g., 1/0), which
    // triggers a pen-up (no connecting segment).
    virtual bool next(double* x_data, double* y_data, bool* defined) = 0;
};

// The plotting loop: consumes a PointSource, transforms each point
// to pixel space via the Viewport, and draws connected segments
// with discontinuity detection.
class Plotter {
public:
    void plot(gfx::Framebuffer& fb, const Viewport& vp,
              PointSource& source, const PlotStyle& style);

private:
    // Discontinuity heuristic (from Phase 1): if |dy_pixels| exceeds
    // a threshold between adjacent points, lift the pen.
    static constexpr int kDiscontinuityThreshold = 140;  // pixels
};

}  // namespace graph
```

Three `PointSource` implementations:

- **`FunctionSource`**: iterates $x$ across the 320 pixel columns, evaluates $Y_n(x)$. This is Phase 1's exact behavior, now wrapped.
- **`ParametricSource`**: iterates $t$ from $T_{\min}$ to $T_{\max}$ in $T_{\text{step}}$ increments, evaluates $X_{nT}(t)$ and $Y_{nT}(t)$.
- **`PolarSource`**: iterates $\theta$ from $\theta_{\min}$ to $\theta_{\max}$ in $\theta_{\text{step}}$ increments, evaluates $r_n(\theta)$, converts to Cartesian $(r\cos\theta, r\sin\theta)$.

The `Viewport` (extracted from Phase 1's window handling) owns the data↔pixel transform:

```cpp
namespace graph {

class Viewport {
public:
    // Window bounds in data space
    double x_min, x_max, y_min, y_max;

    // Map data coordinates to pixel coordinates (within the graph area).
    int   px_x(double x_data) const;
    int   px_y(double y_data) const;

    // Inverse: pixel to data (for trace, cursor readout)
    double data_x(int px) const;
    double data_y(int py) const;

    // Is a data point within the visible window?
    bool visible(double x_data, double y_data) const;
};

}  // namespace graph
```

---

## 5. Parametric mode

### 5.1 Editor

The Y-editor in parametric mode shows six pairs:

```
┌──────────────────────────────────┐
│  Parametric Editor                │
├──────────────────────────────────┤
│  X₁ᴛ = cos(t)          [✓]       │
│  Y₁ᴛ = sin(t)          [✓]       │
│                                    │
│  X₂ᴛ = t                [ ]       │
│  Y₂ᴛ = t^2              [ ]       │
│                                    │
│  X₃ᴛ =                  [ ]       │
│  Y₃ᴛ =                  [ ]       │
├──────────────────────────────────┤
│ F1:EDIT F2:SEL F3:CLEAR F4:GRAPH │
└──────────────────────────────────┘
```

A pair is "enabled" only if both $X_{nT}$ and $Y_{nT}$ are non-empty and the checkbox is on. Enabling one auto-focuses the paired field.

### 5.2 Window parameters

Parametric mode adds a parameter range to the standard x/y window:

```
Tmin = 0        Tmax = 2π
Tstep = 0.1     (parameter increment)
Xmin = -2       Xmax = 2
Ymin = -2       Ymax = 2
Xscl = 1        Yscl = 1
```

$T_{\text{step}}$ controls plot smoothness: smaller = smoother curve but more evaluations. Default $2\pi / 63 \approx 0.0997$ (matching TI's default of ~63 steps over a $2\pi$ range).

### 5.3 Evaluation

`ParametricSource` uses the engine's compile-once path (`Engine::compile` / `eval_compiled`) — the same one Phase 1 graphing uses to avoid re-parsing per point. As built, `eval_compiled` hardcodes X as the swept variable; task 2.4 parameterizes the swept variable slot so parametric sweeps drive `t` (and polar drives `theta`, see §6.3). All 26 letters are already bound as variables in `build_lookup`, so no parser change is needed — only the sweep-write site. Per-point `evaluate_at` is not an option: it re-parses on every call.

---

## 6. Polar mode

### 6.1 Editor

Six polar functions $r_1 \ldots r_6$:

```
┌──────────────────────────────────┐
│  Polar Editor                     │
├──────────────────────────────────┤
│  r₁ = 1 + cos(θ)        [✓]       │  (cardioid)
│  r₂ = 2*sin(3*θ)        [ ]       │  (rose)
│  r₃ =                   [ ]       │
│  ...                              │
├──────────────────────────────────┤
│ F1:EDIT F2:SEL F3:CLEAR F4:GRAPH │
└──────────────────────────────────┘
```

The independent variable is $\theta$, typed as `theta` (a dedicated key binding is open question P2-2). Phase 1's engine already binds `theta` to its own variable slot (`Variables::kTheta`), distinct from the letter variable `t` — the polar sweep writes that slot. The editor displays the symbol $\theta$ while storing the expression text with `theta`.

### 6.2 Window parameters

```
θmin = 0        θmax = 2π
θstep = 0.05    (angle increment)
Xmin = -3       Xmax = 3
Ymin = -3       Ymax = 3
```

### 6.3 Evaluation and conversion

`PolarSource` evaluates $r = f(\theta)$, then converts to Cartesian for plotting: $x = r\cos\theta$, $y = r\sin\theta$. The angle-mode setting (degree/radian from Phase 1) applies — in degree mode, $\theta$ ranges and trig use degrees.

---

## 7. Table view

### 7.1 Table setup (`apps/table_setup.hpp`)

Configuration screen (TI's TBLSET equivalent, modernized):

```
┌──────────────────────────────────┐
│  Table Setup                      │
├──────────────────────────────────┤
│  Start:  0                        │
│  Step:   1                        │
│                                    │
│  Independent:  ● Auto   ○ Ask     │
│  Dependent:    ● Auto             │
├──────────────────────────────────┤
│ F1:SAVE  F2:CANCEL                │
└──────────────────────────────────┘
```

```cpp
namespace graph {

// Lives in graph/ (not apps/) so GraphState can hold it without the
// graph layer referencing upward into apps — see §14.
struct TableConfig {
    double start = 0.0;      // First value of independent var
    double step  = 1.0;      // Increment (auto mode)
    bool   ask_mode = false; // true = user enters each x; false = auto-generate
};

}  // namespace graph
```

### 7.2 Table screen (`apps/table_screen.hpp`)

Displays a scrollable table. Columns adapt to the active graph mode:

- **Function mode**: `x | Y1 | Y2 | ...` (one column per enabled function)
- **Parametric mode**: `T | X1T | Y1T | ...`
- **Polar mode**: `theta | r1 | r2 | ...`

```
┌──────────────────────────────────┐
│    x        Y₁        Y₂          │
├──────────────────────────────────┤
│   -2.00     1.00     -3.00        │
│   -1.00    -2.00     -1.00        │
│    0.00    -3.00      1.00        │
│    1.00    -2.00      3.00   ◄    │  (highlighted row)
│    2.00     1.00      5.00        │
│    3.00     6.00      7.00        │
├──────────────────────────────────┤
│ F1:SETUP  F2:Δx  F3:GRAPH  F4:G-T│
└──────────────────────────────────┘
```

**Auto mode behavior**:

- Rows generated from `start`, `start + step`, `start + 2·step`, ...
- Scrolling up/down extends the table dynamically (regenerates around the visible window). No fixed row count — infinite scroll in both directions.
- The independent-variable column is read-only.

**Ask mode behavior**:

- The table starts empty with an editable independent-variable cell.
- The user types an $x$ value; pressing Enter evaluates all enabled functions for that row and adds it.
- Rows accumulate as entered. This is useful for spot-checking specific inputs.

**Shared behavior**:

- Highlighted (selected) row shows full-precision value in a detail line if the cell is truncated.
- Cursor navigation with arrow keys; `LEFT`/`RIGHT` scrolls horizontally when there are more function columns than fit.
- Values formatted via Phase 1's `math::format_number()`.

### 7.3 Table generation

The table engine reuses the same `PointSource` evaluation, but instead of iterating for plotting, it evaluates at discrete table points:

```cpp
namespace apps {

// Evaluate all enabled functions at a single independent value.
// Fills `results[]` with one value per enabled function/slot.
// Returns the number of columns filled.
int evaluate_table_row(const graph::GraphState& state,
                       double independent_value,
                       double* results, int max_results);

}  // namespace apps
```

For parametric mode, a table row shows the $T$ value plus both $X_{nT}$ and $Y_{nT}$ for each enabled pair.

---

## 8. Split-screen graph|table

A mode where the graph occupies the left half and the table the right half (or top/bottom — decide during implementation, see open questions).

```
┌─────────────────┬────────────────┐
│                 │   x      Y₁     │
│   graph         │ -2.0    1.0     │
│   viewport      │ -1.0   -2.0     │
│   (left half)   │  0.0   -3.0  ◄  │
│                 │  1.0   -2.0     │
│                 │  2.0    1.0     │
├─────────────────┴────────────────┤
│ F1:SETUP F2:MODE F3:FULL-G F4:F-T│
└──────────────────────────────────┘
```

**Behavior** (matches TI-84's G-T mode):

- Trace on the graph moves the highlighted row in the table in sync — tracing to $x = 1$ highlights the $x = 1$ table row.
- The table's selected row places a trace cursor on the graph at that point.
- The split can be vertical (side by side) or horizontal (stacked). TI-84 uses vertical; we'll default to vertical given the square $320\times320$ display gives each half a $160\times280$ area.

### 8.1 Split-screen implementation

The `SplitScreen` composites two sub-views by giving each a clip rectangle:

```cpp
namespace apps {

class SplitScreen : public ui::Screen {
public:
    void on_activate() override;
    bool on_key(const platform::KeyEvent& ev) override;
    void render(gfx::Framebuffer& fb) override;

private:
    GraphScreen graph_view_;   // Renders into left clip rect
    TableScreen table_view_;   // Renders into right clip rect
    bool graph_focused_ = true;// Which pane has input focus

    static constexpr int kSplitX = 160;  // Vertical divider
};

}  // namespace apps
```

Each sub-view's `render()` is given a clip rectangle so it draws only within its half. Phase 1's framebuffer clips only *vertically*, to the active strip (`clip_y0`/`clip_y1`) — there is no per-pane clip rectangle, and a vertical split also needs horizontal bounding. Task 2.19 therefore adds a clip-rect (set/clear) to `gfx::Framebuffer` that composes with the strip window; the primitives are already written to clip, so this is a bounds-intersection change, not a redesign. Input routes to the focused pane; a key (e.g., `F2:MODE` or a dedicated toggle) switches focus between panes.

Trace synchronization is the one piece of shared state: both panes reference the same "current independent value," and updating it in either pane refreshes both.

---

## 9. Graph state and persistence (`graph/graph_state.hpp`)

Phase 1 persisted Y-functions and window settings. Phase 2 generalizes this to cover all three modes plus table config.

```cpp
namespace graph {

struct GraphState {
    Mode mode = Mode::FUNCTION;

    // Function-mode slots (Phase 1)
    char y_funcs[7][config::kMaxExprLen];
    bool y_enabled[7];

    // Parametric-mode slots (new)
    char param_x[6][config::kMaxExprLen];
    char param_y[6][config::kMaxExprLen];
    bool param_enabled[6];

    // Polar-mode slots (new)
    char polar_funcs[6][config::kMaxExprLen];
    bool polar_enabled[6];

    // Windows (mode-specific ranges)
    double x_min, x_max, y_min, y_max, x_scl, y_scl;
    double t_min, t_max, t_step;       // parametric
    double theta_min, theta_max, theta_step;  // polar

    // Table config
    TableConfig table;

    // Persistence
    bool save(platform::Storage& storage) const;
    bool load(platform::Storage& storage);
};

}  // namespace graph
```

Persisted to `/picocalc/graphstate.dat` (binary) — supersedes Phase 1's separate `yfuncs.txt` and `window.dat` (migration: read old files if present, write unified format going forward).

---

## 10. Built-in help

The device ships with no manual, and the 2026-07-11 hardware test drive showed the cost: function names (`ncr`, the store operator `->`, `theta`), constants, and per-screen key bindings are not discoverable on the device. Phase 2 adds a small built-in help browser.

Three content areas, presented as tabs in one `HelpScreen`:

1. **Function catalog** — every function callable from the expression parser, with its signature and a one-line summary. The content comes from a static descriptor table (`math/catalog.hpp`) that the engine's `build_lookup()` registration *also* consumes — one source of truth, so the catalog cannot drift from the parser as Phase 3 adds distribution/stats functions.
2. **Key reference** — per-screen key map: softkeys, global keys (F6 diagnostics, Home), editing keys.
3. **Syntax notes** — store operator `expr->A`, `ans`, constants (`pi`, `e`), history recall, angle mode.

```cpp
namespace math {

// One row per parser-callable function. Drives both engine
// registration (build_lookup) and the help catalog.
struct FnDescriptor {
    const char* name;       // "ncr"
    const char* signature;  // "ncr(n, r)"
    const char* summary;    // "Combinations: n choose r"
    const void* fn;         // Binding for build_lookup
    int arity;              // 0..2 (TE_FUNCTION0..2)
};

// The full catalog, in display order.
const FnDescriptor* catalog(int* count);

}  // namespace math
```

UI: `HelpScreen` in `apps/` — UP/DOWN scrolls, LEFT/RIGHT switches tabs, ESC exits. Entry point: the Home screen's F5 softkey, unassigned in Phase 1. All help content is compiled in (static strings in flash) — no SD dependency, since the SD card is optional on-device.

---

## 11. Task breakdown

Estimated as solo developer, part-time (~20 hrs/week). ~6 weeks.

### Week 11–12: Refactor + parametric

| # | Task | Est. hrs | Notes |
|---|------|---|---|
| 2.1 | Extract `graph/` subsystem from Phase 1 `graph_screen` (Viewport, Plotter, PointSource) | 8 | Refactor; no behavior change. Verify function mode still works. |
| 2.2 | `graph::Mode` abstraction + mode-aware `GraphState` | 4 | |
| 2.3 | `FunctionSource` (wrap Phase 1 behavior) | 2 | |
| 2.4 | `ParametricSource` + engine `t` binding | 4 | |
| 2.5 | Parametric editor (6 X/Y pairs) | 6 | |
| 2.6 | Parametric window params (Tmin/Tmax/Tstep) | 3 | |
| 2.7 | Parametric plotting + trace | 4 | |
| | **Subtotal** | **~31 hrs** | |

**Acceptance**: parametric curves (circle, Lissajous) plot correctly with trace.

### Week 13: Polar

| # | Task | Est. hrs | Notes |
|---|------|---|---|
| 2.8 | `PolarSource` + polar→Cartesian conversion | 4 | |
| 2.9 | Polar editor (6 r functions, $\theta$ input) | 5 | |
| 2.10 | Polar window params ($\theta$min/$\theta$max/$\theta$step) | 3 | |
| 2.11 | Polar plotting + trace + angle-mode handling | 4 | |
| | **Subtotal** | **~16 hrs** | |

**Acceptance**: polar curves (cardioid, rose) plot correctly; degree/radian modes both work.

### Week 14–15: Table view

| # | Task | Est. hrs | Notes |
|---|------|---|---|
| 2.12 | `TableConfig` + table setup screen | 4 | |
| 2.13 | `evaluate_table_row` (mode-aware) | 4 | |
| 2.14 | Table screen — auto mode with infinite scroll | 8 | |
| 2.15 | Table screen — ask mode | 4 | |
| 2.16 | Horizontal scroll for many function columns | 3 | |
| 2.17 | Multi-column layout for parametric (T/X/Y) | 3 | |
| 2.18 | Persist table config in `GraphState` | 2 | |
| | **Subtotal** | **~28 hrs** | |

**Acceptance**: table shows values for all three graph modes; auto and ask modes both work.

### Week 16: Split-screen, help + polish

| # | Task | Est. hrs | Notes |
|---|------|---|---|
| 2.19 | `SplitScreen` composite layout (clipped panes) | 6 | Incl. new clip-rect in `gfx::Framebuffer` (§8.1) |
| 2.20 | Graph↔table trace synchronization | 6 | |
| 2.21 | Pane focus switching | 2 | |
| 2.22 | Mode selector integration (Function/Param/Polar) | 3 | |
| 2.23 | Migrate Phase 1 persistence → unified `GraphState` | 2 | |
| 2.24 | Test all modes on both Pico 1 and Pico 2 | 4 | |
| 2.25 | Performance check (parametric/polar step tuning) | 3 | |
| 2.26 | `math::catalog` descriptor table, wired into `build_lookup` | 3 | Single source of truth (§10) |
| 2.27 | `HelpScreen` — tabs (Functions/Keys/Syntax) + scrolling | 4 | Entry: Home F5 |
| 2.28 | Key-reference and syntax help content | 2 | Reflect final Phase 1 polish keymap |
| | **Subtotal** | **~35 hrs** | |

**Acceptance**: split-screen G-T works with synchronized trace; mode switching is seamless. Help browser opens from Home F5; every function registered in the engine appears in the catalog.

### Summary

| Week range | Subtotal | Deliverable |
|------------|---------|-------------|
| Week 11–12 | ~31 hrs | Refactor + parametric graphing |
| Week 13 | ~16 hrs | Polar graphing |
| Week 14–15 | ~28 hrs | Table view (auto + ask) |
| Week 16 | ~35 hrs | Split-screen + built-in help + integration |
| **Total** | **~110 hrs** | Full multi-mode graphing + table + help |

---

## 12. Performance considerations

Parametric and polar modes evaluate functions at every parameter step, not every pixel column. A $2\pi$ range at $\theta_{\text{step}} = 0.05$ is ~126 evaluations — cheaper than function mode's 320. But small step values (for smooth curves) increase this: $\theta_{\text{step}} = 0.01$ over $4\pi$ is ~1257 evaluations.

Phase 1's graph-profiling numbers (task 5.6) are still HW-PENDING, so there is no measured evals/sec figure yet. Even pessimistically assuming ~100K compiled evals/sec on Pico 1 softfloat, 1257 evaluations is ~13 ms — display rendering (~200 ms full-frame as measured in bring-up) dominates either way. No special optimization needed, but expose `Tstep` / $\theta_{\text{step}}$ to the user so they can trade smoothness for speed on very complex curves.

Split-screen renders two views per frame. With line-buffer rendering, each pane covers half the strips, so total pixel work is unchanged — the cost is re-traversing two view trees. Budget for ~$1.5\times$ a single-view frame time. Still comfortably interactive.

---

## 13. Open questions for Phase 2

| # | Question | Options | Resolution (2026-07-12) |
|---|----------|---------|------------|
| P2-1 | Split-screen orientation: vertical (side by side) or horizontal (stacked)? | Vertical matches TI-84; horizontal may read better on square display | **Horizontal** (D16): full-width graph keeps column caches/trace intact |
| P2-2 | Should $\theta$ have a dedicated physical key, or be typed as `theta` / entered via 2nd-menu? | Depends on keyboard layout available bindings | Typed as `theta`; revisit with the F-key layout rethink |
| P2-3 | Table infinite-scroll regeneration window size (how many rows to buffer above/below visible)? | Affects scroll smoothness vs. memory | No extra buffer: regenerate the visible window per scroll step |
| P2-4 | Unified `GraphState` binary format versioning — add a version byte for future migration? | Yes recommended; costs 1 byte | Yes, via the magic tag ("PCG1"→"PCG2") + size guard |
| P2-5 | In parametric table, show one row per T with X and Y columns, or separate X-table and Y-table? | One row per T is more compact | One row per T with X/Y column pairs |
| P2-6 | Help entry point: Home F5 softkey only, or a global key reachable from any screen (like F6 diagnostics)? | F5 is free today; global is more discoverable but burns a key everywhere | Home F5 only |

---

## 14. Reconciliation notes

- **Persistence migration**: Phase 1 wrote `yfuncs.txt` and `window.dat`. Phase 2's unified `GraphState` supersedes them. Task 2.23 handles reading the old format once and writing the new one going forward. After Phase 2, the old files can be ignored.
- **Trace subsystem**: Phase 1's trace was function-mode-specific. Task 2.1 generalizes it into `graph/trace.hpp` as part of the extraction. This is a refactor of existing code, budgeted within task 2.1.
- **No new math domains**: Phase 2 adds no new mathematical capability to `math::Engine`. The parser already binds `t` (letter variable) and `theta` (dedicated slot); the only engine change is parameterizing which slot the compiled-eval sweep writes (task 2.4) — Phase 1's `eval_compiled` hardcodes X. This keeps Phase 2 low-risk relative to Phase 3 (statistics) and Phase 4 (CAS).
- **gfx clip-rect**: Phase 1's framebuffer clips only to the active strip (vertical). Split-screen needs a real clip rectangle; task 2.19 adds it as a bounds intersection in the existing clipped primitives (see §8.1).
- **Layering**: the new `graph/` subsystem sits below `apps/` (apps → graph → math/gfx). Nothing in `graph/` may include from `apps/` — which is why `TableConfig` lives in `graph/` (§7.1).
- **Phase 1 polish carry-over**: the 2026-07-11 test-drive items (grid color, diag-screen exit, input-history navigation, Home key, `e` constant) are Phase 1 polish expected to land before Phase 2 starts. The help key-reference content (task 2.28) must document whatever keymap those decisions produce, and the graph grid-color fix supersedes nothing here — the plotter extraction (2.1) carries the corrected colors forward.

---

## 15. References

1. Phase 1 spec — [phase1-spec.md](phase1-spec.md)
2. TI-84 Plus parametric/polar graphing guidebook — https://education.ti.com/en/guidebook/details/en/6152F7C2E0B9491482D4CF5C3EEB6EB1/84plce
3. Delta Pico graphing engine — https://github.com/AaronC81/delta-pico
4. Feasibility report (original phase outline) — [../notes/feasibility.md](../notes/feasibility.md)
