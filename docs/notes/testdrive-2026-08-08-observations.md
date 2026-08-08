# Testdrive observations — 2026-08-08

**Board/build:** Pico 1, `3153868` (D47 stack work: parser depth caps, layout
pool rework, numeric-literal fast path, stack guards + fault reporter).
Pico 2 not flashed this session.

Raw feedback from hands-on testing — **not yet investigated or fixed.**

Context: this pass ran groups 1-4 of the post-D47 test plan (typeset display
regression, the three untested slot editors, list expressions, a+bi mode and
numeric literals). Everything else in those four groups was reported correct.
Groups 5-7 (guards-are-live sweep, the Phase 5 CAS checklist, Pico 2) were
not reached.

## Bugs

1. ~~**`5!` and `abs(3+4i)` in a+bi mode: "shows white values rather than
   120/5".**~~ **RESOLVED — not a bug (tester, 2026-08-08, after the session).**
   The original verbatim note was ambiguous between "the values are wrong" and
   "the values are right but not typeset". It is the second, and the
   expectation behind it was mistaken: the tester had read the two entries as
   one expression (`5! / abs(3+4i)`) and expected an improper-fraction exact
   form. They were two separate entries. **The values on screen were correct
   (`120` and `5`), and plain white is the correct rendering** — both results
   are real integers, not exact forms that need amber. No firmware change
   needed; nothing to reproduce.

## UI/UX friction / feature requests

1. **`seq()` requires all five arguments; `range()` does not.**
   `seq(x,x,1,5)` is rejected — `eval_seq` demands exactly five
   (`expr, var, lo, hi, step`, `list_expr.cpp`), while `range(lo,hi[,step])`
   accepts two or three and defaults the step. Found because the test plan
   handed over a four-argument `seq` call; **that was an error in the plan,
   not a defect** — the firmware behaved as written.

   Still worth considering: defaulting `step` to 1 would match `range`'s own
   shape, and TI-84's `seq(` also allows the step to be omitted. Small change
   in `eval_seq` (accept 4 or 5, default the fifth), plus a host test.

2. **The graph screen has no pan.** Raised during the group-5 sweep; zoom in
   and out both behave correctly. **Confirmed against source**: in
   `graph_screen.cpp:1307` all four arrow keys are handled *only* when
   `trace_.active` — with trace off they are consumed (`return true`) and do
   nothing. Zoom is bound separately on `kMinus`/`kEquals`/`kPlus`.

   The keys are therefore free when not tracing, and TI-84 pans the window
   with the arrows in exactly that state. Implementation shape: add
   `pan(dx_frac, dy_frac)` to `graph_model.cpp` mirroring `zoom_in`/`zoom_out`
   (shift `x_min`/`x_max`/`y_min`/`y_max` by a fraction of the current span,
   then `save_window()`), and give the four `Key::k{Left,Right,Up,Down}` cases
   an `else` branch that pans and sets `dirty_ = true`. Plus a host test on
   the new model function.
