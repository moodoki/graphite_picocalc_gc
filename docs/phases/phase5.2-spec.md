# Phase 5.2 Spec: Unified Evaluator (idea F)

**Prerequisite phases**: Phase 4A–4C (the three evaluators being unified, and
the `Complex` type), Phase 5 (CAS — sequenced after deliberately, so that any
fourth symbolic evaluator is known before unification), and
[Phase 5.1](phase5.1-spec.md) (serial injection — see §6, it is the main
practical mitigation for this phase's regression risk).

**Scope**: Replace `math::matexpr`, `math::complexexpr` and `math::listexpr`
with **one** recursive-descent evaluator over a single tagged `Value`
(`{kind: real | complex | matrix | list, …}`), built on an **explicit evaluation
stack** rather than the C++ call stack. A matrix of complex numbers, a complex
scalar times a matrix, a list containing complex elements, a dot product between
two list-vectors — all stop being special cases and become the same generic
binary-op dispatch over a wider tag enum.

**Status**: Specced, not started. Idea F has been a committed follow-on since
2026-07-24 (D37); **judged worth the effort 2026-08-08 (D48)** and given a phase
number at the same time. This is the highest-risk item on the project's list, by
its own long-standing description.

**Why "5.2" and not "5F" or "Phase 7"** — per the naming convention in
`AGENTS.md`: lettered sub-phases are planned work a phase's completion depends
on, dotted ones are significant units that *turned up* and sit outside the
parent phase's goals. Idea F emerged from Phase 4's design-departures review and
was reinforced by Phase 5's bug work (D46, D48), but unifying the evaluators was
never part of what Phase 5 set out to deliver — Phase 5 closed without it. It is
substantial (§4 provisionally sizes it at ~73 hrs, comparable to Phase 6B), so
the dotted form is a judgement call: it is a consequence of earlier phases
rather than a new goal of its own, and it belongs before Phase 6 rather than
alongside it. **Revisit if 5.2's scope grows** — if the sizing pass in 5.2.1
pushes it materially past its current estimate, promoting it to a full phase
number would be the honest move.

**Reference reading**:
[design-departures-matrix-complex.md](../notes/design-departures-matrix-complex.md)
**§F** — the original idea and its 2026-08-08 update, including the design
constraint below. [decisions.md](../notes/decisions.md) **D37/D40** (F committed
as a real follow-on), **D46** (the correctness argument), **D48** (the structural
argument and the explicit-stack constraint). `phase4-spec.md` **§5.2** carries
the performance guardrail that bounds this phase's scope — read it before
widening anything.

---

## 1. Why this phase exists

Two **independent** arguments converge, which is what moved F from "planned,
timing tied to a trigger" to worth scheduling:

**Correctness (D46).** The real and complex evaluators silently disagreed about
DEGREE-mode trig since Session 18 — `c_sin` never applied `rad()`, so `sin(30)`
returned `0.5` from the real path and `-0.9880316241` from the complex one. That
is precisely the class of bug unification removes: one evaluator cannot disagree
with itself.

**Structural (D48).** Four parsers have needed four separately-discovered stack
budgets, and **three of the four were found by something crashing**:

| parser | cap | how it was found |
|---|---|---|
| CAS | `kMaxDepth = 12` / simplifier 8 | D45 audit |
| tinyexpr | `kMaxParseDepth = 7` | D47 — crash record, `factor+0xa` |
| `complexexpr` | 7 / nested 4 | D47 |
| `matexpr` | 3 | **D48 — reproducible hard fault** |

`matexpr`'s cap landed with **84 bytes of margin** on the Pico 1 and required a
follow-up leaf fix before it held on the Pico 2 at all. That is containment, not
headroom, and it is the fourth time the same shape of bug has been paid for.

## 2. Scope boundary — four parsers become two, not one

**`evaluate_real()` (tinyexpr) is never touched.** `phase4-spec.md` §5.2
established the guardrail when 4C landed: *"making the default numeric path
complex would double arithmetic cost on the hot graphing loop… keep two
evaluation entry points"*. That generalizes directly — a unified tagged-`Value`
evaluator must stay **strictly home-screen-only**, exactly as
`evaluate_complex()` is today. Graphing, tables and stats keep the narrow, fast
real path.

So the end state is two evaluators, not one:

| path | before | after |
|---|---|---|
| graphing / tables / stats | tinyexpr (`evaluate_real`) | **unchanged** |
| home screen | `matexpr` + `complexexpr` + `listexpr` | one unified evaluator |

## 3. The explicit-stack constraint (D48)

**Build the evaluator on an explicit operand/operator stack, not the call
stack.** This is a decision, not an option, and it is the difference between
this phase solving the problem and inheriting a fifth frame budget.

Rationale: every cap in §1 exists because depth lives in call frames against
core 0's **4 KB** (`__StackBottom`–`__StackTop`, identical on both boards — the
RP2350's extra SRAM does not reach the stack, which sits in a 4 KB scratch bank
on each chip). Move depth into an array and it becomes a sized, inspectable
resource instead of a cliff.

