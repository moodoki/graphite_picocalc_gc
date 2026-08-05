# Testdrive observations — 2026-08-05

**Board/build:** Pico 1, current tip (`5ef025f`, Phase 5 Stage 5). Pico 2 not
tested this session.

Raw feedback from hands-on testing — not yet investigated or fixed.

## Bugs

1. **Y= function editor freezes the calculator, every time, on Pico 1.**
   Opening the Y= editor renders only the first few pixel rows of the header
   before the calculator locks up completely — keys are totally
   unresponsive, a physical power cycle is required to recover. Reproducible
   every time the screen is opened. No serial capture was taken. Pico 2 has
   not been tested yet, so it's unknown whether this is board-specific.
   Testing stopped here — the CAS on-device checklist (exact-form display,
   DEGREE-mode folding, Alt+Enter, reboot-reload) had not been reached yet
   when this was hit. Everything else tested on Pico 1 this session (screens
   / features other than Y=) was reported fine, no issues.

   Not the same issue as the Session 14/15 "Y=-editor truncation" bug
   (`session14-observations-verbatim.md`, D26) — that was a display-overlap
   readability fix (long expressions clipped with `...` before the enable
   checkbox), closed and confirmed on both boards 2026-07-22. This is a
   fresh, distinct failure mode: a hard lockup, not a rendering glitch.

## UI/UX friction / feature requests

1. **ZTrig in DEGREE mode uses the wrong graphing window.** When Number/Angle
   mode is DEG, ZTrig currently sets graphing limits to ±2π-based values
   (radian-appropriate). It should instead set limits that make sense for
   DEG mode.
