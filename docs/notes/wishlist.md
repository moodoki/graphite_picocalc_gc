# Feature wishlist — desired, not yet planned

Features wanted but not scheduled into a phase. When scoping a new phase,
pull relevant items in, then move them to **Graduated** below with the target
sub-phase so we keep the provenance. Once a graduated item actually ships,
move it again to **Completed / Closed** with the as-built decision (D-number)
so we keep the provenance end to end. Keep this list short — it is not a
backlog of implementation tasks (those live in the phase specs / worklog),
only of features that don't yet have a home.

## Active (unscheduled)

- **Fix tinyexpr's `(-2)^2 = -4`** (found 2026-08-09 by Phase 5.2's differential
  harness; **D50**, spec P5.2-6). `factor()` in the `TE_POW_FROM_RIGHT` build
  hoists a negation out of a power without knowing whether parentheses closed
  it, so `(-2)` and `-2` are the same node by the time it runs. The two shipped
  evaluators therefore **disagree on this input today** — `-4` from tinyexpr,
  `4` from `complexexpr` — and which one you get depends on the number mode.
  Phase 5.2 fixes the home screen; only patching the vendored parser fixes
  graphing, tables and the solver. ~5 lines, parse-time only, no hot-loop cost.
  Deliberately *not* folded into 5.2: it is a bugfix that stands alone and is
  not gated on that phase. Listed here so it survives 5.2's closure — see
  [unified-evaluator-changes.md](unified-evaluator-changes.md) F1 for the
  mechanism and the reproduction.

- **Replace tinyexpr with the unified evaluator on the numeric path too**
  (raised 2026-08-09, **D50**, spec P5.2-7). Would make it four parsers → *one*,
  fix the above everywhere, remove tinyexpr's depth-7 parse cap from graphing
  (D47) and return 7,897 B of flash. **Revisit after Phase 5.2 closes**, and not
  before §9's M1 has measured per-sample latency for the stack machine against
  tinyexpr — that number is the one input the decision needs and nobody has it.
  Known costs, measured rather than guessed: a `Program` is a fixed 2,064 B
  against a malloc'd tree of ~120 B for `sin(x)+2*x`, so caching Y1-Y7 compiled
  would cost 14.4 KB against ~1 KB; `compile()`/`run()` are non-reentrant
  singletons and the numeric path re-enters them; the evaluator's chunk staging
  overlays the `kCompute` arena that `stats`/`matrix`/`infer` own; and there is
  no differential corpus off the home screen, where 5.2.9 found three bugs
  inside covered territory. Note that `phase4-spec.md` §5.2's "would double
  arithmetic cost" does **not** argue against this — it argued against a
  `Complex` numeric value type, and this evaluator keeps a real tier.

