# Public docs — four facets

**Date**: 2026-08-10
**Status**: Plan. Extends [docs-site-plan.md](docs-site-plan.md) and the
`docs-site/` tree seeded on the `docs/site` branch. Nothing here is built yet.

## What changes

`docs-site-plan.md` split the audience two ways — *user* (the new public site)
and *developer* (the existing `docs/` tree, deliberately **not** republished).
That split was right for a private repo whose only reader was the person who
wrote it. Going public splits it further, because two audiences that were
previously the same person are now not:

| Facet | Reader | Question they arrived with |
|-------|--------|----------------------------|
| **1. User guide** | Picked up a flashed PicoCalc | "How do I run a 2-sample t-test?" |
| **2. Developer guide** | Wants to build, fix or extend the firmware | "Where does the graph plotter live, and what am I allowed to include from it?" |
| **3. Study guide** | Curious about how a calculator is built | "How does a shunting-yard compiler actually work, and how did you fit one in 4 KB of stack?" |
| **4. App developer guide** | Writing a MicroPython app for the device | "What does the `calc` module expose, and how does my app get a screen?" |

Facet 1 exists in draft. Facet 2 exists as repo files but not as a doc.
Facet 3 does not exist and is the most valuable new writing. Facet 4 is gated
on Phase 6B shipping.

## Why the study guide is worth writing

It is the facet with the least overlap with anything else, and the project is
unusually well-positioned to write it: `decisions.md` already contains the
*reasoning* — rejected alternatives, measured numbers, the constraint that
forced each choice — which is exactly the material a tutorial normally lacks
and has to invent.

Most compiler and numerics tutorials are written against unbounded memory. This
one would be written against 264 KB of SRAM, a 4 KB stack shared with a
display service on the other core, and no FPU on one of the two targets. The
constraints are the interesting part, and they are documented as they were
discovered rather than reconstructed afterwards.

It is also the honest home for the project's failures. D48's hard fault, D53's
intermittent PSRAM read, D46's two evaluators silently disagreeing — these are
better teaching material than any of the code that worked first time, and
there is nowhere in a user or contributor doc where they belong.

## Structure

The existing pipeline needs no change to accommodate this: `SUMMARY.md` is the
single nav source, the wiki flattener already handles arbitrary directories,
and `validate_md.py` already covers the tree. Adding a facet is adding a
directory and some `SUMMARY.md` lines.

```
docs-site/
├── index.md                    # landing page — routes to the four facets
├── getting-started/            # shared: install, flash, first expression
│   ├── build-and-flash.md
│   └── first-steps.md
│
├── guide/                      # FACET 1 — user guide (15 chapters, exists)
├── reference/                  # FACET 1 — generated from firmware source
│
├── developing/                 # FACET 2 — developer guide (new, thin)
│   ├── index.md                #   what the codebase is, and the layer rules
│   ├── environment.md          #   toolchain, both boards, CI
│   ├── testing.md              #   host suite, serial injection, the eval probe
│   ├── working-on-it.md        #   conventions, branches, the decision log habit
│   └── hardware-notes.md       #   what only shows up on a real board
│
├── internals/                  # FACET 3 — study guide (new, the big one)
│   ├── index.md
│   ├── 01-tokenizer.md
│   ├── 02-shunting-yard.md
│   ├── 03-rpn-stack-machine.md
│   ├── 04-tagged-values.md
│   ├── 05-natural-math-rendering.md
│   ├── 06-graphing-pipeline.md
│   ├── 07-numeric-algorithms.md
│   ├── 08-linear-algebra.md
│   ├── 09-cas-passes.md
│   ├── 10-symbolic-integration.md
│   ├── 11-memory-on-a-microcontroller.md
│   ├── 12-two-cores.md
│   └── 13-what-went-wrong.md
│
├── apps/                       # FACET 4 — app developer guide (stub until 6B)
└── about/
```

`developing/` is deliberately **thin**. It is an entry point that orients a
newcomer and then sends them into the repo — `CONTRIBUTING.md`,
`docs/architecture.md`, `docs/notes/decisions.md`. Republishing the phase specs
and the decision log on the site was rejected in the original plan and stays
rejected: they are written dense and D-numbered for a future session, they
version with the code, and a copy on a site is a copy that goes stale.

## Facet 3 chapters, and what each is actually about

