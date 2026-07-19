# Feature wishlist — desired, not yet planned

Features wanted but not scheduled into a phase. When scoping a new phase,
pull relevant items in, then move them to **Graduated** below with the target
sub-phase so we keep the provenance. Keep this list short — it is not a
backlog of implementation tasks (those live in the phase specs / worklog),
only of features that don't yet have a home.

## Active (unscheduled)

- **Symbolic display (KIV, raised Session 10 eval)**: pi-multiple axis ticks,
  surd displays, fraction/pi-fraction answers. Phase 3/4 polish family; the
  4-sig-digit tick cap is the current stopgap.
- **Greek/typographic stats display (Session 13, D24.9)**: greek mu/sigma for
  mean/stddev, subscripts (Sx, sigma_x) in the stats results. Needs more baked
  glyphs (the `--map` mechanism generalizes) and a subscript story in the text
  renderer; judge against the plain-text results after more device use.
- **JuliaMono font swap (Session 13, D24.9)**: fuller math glyph coverage than
  Spleen. Needs baking from the upstream TTF/BDF sources (our pipeline reads
  BDF) and a licensing check (JuliaMono is SIL OFL 1.1 — compatible in
  principle, verify attribution requirements) before any work.
- **Scientific constants (Session 13, D24.9)**: c, h, N_A, ... Easy to fold
  into any session once the exposure surface is decided (catalog entries vs. a
  `const` command vs. a 2nd-key layer) — decide at the start of that session.
- **Unit conversions (Session 13, D24.9)**: as an app later, or
  scientific-calculator-style conversion pairs. Design effort needed; deferred.
- **Beyond 6 lists + SD list-data files (Session 13, D24.9)**: named lists
  and loading list data from files on the SD card; useful groundwork for a
  future CBL/CBR-style data-logger expansion.

## Graduated — now planned

- **Complex numbers** → Phase 4 sub-phase **4C** (see
  [phase4-spec.md](../phases/phase4-spec.md) §5). Scoped 2026-07-19. The whole
  stack is real-valued `double` today; 4C adds an `a+bi` value type through the
  engine, complex-aware functions, display format, and a MODE row entry.
  **Committed scope: lists and matrices hold complex values too** — that's why
  `Array`/`lists.dat` carry the dtype tag (D21/D22); 3A shipped with all element
  access routed through `get`/`set`, so the accessor internals are the only place
  the complex representation lands. (3B note: `math::stats` streams through
  `read_range` — real-valued stats stay correct whatever the storage dtype
  becomes.)
- **TI-84 CALC-menu graph analysis** (value, zero, min/max, intersect, dy/dx,
  numeric fnInt) → Phase 4 sub-phase **4B** (see
  [phase4-spec.md](../phases/phase4-spec.md) §4). Scoped 2026-07-19. Numeric +
  interactive on the graph screen, layered on the existing compiled-eval
  machinery.
