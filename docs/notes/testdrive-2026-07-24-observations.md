# Testdrive observations — 2026-07-24

Board/build tested: **both boards** — Pico 1 (current HEAD, D35 perf fixes,
bss ~201.9 KB) and Pico 2 (Session 19 build, no D35 fixes yet).

Raw feedback from a soak/discussion session aimed at closing out open Phase 4
threads. Not yet investigated or fixed — decisions/dispositions are the
user's own calls made live during the interview, not analysis added after
the fact.

## Open design-thread dispositions

- **MatAns (home-screen token for last matrix result)**: fold into 4D, "but
  exact implementation depends on how we decide to handle matrices and
  complex numbers, which we have planned to revisit."
- **fnInt shading following curve color**: fold into 4D.
- **4C "Non-real result" phrasing** (REAL-mode domain errors): clear, leave
  it as-is.
- **Font/glyph** (√ inline-only/no vinculum; shared Unifont-derived i/∠
  glyphs vs. Terminus): both look fine.
- **F3 MODE vs ZOOM slot** (D20 KIV): didn't test this session.
- **D16 trace-sync option b** (table-step sync in split view): current
  behavior is fine.
- **Session 13 caps** (4 lift operands/expression, 64-element brace
  literals): didn't test this session — no pinch reported.

## New feedback (not previously tracked)

- **fnInt / trace numeric entry**: no way to type integration limits
  directly — only settable by moving the trace cursor. Requested this be
  extended to trace functionality generally (typed values, not just
  cursor-driven selection).
- **Vector/matrix operations**: wants lists usable as vectors (or narrow
  matrices), with vector ops like cross and dot products. Wants different
  norms (vector and matrix norms). Asked whether an eigenvector function
  exists. Overall take: having matrices and lists walled off from each
  other, like on the TI, feels clunky.