Each chapter follows the same shape: **the general algorithm**, taught properly
and independently of this codebase; then **how it is implemented here**, with
the constraint that shaped it; then **what it cost** — a number, from the
measurements or the size report.

| Chapter | Algorithm | The constraint that shaped it |
|---------|-----------|-------------------------------|
| 01 Tokenizer | Lexing an infix expression, implicit multiplication, the `-` ambiguity | No `std::string`; fixed buffers |
| 02 Shunting yard | Dijkstra's algorithm, precedence, associativity, unary operators, function calls | Right-associative `^` and the unary-minus binding that took two independent bugs to get right (D50, D51) |
| 03 RPN stack machine | Compile once, evaluate many; a flat program versus a tree walk | Why graphing made this the right shape: 7 slots $\times$ ~300 samples per redraw |
| 04 Tagged values | One 24-byte `Value` over real, complex, matrix and list; dispatch without RTTI or virtuals | Replacing four evaluators that silently disagreed (D46) |
| 05 Natural math rendering | Layout trees, box metrics, baselines — stacked fractions and raised exponents | A $320\times320$ display and a font with 135 glyph slots |
| 06 Graphing pipeline | Viewport transforms, sampling, discontinuity detection, trace | Redraw budget; why the plotter never allocates |
| 07 Numeric algorithms | Root finding, numeric integration, the distribution functions | Softfloat on the RP2040 — the same code, ~10$\times$ slower |
| 08 Linear algebra | LU, determinant, inverse, row echelon, the eigenvalue algorithm | 2 KB slabs, and when an array moves to PSRAM |
| 09 CAS passes | Simplify, expand, factor, differentiate as tree rewrites over a pool | Pool exhaustion must be an error, not a plausible wrong answer (D45) |
| 10 Symbolic integration | Pattern matching, and where the Risch algorithm begins | Why it stops where it does — differential algebra, not the hardware ([reference](../references/risch-algorithm.md)) |
| 11 Memory | Static allocation, arenas, pools, PSRAM over PIO SPI, DMA | The 4 KB stack; `kSlabBytes`; the RAM budget across two boards |
| 12 Two cores | A display service on core 1, and what that does to everyone else's stack | D47: where core 0 meets core 1 |
| 13 What went wrong | Four real bugs, start to finish | A hard fault only one board showed; a wrong answer only one evaluator gave; a PSRAM read that failed 2% of the time in one code path and 0% in another |

Chapter 13 is the one to write first if only one gets written. It is the
chapter that could not have been written by reading the code.

## Facet 4 — the app developer guide

Stub now, real when Phase 6B ships. Structure to reserve:

- What an app is — the lifecycle and screen contract from 6A
- Getting a script onto the device (SD card) and running it
- The `calc` module: evaluating expressions, reading and writing variables,
  lists and matrices, from Python
- Drawing: what an app may do to the framebuffer
- Memory: the 48 KB heap, and what happens when a script exceeds it
- Worked examples

One thing to flag before writing any of it: 6B's `calc` bindings were specced
against the evaluator Phase 5.2 replaced, so the surface this facet documents
needs re-verifying first. Documenting it from the spec would document something
that no longer exists.

## Sequencing

Nothing here blocks firmware work, and the facets are independent.

1. **Now, with the public flip** — `developing/` (facet 2). It is thin, it is
   mostly reorganising `CONTRIBUTING.md` into site pages, and a public repo
   with no contributor entry point is the gap that matters soonest.
2. **Next** — `internals/13-what-went-wrong.md`, then chapters 02–04. These
   cover the evaluator, which is both the most recently reworked part of the
   codebase and the part with the best-documented reasoning behind it.
3. **Alongside** — finish facet 1's guide prose, which is still stubs.
4. **When 6B ships** — facet 4.

The offline generator should grow per-facet bundles rather than one document:
a user reading a guidebook and someone reading a study guide want different
PDFs, and `gen-offline.sh` already walks `SUMMARY.md`, so the split is a
grouping argument rather than new machinery.

## What not to do

- **Don't republish `docs/`.** Same reasoning as the original plan. The
  decision log, phase specs and worklog stay in the repo, versioned with the
  code. The study guide *draws on* them; it does not mirror them.
- **Don't let facet 3 become API documentation.** If a chapter cannot be read
  by someone who will never open this codebase, it belongs in facet 2.
- **Don't gate the site on screenshots.** Still true, still text-first.
- **Don't write facet 4 from the 6B spec** — see above.
