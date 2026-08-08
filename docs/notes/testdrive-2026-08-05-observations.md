# Testdrive observations — 2026-08-05

**Board/build:** Pico 1, current tip (`5ef025f`, Phase 5 Stage 5). Pico 2 not
tested this session.

Raw feedback from hands-on testing — not yet investigated or fixed.

**Status 2026-08-08: BOTH items FIXED and HW-verified on the Pico 1.**
Bug 1 was a core-0 stack
overrun into core 1's stack — `SlotEditorScreen::render()` called
`Engine::compile()`, a 2,232 B frame, once per row per 16-px strip while core
1 was mid-DMA. The investigation found two more instances of the same class:
the home-screen list path (`eval_list_into`, 2,248 B and recursive), and
tinyexpr's parser having no depth cap at all — the latter caught by the newly
added stack guard + fault reporter on the first flash, which traced it to a
prologue push in `factor`. **Y1 on this board was still holding one of the
2026-08-02 "up to 20 nested trig calls" stress probes**, which is what made
the Y= editor fail every single time. It now draws red (rejected by the new
`kMaxParseDepth = 8`) and the editor and graph both work. ZTrig in DEGREE
confirmed working as intended on the same pass. See `decisions.md` **D47**.

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
   mode is DEG, ZTrig currently sets graphing limits to $\pm 2\pi$-based values
   (radian-appropriate). It should instead set limits that make sense for
   DEG mode.
