# Test-drive observations — 2026-08-02 (Pico 2, reflashed to current HEAD)

Raw feedback from a Pico 2 hardware session, gathered by interview. The board
came into this session 9 builds behind (still Session 19); reflashed to
current HEAD (`dadc7cf`) at the start. Not yet investigated or fixed — logged
verbatim for a future session to triage. Interview was left partway through;
several checklist items (display-pipeline visual artifacts, MatAns repro,
APD dim/wake, general open-ended pass) were not reached this session.

## First boot after reflash

- **One-time data reset occurred as expected.** Variables (PCV1 format
  bump), and list/matrix stores (PCL2/PCM2 per-store format), came up reset
  on first boot under the new firmware — consistent with the documented
  "old files ignored" precedent for this format bump.

## General perf feel

- **Snappy, no complaints** on general UI responsiveness (menus, screen
  switches, typing) with current code on the Pico 2.

## Graph recompute timing (compute-bound stress probe)

Measured via the existing `graph recompute: <N> us` serial printf
(`graph_screen.cpp`), no new instrumentation added. Push-budget floor from
the earlier Pico 1 D10 A/B testing is ~146 ms; nothing below cleared it.

1. **7 functions (Y1-Y7, moderate trig expressions) + 8001-point scatter
   plot enabled**: `50843 us` (~50.8 ms). Comfortably under budget scatter
   included.
2. **Y1 only, 10 nested trig calls (`sin(cos(sin(cos(...x*2...)))`),
   scatter off**: `28116 us` (~28.1 ms).
3. **Y1 only, 20 nested trig calls, scatter off**: `33707 us` (~33.7 ms).
   Doubling nesting depth (10→20) added only ~20% time (+5.6 ms), not the
   ~2x that would be expected if cost scaled linearly with expression-tree
   depth — flagged as an observed anomaly, not diagnosed.

No compute-bound stall (i.e. nothing approaching or exceeding the 146 ms
push floor) was produced in these attempts on the Pico 2.

## Feature requests / UI friction

1. **No copy/paste in expression editors.** Duplicating one Y= expression
   into another slot requires retyping it in full on the physical keypad.
   Already added to `wishlist.md` (Active/unscheduled) same session.
