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
