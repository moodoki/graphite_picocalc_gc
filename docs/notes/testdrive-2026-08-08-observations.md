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

1. **`5!` and `abs(3+4i)` in a+bi mode: "shows white values rather than
   120/5".** Verbatim. Two readings, and the session ended before it could be
   reproduced, so **do not assume which**:
   - the results are *wrong* (something other than `120` and `5` is on
     screen, rendered in white), or
   - the results are *right* but the tester expected them typeset/amber
     rather than plain white.

   Both are plausible and they need completely different fixes, so
   **reproduce before touching anything**. Expected values are confirmed
   correct on the host: `5!` -> `120`, `abs(3+4i)` -> `5` (both real-valued,
   so plain white is what the current display rules would produce — which
   makes the second reading the more likely one).

   Worth checking specifically, because this build changed both paths:
   `5!` goes preprocess_factorial -> `fac(5)` -> not a numeric literal ->
   `parse_scalar_span`'s eval_field fallback; `abs(...)` is a `kFns` entry
   that never touches that path. If only one of the two is wrong, that split
   localises it immediately.

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