- **Screenshot capture — serial dump (debug aid) + save-to-SD (user feature)**
  (raised 2026-08-05, same session as serial key injection — two uses of the
  same underlying capability; that item has since graduated to **Phase 5.1**,
  and note its scoping found that *this* item is **not** a prerequisite for
  reading result colour, since `HomeScreen::ResultKind` already encodes it). Frame is 320x320 RGB565 (200 KB raw),
  identical resolution/format on both boards, but the capture path differs
  sharply by board: **Pico 2** holds a complete frame in SRAM at once
  (`frame_buf`, `src/gfx/framebuffer.cpp:17`) with a genuinely stable window
  to read it — right after `render()` returns and before the next frame's
  `drain_acks()` lets core 0 overwrite it again
  (`framebuffer.cpp:104-109`). **Pico 1 never has a full frame in SRAM at
  all** — it renders in 16-row strips (`config::kStripHeight`, ~20
  calls/frame) into two 10 KB ping-pong buffers; a screenshot there means
  accumulating each finished strip (grabbed right before `submit()`, not by
  re-entering `render()`, to respect the idempotent-`render()` strip
  contract in `phase3-spec.md` §8) into a scratch region — SRAM doesn't have
  200 KB of spare headroom on Pico 1, so PSRAM (not memory-mapped, only
  `read()`/`write()` via PIO/SPI, `psram.hpp:38-39`, ~6.8 MB/s HW-measured
  bulk throughput — ~29 ms for a full frame) is close to mandatory as the
  accumulator there, optional on Pico 2. Both boards' core-1 display-push
  path is `__not_in_flash_func` (RAM-resident) per D10, because running it
  from flash while core 0's USB/TinyUSB stack is active hard-faults the chip
  (shared XIP cache contention) — any new capture code touching the
  display/DMA path from core 1 while USB is live must stay RAM-resident
  too, same landmine D10 already fought.
  - **Debug-aid variant (serial dump)**: stream the captured frame out over
    `stdio_usb` (already enabled, output-only today — printf diagnostics
    only, nothing reads stdin, no existing screenshot code anywhere in the
    repo including the vendored `picocalc_diag` bring-up target). Actual
    `stdio_usb` throughput is **undocumented/unmeasured anywhere in this
    project** — needs a real bench number before committing to raw dump vs.
    an encoding; given calculator screens are mostly sparse/few-color
    (`display.hpp:20-34`'s small fixed palette), simple RLE is very likely
    worth it over a raw 200 KB dump, but that's inference, not measured.
    Host-side capture script would need to assert DTR/RTS the same way
    `scripts/serial-capture.py` already does for other serial reads, or
    output will silently drop.
  - **User-facing variant (save to SD)**: write the captured frame to
    storage as a file (format/encoding TBD — raw RGB565 dump, or convert to
    a standard image format like BMP/PPM for viewing off-device without
    custom tooling). No existing SD image-write path to build on; would
    follow the existing `Storage`/persistence conventions used elsewhere
    (list/matrix/graph-state files) for the write side, but the pixel
    encoding itself is new design work.
  - No prior design work on either variant before this session; no phase
    home.
- **Inverse-trig exact forms** (raised 2026-08-05, Pico 2 Stage 5 testdrive):
  `asin(1)` shows `1.570796327` where the forward direction already shows
  `sin(pi/6)` as `1/2`. D44 built a *forward* special-angle table only
  (`src/math/cas/exact.cpp`, 24 entries indexed in twelfths of $\pi$), so
  nothing recognizes `asin(1)` as $\pi/2$ or `atan(1)` as $\pi/4$. The
  symmetric completion needs its own table over the inverse arguments
  ($0$, $\pm 1/2$, $\pm\sqrt{2}/2$, $\pm\sqrt{3}/2$, $\pm 1$ for asin/acos;
  $0$, $\pm\sqrt{3}/3$, $\pm 1$, $\pm\sqrt{3}$ for atan), angle-mode
  awareness (in DEGREE, `asin(1)` is a plain `90` and correctly stays
  white), and its own tests — comparable in size to D44. Deliberately
  deferred out of Phase 5 Stage 5 rather than grown into a hardening
  session; no design work beyond this note.
- **Say *why* an editor field is invalid, not just colour it red** (raised
  2026-08-08, Pico 1 testdrive). Today `SlotEditorScreen::render()` draws a
  row white or red off a single cached bool (`valid_mask_`, D47), and
  `field_valid()` → `Engine::compile()` throws the reason away — the engine
  returns `nullptr` for every failure mode alike, so the UI genuinely does
  not know whether it is a syntax error, an unknown identifier, a non-real
  variable, or (since D47) an expression nested past `kMaxParseDepth`. The
  trigger was exactly that ambiguity: after the D47 fix Y1 sat red with no
  hint that the cause was nesting depth. Same gap in the list, matrix and
  seq editors.
  - Shape: give the compile path an out-parameter for a static reason
    string. tinyexpr already hands back an error *offset* from
    `te_compile`'s `int *error`, which `Engine::compile` currently discards
    (`engine.cpp`) — that would also allow pointing at the offending
    character, not just naming the problem.
  - Display is the harder half on a 320x320 panel: rows are 26 px and the
    expression text already truncates with an ellipsis before the enable
    checkbox. Likely a status line at the bottom for the *selected* row
    only, rather than per-row text.
  - Note the D47 constraint: whatever this does must not put the compiler
    back inside `render()`. The reason string has to be cached alongside
    the valid bit, refreshed from `on_activate()`/`on_key`.
- **Crosshair (horizontal line) in trace mode** (raised 2026-08-08, Pico 1
  testdrive, as a question — "is trace supposed to show a horizontal line
  too?"). It is not: `draw_trace` renders a full-height *vertical* line plus
  a 5x5 cursor square in the slot's colour (`graph_screen.cpp:1102-1105`,
  and the param/polar/seq path at `:1228-1231`). No decision ever specified
  a crosshair, so this is unimplemented rather than broken. Adding the
  horizontal arm is small — one `draw_hline` at the cursor row, skipped when
  the point is offscreen — but worth judging on device first: the panel is
  320 px and a full-width line may read as clutter against the grid, so a
  short arm around the cursor, or a MODE toggle, may be better than a full
  crosshair. TI-84 itself draws neither: it flashes a small cursor on the
  curve with the readout at the bottom, which is closer to what this already
  does.
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

- **Serial key injection for on-device test automation** (raised
  2026-08-05, Pico 1 testdrive) -> **Phase 5.1** (see
  [phase5.1-spec.md](../phases/phase5.1-spec.md), tasks 5.1.1-5.1.6),
  scoped 2026-08-08 to the line-oriented variant. Per-keystroke `KeyEvent`
  synthesis stays deferred with an explicit revival trigger (that spec's
  section 7). Two findings closed the gap between "idea" and "planned": the
  sibling screenshot item below is **not** a prerequisite, because
  `HomeScreen::ResultKind` (`home_screen.hpp:36`) already encodes
  white/amber/error and can simply be printed; and flashing no longer needs
  the BOOTSEL button (`picotool load -f -x`), leaving keyboard input as the
  last manual step in the bench loop. Motivated by D48, whose bench work
  needed ~15 hand round-trips to land one integer.
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
