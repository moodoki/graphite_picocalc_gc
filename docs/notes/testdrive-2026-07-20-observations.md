# Testdrive feedback — 2026-07-20

Board tested: **Pico 2**. Firmware was actually the **Session 18 (4C) build**
(complex numbers) — note that `next-session.md` as of this writing still
records the Pico 2 as being on the Session 16 (4A) build; the 4C flash
happened but wasn't recorded there. Pico 1 not tested this session.
Session depth: moderate. No crashes, freezes, hangs, or unexpected reboots.

Raw feedback below — **not yet investigated or fixed**, logged verbatim for a
future session.

## Checklist responses

- **4A matrices + solver** ([A]-[J] matrix vars, bracket typing/editor,
  numeric solver): works as expected.
- **4C complex display** (MODE screen "Number" row cycling REAL/RECTANGULAR/
  POLAR, a+bi form, polar form): works as expected, *except* see the polar
  angle glyph note below.
- **4C REAL-mode domain errors** (e.g. `sqrt(-4)` → "Non-real result"):
  clear as-is.
- **4B CALC menu** (min/max/intersect/fnInt, including the min/max "Guess?"
  step): works as expected.
- **Storage health** (SD/PSRAM retry, status-bar indicators, hot-plug): no
  issues this session.
- **Stats** (plain-text results, "Computing..." indicator on large
  datasets): fine as-is.
- **F3 slot** (MODE vs ZOOM on the graph screen): no opinion / didn't
  notice.

## UI notes

1. ASCII `<` as the polar angle separator (e.g. "2<60") is confusing. We
   should take this chance to swap the font — test-drive **JuliaMono** as
   the new font. Include math glyphs so pi, theta, sigma, Sigma, Chi, mu,
   etc. display properly (real glyphs, not ASCII stand-ins). Also include
   an alternative `i` glyph for the complex-number `i` when rendering in
   pretty print.

## Lists UX — wants a change

Truncation is problematic with float results. Long results should round to
shorter prints (say 5 chars), and allow left/right arrows to scroll and
view the full output. KIV a separate screen to display list results with
full precision.
