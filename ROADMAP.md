# Roadmap and status

Current release: **[v0.4.0](https://github.com/moodoki/graphite_picocalc_gc/releases/tag/v0.4.0)**
— prebuilt UF2s for both boards. Both targets build clean; the host suite is
16 files / 2,599 checks, green.

## Phases

Work is organized into numbered phases. **Lettered** sub-phases (4A, 4D, 6B)
are planned parts of a phase's own goals; **dotted** sub-phases (5.1, 5.2) are
significant work that turned up outside them. See
[AGENTS.md](AGENTS.md) for the convention.

| Phase | Status | Notes |
|-------|--------|-------|
| 0: Prep | Complete | Environment, repo, vendored drivers |
| 1: HAL + calculator + basic graphing | **Complete** | HW-verified on both boards ([retro](docs/notes/phase1-retro.md)) |
| 2: Graph modes + table + split + help | **Complete** | Parametric/polar, value table, split-screen, built-in help ([retro](docs/notes/phase2-retro.md)) |
| 3: Statistics | **Complete** | Lists, regression, distributions, inference, stat plots ([retro](docs/notes/phase3-retro.md)) |
| 4A–4C: Matrices + graph analysis + complex numbers | **Complete** | HW-verified on both boards (D28/D29/D30) |
| 4D: GC completeness — **the pre-release milestone** | **Complete** | Nine batches, 2026-07-26/27. Feature-complete as a graphing calculator, independent of CAS. Tagged **v0.1.0** |
| 5: CAS (symbolic math) | **Complete** | Engine, UI integration, exact-form display, Stage 5 stack hardening. Tagged **v0.2.0** |
| 5.1: Serial line injection | **Complete** | Host-driven on-device test automation. Tagged **v0.3.0** |
| 5.2: Unified evaluator | **Complete** | One tagged-value evaluator replacing three. Tagged **v0.4.0** |
| 6: Non-calculator functions | **6A + 6C.1 done, HW-verified; 6B next** | App launcher, shared text editor, file browser, Notepad. MicroPython (6B) still to come. [Spec](docs/phases/phase6-spec.md) |

Everything marked Complete is hardware-verified on both the Pico 1 H and the
Pico 2 H.

## Phase specs

Each phase has a design contract written before the work, kept afterwards as
the record of what was agreed:

- [phase0-prep.md](docs/phases/phase0-prep.md) — environment and repo bootstrap
- [phase1-spec.md](docs/phases/phase1-spec.md) / [phase1-plan.md](docs/phases/phase1-plan.md) — HAL, calculator, basic graphing
- [phase2-spec.md](docs/phases/phase2-spec.md) — graph modes, table, split-screen, help
- [phase3-spec.md](docs/phases/phase3-spec.md) — statistics
- [phase4-spec.md](docs/phases/phase4-spec.md) — matrices, graph analysis, complex numbers, GC completeness
- [phase5-spec.md](docs/phases/phase5-spec.md) — CAS
- [phase5.1-spec.md](docs/phases/phase5.1-spec.md) — serial line injection
- [phase5.2-spec.md](docs/phases/phase5.2-spec.md) — unified evaluator
- [phase6-spec.md](docs/phases/phase6-spec.md) — app framework and MicroPython

## What Phase 5.2 changed, and why it mattered

5.2 replaced the three separate home-screen mini-evaluators (real, complex,
list) plus a fourth for matrices with **one tagged-value evaluator**: a
shunting-yard compiler emitting a flat RPN program, run by a stack machine.

It was motivated by two independent findings rather than by tidiness:

1. The real and complex evaluators **silently disagreed** on DEGREE-mode trig
   (D46), so the answer depended on which one happened to handle your input.
2. Four parsers each needed a separately-discovered stack budget on a 4 KB
   stack — three of them found by a crash (D48).

As built it deleted 3,903 lines and three of those four depth caps, returned
~6.9 KB of static RAM, and turned up a *second* live disagreement between the
two shipped evaluators along the way (`(-2)^2`, D50 → fixed in v0.3.2). Every
behaviour that changed is recorded in
[unified-evaluator-changes.md](docs/notes/unified-evaluator-changes.md), and
the before/after measurements — including the regressions — in
[docs/notes/measurements/phase5.2/](docs/notes/measurements/phase5.2/).

## Phase 6: non-calculator functions

**6A (app framework) and 6C.1 (Notepad) are done and hardware-verified.**
The calculator now has an app launcher reached from the home screen by an
`F6` softkey or an `apps` command, a shared line-numbered text editor, a
file browser with directory navigation and file management, and Notepad as
the first app on the framework.

- **6B — MicroPython** is next: the interpreter as an app on that framework,
  exposing a `calc` module so scripts can reach the math engine.

What changed under 6A that matters for scoping 6B:

- **The SRAM tooling was wrong, and fixing it changed the picture.**
  `size-report.sh` computed headroom from Berkeley `size`'s `bss + data`
  columns, which bin this target's `.data` under *text* and report `data 0` —
  omitting ~44 KB of live SRAM from every reading it had ever produced. Real
  free SRAM was **14.3 KB**, not the ~58 KB reported, so the MicroPython heap
  never fit on either board (D69).
- **54 KB was then recovered** (D70): one-shot persistence buffers folded into
  a shared region, the ArrayStore slab pool cut behind a new PSRAM fallback,
  and the render strip height halved. Pico 1 free SRAM is now **61 KB** and
  Pico 2 **126 KB**, against the 48 KB and 104 KB the two heap budgets need.
- **6B's `calc` bindings were re-verified** against the unified evaluator
  (D60, closing issue #27) — no dead entry points, and a concrete
  `calc.eval()` shape recorded for 6B.3 to build against.

## Beyond the plan

- [wishlist.md](docs/notes/wishlist.md) — wanted but unplanned, with what would
  have to be true to schedule each item
- [ti-parity.md](docs/notes/ti-parity.md) — feature stocktake against the
  TI-83/84+ and the TI-Nspire CX II CAS
- [design-departures-matrix-complex.md](docs/notes/design-departures-matrix-complex.md)
  — first-class matrix/vector/complex ideas that were considered and not built
- [docs-site-plan.md](docs/notes/docs-site-plan.md) — the public documentation
  plan
