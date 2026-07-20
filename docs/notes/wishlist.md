# Feature wishlist — desired, not yet planned

Features wanted but not scheduled into a phase. When scoping a new phase,
pull relevant items in, then move them to **Graduated** below with the target
sub-phase so we keep the provenance. Once a graduated item actually ships,
move it again to **Completed / Closed** with the as-built decision (D-number)
so we keep the provenance end to end. Keep this list short — it is not a
backlog of implementation tasks (those live in the phase specs / worklog),
only of features that don't yet have a home.

## Active (unscheduled)

- **Symbolic display (KIV, raised Session 10 eval)**: pi-multiple axis ticks,
  surd displays, fraction/pi-fraction answers. Phase 3/4 polish family; the
  4-sig-digit tick cap is the current stopgap.
- **Greek/typographic stats display (Session 13, D24.9)** — *mostly shipped
  D31*: greek `σ`/`μ`/Σ and the ² superscript now render in the stats/inference/
  distribution results. **Still open: true subscripts** (`Sₓ`, `σₓ`) — needs a
  subscript story in the text renderer, not just baked glyphs.
- **Antialiased / higher-res / desktop-emulator font rendering (D31)**: the
  rasterized fonts (JuliaMono, Iosevka) read worse than the bitmap fonts at
  8px 1bpp on the PicoCalc; they'd likely look good with antialiasing, a
  higher-resolution panel, or a desktop emulator build (which would also help
  the D-prelude-2 "third target" note). All unplanned — revisit together.
- **Scientific constants (Session 13, D24.9)**: c, h, N_A, ... Easy to fold
  into any session once the exposure surface is decided (catalog entries vs. a
  `const` command vs. a 2nd-key layer) — decide at the start of that session.
- **Unit conversions (Session 13, D24.9)**: as an app later, or
  scientific-calculator-style conversion pairs. Design effort needed; deferred.
- **Beyond 6 lists + SD list-data files (Session 13, D24.9)**: named lists
  and loading list data from files on the SD card; useful groundwork for a
  future CBL/CBR-style data-logger expansion.
- **Vertical centering for fraction expressions**: fraction display currently
  top-aligns to the surrounding text line instead of centering on the
  fraction bar. Readable as-is, but a stacked numerator/denominator reads
  better vertically centered against neighboring glyphs. Text-renderer layout
  change, not yet scoped.
- **Auto power-off / standby after idle**: no inactivity timeout today: the
  calculator stays fully on and drawing power until manually switched off.
  Battery-life feature; needs an idle timer, a low-power/sleep path, and a
  wake trigger (key press). Not yet scoped.
- **Remember screen brightness / keypad backlight setting**: brightness and
  backlight level reset instead of persisting across power cycles. Needs a
  feasibility check first — depends on what settings-persistence path already
  exists (if any) and whether the PicoCalc's brightness/backlight controls
  are even readable back or only write-only. Not yet scoped.

## Graduated — now planned

(empty — everything graduated so far has shipped; see Completed / Closed)

## Completed / Closed

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
  Revisit if that storage gap is ever actually requested (tracked as an open
  watch-item in next-session.md).
- **TI-84 CALC-menu graph analysis** (value, zero, min/max, intersect, dy/dx,
  numeric fnInt) → Phase 4 sub-phase **4B**, shipped as **D29** (2026-07-20,
  see [decisions.md](./decisions.md) D29 and
  [phase4-spec.md](../phases/phase4-spec.md) §4). Numeric + interactive on the
  graph screen, layered on the existing compiled-eval machinery.
- **JuliaMono font swap (Session 13, D24.9)** → shipped **D31** as a general
  font selector: `-DPICOCALC_FONT=spleen|juliamono|iosevka|unifont|terminus`
  (default terminus). Licensing handled (OFL, vendored).
