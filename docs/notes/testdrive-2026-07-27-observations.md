# Test-drive observations — 2026-07-27 (Pico 1, current 4D build)

Raw feedback from a usage survey session, gathered by interview. Not yet
investigated or fixed — logged verbatim for a future session to triage.

## Coverage (4D HW-PENDING batches)

- **Batch 2 — complex matrices (4D.25)**: works as expected. Editor
  a+bi/polar migration, REAL-mode "Non-real result" guard, `det`/`rref`/
  `ref`/`[A]^-1`/`rank`/`transpose`/`augment`/`[A]^2`, scalar `i*[B]`
  multiplication, mixed-matrix add, element read, complex scalar store,
  and power-cycle persistence all checked out. (One `ref([A])` result that
  looked wrong mid-session turned out to be a data-entry mistake on the
  interviewer's side — the matrix cell actually held `4+i`, not `4-i` —
  not a device bug.)
- **Batch 3 — sequence graphing (4D.6-8)**: works as expected overall,
  with two minor bugs found (see below). MODE cycling, basic ramp
  (u(n)=u(n-1)+1), Fibonacci table, cross-referenced sequences
  (v(n)=2*u(n-1)), window/ZoomFit, WEB-mode cobweb convergence, bad-form
  error cases (circular ref, lag-3), and F6 CALC no-op all passed.
- **Batch 4 — zoom + shading (4D.9-11)**: works as expected. ZDecimal,
  ZSquare, ZBox (crosshair/rubber-band/Alt-move/ESC-cancel), per-function
  shade toggle (none/above/below) with persistence, two-curve `H` band
  shading, and fnInt shaded-region color all checked out.
- **π tick labels (Batch 7 re-confirm)**: fixed — π/2, π, 3π/2, 2π render
  with the real glyph now.

## Bugs

1. **SEQ mode trace doesn't snap to exact values.** On the basic ramp
   test (u(n)=u(n-1)+1), the table showed exact integer values, but the
   F4 trace readout showed x/y as "something close to the integer values"
   rather than the exact integers — small float noise, not exact.
2. **Sequence function color swatch depends on recursive vs. explicit
   form, not the assigned plot color.** In the sequence editor list,
   a function that references u(n-1)/u(n-2) (recursive) always shows red;
   a function computed directly from n (no self-reference) always shows
   white — regardless of what plot color is actually assigned, and
   regardless of the fact that the graph itself plots in the correct
   color either way.

## Feature requests / UI friction

1. **Matrix results with many decimal places are hard to read.**
   Suggested: cap displayed digits.
2. **Constants with multi-character names are hard to read in their
   current display.** No specific fix proposed yet — flagged as "kiv"
   (keep in view) for design thought in a future session.

## Watch-items checked off

- **2-Var stats subscripts**: still not rendering (unchanged from
  2026-07-26 — documented gap, not new).
- **Scientific constant picker (`const` screen) text/number overlap**:
  still present (unchanged from 2026-07-26).
