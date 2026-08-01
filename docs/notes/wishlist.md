# Feature wishlist — desired, not yet planned

Features wanted but not scheduled into a phase. When scoping a new phase,
pull relevant items in, then move them to **Graduated** below with the target
sub-phase so we keep the provenance. Once a graduated item actually ships,
move it again to **Completed / Closed** with the as-built decision (D-number)
so we keep the provenance end to end. Keep this list short — it is not a
backlog of implementation tasks (those live in the phase specs / worklog),
only of features that don't yet have a home.

## Active (unscheduled)

- **Copy/paste in expression editors** (raised 2026-08-02, Pico 2 testdrive):
  no way to copy text between fields — e.g. duplicating one Y= expression
  into another slot means retyping it in full on the physical keypad. No
  design work done; unscoped.
- **3D plotting (surface graphs, $z = f(x,y)$)** (raised 2026-07-21): not
  on TI-83/84+ at all — this is TI-Nspire CX II CAS territory (rotatable
  wireframe/surface plots), similar in kind to CAS itself: a capability
  that separates the 84+ tier from the Nspire tier, not a parity gap.
  Would need a new graph mode, a projection/rotation math layer, and a
  different renderer path from the existing 2D column-based plotter
  (`graph/`, `render/`) — substantial scope, likely its own sub-phase or
  phase if ever picked up, not a small addition. No design work done;
  raised as a stretch idea only, no phase home yet.
- **Antialiased / higher-res font rendering (D31)**: the rasterized fonts
  (JuliaMono, Iosevka) read worse than the bitmap fonts at 8px 1bpp on the
  PicoCalc; antialiasing or a higher-resolution panel would likely help.
  Hardware-dependent, still fully unplanned. *(The "desktop emulator" half
  of this item graduated to Phase 6 §9 as a named candidate; see below.)*

## Graduated — now planned

- **Pi-multiple axis ticks + `▶Frac`/`▶Dec` fraction answers** (split off
  the old "Symbolic display" item) → Phase 4, sub-phase **4D** (see
  [phase4-spec.md](../phases/phase4-spec.md) §7.1, tasks 4D.2/4D.3).
- **Surd / exact-value display** (the other split-off piece of the old
  "Symbolic display" item, raised Session 10 eval): keeping $\sqrt{2}$ as
  $\sqrt{2}$ instead of a decimal → folded into **Phase 5** core scope on
  2026-07-21 (see [phase5-spec.md](../phases/phase5-spec.md) §10.1, tasks
  4D.23/4D.24) — it needs the `Expr` tree and `simplify()` Phase 5
  builds anyway, so there was no reason to leave it homeless once Phase 5
  existed. Unit/dimensional arithmetic (`3 m/s` staying symbolic) is a
  materially bigger feature and remains explicitly out of scope (Phase 5
  non-goals, §13).
- **True subscripts** (`Sₓ`, `σₓ`) in stats/inference display (Session 13,
  D24.9; the piece D31's Greek-letter pass left open) → Phase 4 **4D**
  (§7.1, task 4D.4).
- **Vertical centering for fraction expressions** → Phase 4 **4D** (§7.1,
  task 4D.5).
- **Scientific constants** (Session 13, D24.9) → Phase 4 **4D** (§7.4,
  task 4D.17).
- **Unit conversions** (Session 13, D24.9) → Phase 4 **4D** (§7.4, task
  4D.18) as a native catalog, not a later app — closes the gap while it's
  cheap; TI-84 CE ships these natively too, not as a sideloaded app.
- **Beyond 6 lists** (Session 13, D24.9; the "named lists" half of the old
  combined item) → Phase 4 **4D** (§7.3, task 4D.13), capped by whatever
  `ArrayStore` headroom actually allows — see phase4-spec.md open question
  P4-10. *(The "SD list-data files / CBL-CBR data-logger" half of the old
  item did **not** graduate — still unscoped, no phase home yet.)*
- **Auto power-off / standby after idle** → Phase 4 **4D** (§7.5, task
  4D.19), feasibility-check-first per the task table.
- **Remember screen brightness / keypad backlight setting** → Phase 4
  **4D** (§7.5, task 4D.20), feasibility-check-first per the task table.
- **Desktop emulator build** (the tooling half of the old antialiasing
  item) → named as a candidate in Phase 6 §9 (see
  [phase6-spec.md](../phases/phase6-spec.md)), still unscoped/not
  committed — listed there rather than shipped.

## Completed / Closed

- **Coarsen too-dense grid lines** (usage feedback 2026-07-25) → **shipped
  same day**, no phase/D-number (small, localized fix). When `Xscl`/`Yscl`
  is tiny relative to the axis range, `GraphScreen::draw_axes` coarsens the
  grid step to the smallest multiple of `scl` spaced >= 4 px, so a
  wide/tall window draws the largest meaningful grid (~80 lines/axis max)
  instead of thousands of merged lines — faster and legible, no visual
  change at normal scales. Tick labels snap to the coarsened grid step so
  they stay on grid lines. HW-confirmed on the Pico 1. Shared `thin_factor`
  helper in `src/apps/graph_screen.cpp`.
- **Complex numbers** → Phase 4 sub-phase **4C**, shipped as **D30** (2026-07-20,
  see [decisions.md](./decisions.md) D30 and
  [phase4-spec.md](../phases/phase4-spec.md) §5). Adds a `Complex` type,
  `complexexpr` home-screen evaluator, `i`/`2i` syntax, complex-aware function
  set, and a MODE row entry. Matrix eigenvalues can now return a full complex
  spectrum as a formatted (unstorable) string via the new `Kind::kText` result.
  **Scope not fully met**: the original wishlist item committed to "lists and
  matrices hold complex values too," but D30 deferred that — `Variables::vars`
  and list storage (l1..l6) stay `calc_t`/real-only, so `2i->a` and storing a
  complex eigenvalue spectrum both error "Complex results can't be stored."
  **Partially graduated**: complex-valued *variable/Ans* storage → Phase 4
  **4D** (task 4D.15, see [phase4-spec.md](../phases/phase4-spec.md) §7.3).
  Complex-valued *lists and matrices* did **not** graduate — flagged in
  [design-departures-matrix-complex.md](design-departures-matrix-complex.md)
  as needing a Pico 1 memory feasibility study first, bigger than a 4D
  closing-pass item. Still unscoped.
- **TI-84 CALC-menu graph analysis** (value, zero, min/max, intersect, dy/dx,
  numeric fnInt) → Phase 4 sub-phase **4B**, shipped as **D29** (2026-07-20,
  see [decisions.md](./decisions.md) D29 and
  [phase4-spec.md](../phases/phase4-spec.md) §4). Numeric + interactive on the
  graph screen, layered on the existing compiled-eval machinery.
- **JuliaMono font swap (Session 13, D24.9)** → shipped **D31** as a general
  font selector: `-DPICOCALC_FONT=spleen|juliamono|iosevka|unifont|terminus`
  (default terminus). Licensing handled (OFL, vendored).