Secondary benefit: such an array is accessed **sequentially**, which makes it
PSRAM-eligible if it ever needs to be large. A call stack is not — `psram.hpp`
is PIO-driven SPI, `alloc()` returns offsets rather than pointers, and it is not
memory mapped on either board (the RP2350's native QMI mapping is unused). At
~200 µs/KB it would be unusable for call frames regardless. Note this is a
*future option*, not a v1 requirement: sizing the stack in bss is expected to
suffice, and PSRAM adds latency that should not be paid unless needed.

## 4. Task breakdown

Deliberately coarse — the design work in §5 and §6 must land before these can be
sized honestly. Estimates are placeholders pending 5.2.1.

| # | Task | Est. hrs | Acceptance |
|---|------|----------|------------|
| 5.2.1 | Tagged `Value` design + sizing pass against Pico 1 headroom (§5) | 4 | A written per-node size budget; go/no-go on the union width |
| 5.2.2 | Explicit evaluation stack: structure, depth bound, overflow behaviour | 6 | Over-deep input is a clean error at a *stated* depth, never a fault |
| 5.2.3 | Core evaluator — tokenizer, precedence, generic binary-op dispatch | 16 | Real-only expressions match `evaluate_real` results exactly |
| 5.2.4 | Complex tier migration (`complexexpr` behaviours) | 10 | `test_complex_expr`'s 122 checks pass against the new evaluator |
| 5.2.5 | List tier migration (`listexpr`) | 10 | `test_lists`' 241 checks pass |
| 5.2.6 | Matrix tier migration (`matexpr`) | 12 | `test_matrix`' 408 checks pass, including D48's depth-cap tests |
| 5.2.7 | Cross-tier cases that were previously impossible (§7) | 8 | Complex-element matrices, list⊗matrix ops behave per §7 |
| 5.2.8 | Retire the three old evaluators; remove their caps | 3 | `kMaxParseDepth` constants gone from `mat_expr.hpp`/`complex_expr.hpp` |
| 5.2.9 | On-device verification, both boards | 4 | Stack peaks measured and recorded; no regression vs the D48 figures |
| | **Total (provisional)** | **73** | |

## 5. Sizing — the open constraint

A tagged union over `{real, complex, matrix-ref, list-ref}` is **larger per node
than any of today's three narrower working types**. §F flagged this as needing a
real sizing pass against Pico 1 headroom before committing, and that headroom is
now thinner than when the concern was raised: `.bss` is **211,356** after D48,
leaving roughly 51 KB above the 48 KB MicroPython heap Phase 6B wants.

5.2.1 must produce an actual number before 5.2.3 starts. If the union is too
wide, the fallbacks are the `pre-phase5-review.md` levers (MicroPython heap
48→40 KB, ArrayStore slab cut, persistence `g_chunk` fold) — but spending them
here means Phase 6 inherits a tighter budget, so that is a trade to make
consciously, not by drift.

## 6. Risks and mitigations

- **This is a rewrite of three working, tested evaluators** against roughly
  1,200 host checks that currently pin their *separate* behaviours. The checks
  are the specification; where the unified evaluator must differ, that is a
  deliberate decision to record, not a test to quietly relax.
  *Mitigation*: per-tier cutover (5.2.4–5.2.6) so each tier's suite gates its own
  migration, rather than one big-bang switch.
- **Regression surface is largely on-device.** *Mitigation*: this is why
  [Phase 5.1](phase5.1-spec.md) is sequenced first — scripted line submission
  makes an unattended regression pass over the home screen possible for the first
  time.
- **Silent wrong answers, not crashes.** D47/D48 made several buffers `static` on
  non-reentrancy arguments verified by inspection. A unified evaluator changes
  the reentrancy assumptions those rest on. *Mitigation*: re-audit every `static`
  buffer on the home-screen path as part of 5.2.8, not as an afterthought.
- **Scope creep into `evaluate_real()`.** *Mitigation*: §2 is a hard boundary;
  any proposal to touch the graphing path is a separate decision requiring its
  own performance measurement.

## 7. Open questions

| # | Question | Notes |
|---|----------|-------|
| P5.2-1 | Migration strategy: parallel implementation behind a build flag, or per-tier cutover in place? | §4 assumes per-tier; a flag costs bss but allows A/B on hardware. Decide in 5.2.1. |
| P5.2-2 | What *new* cross-tier behaviours become reachable, and are they all wanted? | Unification makes complex-element matrices and list⊗matrix ops fall out for free. Some may be undesirable or need TI-parity checks before being exposed. |
| P5.2-3 | Does the explicit stack live in bss or the CAS arena? | The CAS `ExprPool` is already two-ended with LIFO scratch (D45) and may be the natural home rather than a second allocator. |
| P5.2-4 | Does idea H (polymorphic variables, D40) become cheap once this lands? | §H notes unified storage "almost certainly means a fourth format change". Worth re-costing after, not before. |
| P5.2-5 | Do the retired parsers' error strings need to be preserved verbatim? | Host tests assert on exact strings ("Too deeply nested", "Dim mismatch"). Changing them is a test churn cost to budget. |

## 8. Non-goals

- Touching `evaluate_real()` / tinyexpr (§2).
- Idea H, polymorphic variables — unscheduled, revisit only if real usage demands
  it (D40, and P5.2-4 above).
- Raising any depth cap as a standalone change. The caps exist for measured
  reasons; they disappear when the parsers they guard do (5.2.8), not before.

## References

- [design-departures-matrix-complex.md](../notes/design-departures-matrix-complex.md) §F
- [decisions.md](../notes/decisions.md) D37, D40, D45, D46, D47, D48
- [phase4-spec.md](phase4-spec.md) §5.2 — the performance guardrail
- [pre-phase5-review.md](../notes/pre-phase5-review.md) — the SRAM levers §5 may need
