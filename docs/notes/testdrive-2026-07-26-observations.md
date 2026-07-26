# Test-drive observations — 2026-07-26 (Pico 1, current 4D build)

Raw feedback from a usage survey session, gathered by interview. Not yet
investigated or fixed — logged verbatim for a future session to triage.

## Coverage (4D HW-PENDING batches)

- **Batch 9 — device polish (APD sleep/wake, settings screen)**: works as expected.
- **Batch 8 — matrix eigenvectors**: works as expected.
- **Batch 7 — display & formatting**: **found a problem** (see below).
- **Batch 6 — named lists**: works as expected.
- **Batch 5 — data & catalog glue**: works as expected.
- **Batch 4 — zoom + shading**: not tested this session.
- **Batch 3 — sequence graphing**: not tested this session.
- **Batch 2 — complex matrices**: not tested this session.

## Bugs

1. **2-Var stats subscripts not rendering.** Batch 7 (4D.1-5) shipped true
   subscript glyphs (Sₓ/σₓ) for 1-Var stats results. Observed: 1-Var stats
   subscripts render correctly; **2-Var stats does not use the subscript
   glyphs**. (Note: the worklog's Batch 7 row already flags that no
   subscript-y glyph exists for 2-Var's Sx/Sy pairs — this observation
   confirms that gap is visible in practice, not just a documented
   limitation.)

## Feature requests / UI friction

1. **Scientific constant picker (`const` screen): numbers overlap
   description text.** The literal numerical value shown for each constant
   collides with its description on the selection screen. Suggested
   options from the developer: limit displayed precision, or drop the
   literal numerical display from the selection screen entirely.

## Watch-items checked off

- **Font/glyph watch-items (Session 19)** — inline-only `√` (no vinculum)
  and the shared Unifont-derived `i`/`⇒` glyphs against Terminus's own
  glyph shapes: **looks fine / acceptable**, no issue found.
