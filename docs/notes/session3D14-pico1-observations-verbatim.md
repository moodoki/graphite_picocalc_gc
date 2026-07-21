# Task 3D.14 — Pico 1 combined pass, on-device observations (2026-07-22)

Board: **Pico 1 (RP2040)**, swapped in and freshly reflashed with
`build/pico/picocalc_graphcalc.uf2` built from HEAD `f9dbfb6` (Session 19
font/glyph build). This board had been stuck on Session 7 firmware since
2026-07-18 — this closes out the long-deferred task 3D.14 / decision D18
combined pass. **Scope this session: Phase 2 + Phase 3 only** (developer
decision) — Phase 4A-4C (matrices, CALC menu, complex numbers) and the
Session 19 font/glyph work were explicitly left for a future Pico-1
session and not exercised here.

Post-flash serial capture (before this interview) already confirmed a
healthy boot: PSRAM OK, battery telemetry sane, graph recompute running,
no crashes/hangs/repeated FAIL across three capture windows.

Logged for a future session — **no fixes applied yet, findings not
investigated**.

## Strip-renderer / Pico-1-specific checks (never run on this board before)

- Split-pane clipping (graph|table split, no bleed across divider): works as expected.
- Graph screen status bar + top-of-plot clipping: works as expected.
- General strip-render idempotency (visual glitches/tearing/flicker anywhere): nothing noticed.
- First boot one-time migration/reset (graphstate PCG1->PCG5, lists/matrices formats, first time on this board): clean, no issue.

## Session 8+9 fix list re-verification (fixed blind on Pico 2, first Pico-1 check)

- Screen-stack leak (repeated F4 toggling graph<->editor<->table getting stuck): works as expected.
- Held-key table scroll overrun: stops promptly at key release.
- Home status bar overdraw / battery staleness: works as expected.
- Charging flag (plugged in, shows "(charging)"): shows correctly.
- DEG/RAD, display mode (a+bi/polar), FIX digits persistence across power cycle: persists correctly.
- Wording (ESC/F4 "back" phrasing) + table softkey divided-cell style: works as expected.
- Square ZStandard window: square as expected.
- Typed commands (`cls`, `clrhist`, `help`, `diag`, `files`): all work.
- F-key remap (D20) matches KEYS help tab: matches.
- Case-sensitive input (`2->A` errors, lowercase var echo): works as expected.
- DEL/SPACE semantics (DEL clears, SPACE toggles slot enable): works as expected.
- Polar cardioid DEGREE-mode final-arc gap near 360 deg: closes properly now.
- Split-screen trace activation from *within* split (previously a known UX gap on Pico 2): works now.

## Phase 2 acceptance (2.24 checklist, first time on this board)

- Function mode (Y= editor, graphing, trace): works as expected.
- Parametric (circle + Lissajous with trace, editor auto-focus/pair behavior): works as expected.
- Polar (cardioid + rose, both RADIAN and DEGREE): works as expected.
- Tables (AUTO infinite scroll, ASK add/delete, LEFT/RIGHT column scroll, setup Start/Step): works as expected.
- SD/PSRAM diag health + hot-eject/reinsert recovery (D26 retry-forever): works as expected.

## Phase 2/3 findings

1. **Help screen (FUNC tab): factorial with `!` results in a syntax error.**
   (Reported as-is; not investigated this session.)

## Phase 3 acceptance (Sessions 11/12/15, never run on Pico 1 hardware before)

- List editor (3A) — create/edit, list operations: works as expected, **but feels a little sluggish** on this board.
- Descriptive stats (3B) — 1-Var/2-Var stats, regression models: works as expected.
- Probability distributions (3C): works as expected.
- Inference (3D) — hypothesis test forms (z/t/proportion/chi-square/ANOVA), confidence intervals: works as expected.
- StatPlot layer (scatter, xy-line, histogram, box plot, normal probability plot, ZoomStat): works as expected.
  - Verified with contrast pairs to confirm data-dependent rendering, not hardcoded:
    box plot with a constructed outlier (`{2,4,5,6,7,8,9,10,11,30}`) vs. one
    without (`{5,7,8,9,10,11,12,13,15}`); normal probability plot with a
    roughly-normal set (`{68,70,71,72,72,73,73,74,74,75,75,76,77,78,80}`) vs.
    a right-skewed set (`{1,2,2,3,3,3,4,4,5,6,8,12,20}`).
  - **5000-point scatter plot is slow to render.**

## Open items raised, not covered above

None — developer confirmed nothing further to add this session.
