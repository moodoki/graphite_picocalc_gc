# Phase 5.2 Spec: Unified Evaluator (idea F)

**Prerequisite phases**: Phase 4A–4C (the three evaluators being unified, and
the `Complex` type), Phase 5 (CAS — sequenced after deliberately, so that any
fourth symbolic evaluator is known before unification), and
[Phase 5.1](phase5.1-spec.md) (serial injection — see §6, it is the main
practical mitigation for this phase's regression risk).

**Scope**: Replace `math::matexpr`, `math::complexexpr` and `math::listexpr`
with **one** evaluator over a single tagged `Value`
(`{kind: real | complex | matrix | list, …}`), structured as a
**shunting-yard compiler producing a flat RPN program, run by a stack machine**
— neither phase recurses on the C++ call stack. A matrix of complex numbers, a
complex scalar times a matrix, a list containing complex elements, a dot product
between two list-vectors — all stop being special cases and become the same
generic binary-op dispatch over a wider tag enum.

It also **absorbs the shared function catalogue** (`catalog.hpp`), so the home
screen no longer escapes to tinyexpr for scalar spans.

**Status**: **In progress — tasks 5.2.1-5.2.8 done 2026-08-09.** The evaluator
now compiles and runs every home-screen value kind — real and complex scalars,
lists and matrices of either — with the whole callable surface resolved
natively, and it commits its own results: one store grammar, one flag
convention, and a probe mode that writes nothing. **What is left is the
differential harness (5.2.9) and the cutover (5.2.10-5.2.12); nothing is wired
to a screen yet** (§6.1).
Sizing: §5. Migration strategy: §6.1. Idea F has been a committed follow-on
since 2026-07-24 (D37); **judged worth the effort 2026-08-08 (D48)** and given a
phase number at the same time. This is the highest-risk item on the project's
list, by its own long-standing description.

5.2.1's headline is that the sizing concern attached to this idea since 2026-07-24
**does not survive measurement** — the tagged union is *smaller* than what
`matexpr` already uses, and retiring the three evaluators frees ~10 KB of bss
rather than consuming any. See §5.

**Why "5.2" and not "5F" or "Phase 7"** — per the naming convention in
`AGENTS.md`: lettered sub-phases are planned work a phase's completion depends
on, dotted ones are significant units that *turned up* and sit outside the
parent phase's goals. Idea F emerged from Phase 4's design-departures review and
was reinforced by Phase 5's bug work (D46, D48), but unifying the evaluators was
never part of what Phase 5 set out to deliver — Phase 5 closed without it. It is
substantial — §4 now sizes it at **~146 hrs**, larger than Phase 6B — so the
dotted form is a judgement call: it is a consequence of earlier phases rather
than a new goal of its own, and it belongs before Phase 6 rather than alongside
it.

**The size question was raised and settled 2026-08-09.** An earlier draft of
this paragraph said promotion to a full phase number "would be the honest move"
if the estimate grew materially — and it then doubled, from 73 to 146 hrs, once
5.2.1's surface inventory landed. The call was to **keep 5.2**: provenance, not
size, is what the convention turns on. Recorded here so a later reader sees the
test was applied rather than forgotten.

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
| home screen | `matexpr` + `complexexpr` + `listexpr` + tinyexpr via `eval_field` | one unified evaluator |

**"Four → two" holds only because the catalogue is absorbed** (decided
2026-08-09). Today every unrecognized scalar span in all three evaluators
escapes to `eval_field` → `Engine::evaluate_at` → tinyexpr, which is the only
place `pi`, `e`, `ans`, `theta` and `catalog.cpp`'s ~60 functions resolve. Left
in place, that escape hatch would keep tinyexpr reachable from the home screen
and the claim would be "four → three".

Absorbing it is cheap because **`catalog.hpp` is already evaluator-agnostic
shared data** — "one source of truth, so help cannot drift from the parser".
`engine.cpp:194-200` consumes it as `{name, fn, TE_FUNCTION0 + arity}`, plain
`double`-taking function pointers behind a `const void*`, so the unified
evaluator needs an arity dispatcher (0–4), not sixty ports. Two consequences:

- **Non-real arguments need a policy.** Catalog entries are `double`-only. Where
  `complexexpr`'s `kFns` has a complex counterpart (`sqrt`, `exp`, `ln`, the trig
  set, `abs`, `arg`, `conj`, `real`, `imag`) dispatch by `Value` kind; otherwise
  error on a non-real argument. The `m_*` angle-mode wrappers
  (`complex_expr.cpp:115-139`) **must survive** — they are D46's fix.
- **Help-only rows gain real bindings.** `sum`/`mean`/… are `fn == nullptr` rows
  today because they live in `list_expr` and aren't tinyexpr-callable. After
  unification they become genuine entries, removing a standing inconsistency in
  the help browser.

## 3. The explicit-stack constraint (D48) — met by the compiled form

**Build the evaluator on an explicit operand/operator stack, not the call
stack.** This is a decision, not an option, and it is the difference between
this phase solving the problem and inheriting a fifth frame budget.

**Decided 2026-08-09 (5.2.1): a shunting-yard compiler emitting a flat RPN
program, executed by a stack machine.** One mechanism satisfies two requirements
that looked independent:

- *This constraint.* A stack machine has no evaluation recursion, and an
  iterative shunting-yard parser has none at compile time either. Depth becomes
  a sized array in bss instead of a cliff.
- *The list lift's performance contract.* `listexpr` today binds list names to
  scalar slots, **compiles once via tinyexpr, then evaluates per element** in
  256-element chunks against PSRAM-backed arrays (`eval_lift`,
  `list_expr.cpp:1002`). A flat program keeps that: parsing happens once and
  the per-element work is instruction dispatch. An interpreter that re-parsed
  per element would be $N$ times slower on a 999-element list — the easiest way
  to wreck this phase.

Shape:

| piece | role |
|---|---|
| `Value` — 24 B tagged union (§5) | operand type |
| instruction array | push-literal / push-ref / make-list / binop / unop / call(fn,arity) / quoted-body jump / index / store |
| operand stack | `Value[N]` in bss; N = 64 costs 1,536 B |
| operator stack | compile-time only |
| result temporaries | `Array` handles over the existing ArrayStore |

`substitute_reductions`' textual fixpoint and `eval_clift`'s separate narrow
grammar both disappear: reductions become ordinary functions over a list
`Value`, and complex lists fall out of the normal binop dispatch.

**How the lift itself works — revised 2026-08-09 (5.2.6).** The row above used
to read "element slots — rebound per index by the lift, program unchanged", and
the instruction set carried a `kPushElem` for it. Building the tier retired
that: **a program is not uniformly element-dependent.** In `l1/sum(l1)` the
reduction is loop-invariant, so re-running the whole program per element
recomputes an $O(N)$ reduction $N$ times — quadratic on an expression
`listexpr` does in linear time today, because it substitutes reductions
*before* lifting. Getting that back would need dataflow analysis to find the
invariant subranges plus a rewritten program to hoist them.

The tier instead **broadcasts at the instruction that consumes a list**: each
elementwise op streams its operands in 256-element chunks and materialises one
temporary `Array`. Every node is then evaluated exactly once per element by
construction, reductions included, and nesting (`mean(l1*2)+l3`) needs no
analysis at all. That is what §1's "the same generic binary-op dispatch over a
wider tag enum" already described; §3's element-slot row was the mechanism it
replaces. The cost is intermediate arrays — `sin(l1)+2*l2` streams three passes
where today's lift streams one — drawn from the ArrayStore temporaries
`listexpr` already pays for. **5.2.12 measures it against today's lift on
hardware; that measurement settles whether the trade was right, not this
paragraph.**

One mechanism did survive from the element-slot idea, for the one case that
genuinely needs deferred evaluation: `seq(expr, var, lo, hi, step)` compiles
its body as a **quoted range** the top level jumps over, and the machine
re-enters that range per index with the loop variable bound (saved and
restored). It is the machine's only re-entry point and is depth-capped at 2.

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

Revised 2026-08-09 after 5.2.1's surface inventory and the four decisions taken
from it (all-three-at-once, compiled form, catalogue absorbed, superset store).
The previous 5.2.2-5.2.9 list predates all of them.

| # | Task | Est. hrs | Acceptance |
|---|------|----------|------------|
| ~~5.2.1~~ | ~~Tagged `Value` design + sizing pass~~ **DONE 2026-08-09** | 4 | §5: union 24 B vs matexpr's 32 - go. Migration strategy §6.1 |
| ~~5.2.2~~ | ~~`Value`, operand stack, instruction encoding~~ **DONE 2026-08-09** | 8 | Sizes fixed; opcode set written down; stack depth chosen and costed |
| ~~5.2.3~~ | ~~Shunting-yard compiler -> RPN program~~ **DONE 2026-08-09** | 20 | Iterative, no parse recursion. Precedence matches `mat_expr.cpp:812-865` |
| ~~5.2.4~~ | ~~Stack machine: real + complex tiers~~ **DONE 2026-08-09** | 16 | `test_complex_expr`'s checks pass; `m_*` angle-mode wrappers preserved (D46) |
| ~~5.2.5~~ | ~~Absorb catalogue, constants, variables~~ **DONE 2026-08-09** (three tables, not one) | 10 | `pi`/`e`/`ans`/`theta`/`a-z` and all `catalog.cpp` entries resolve natively; non-real policy implemented |
| ~~5.2.6~~ | ~~List tier incl. the lift~~ **DONE 2026-08-09** (broadcast, not element slots — §3) | 22 | `test_lists` passes; **compile-once/eval-N preserved**, 256-element chunks |
| ~~5.2.7~~ | ~~Matrix tier~~ **DONE 2026-08-09** (`dim`/`eigenvals` no longer need to stand alone) | 20 | `test_matrix` expression-layer passes incl. stand-alone `dim`/`eigenvals`/`mat2list` |
| ~~5.2.8~~ | ~~Superset store grammar + commit semantics~~ **DONE 2026-08-09** (both outstanding narrowings closed) | 10 | All five target forms; one flag convention; **no-commit mode** for the REAL probe |
| 5.2.9 | Differential harness | 12 | Snapshot/restore of Ans, matrices, lists, named lists, vars between runs |
| 5.2.10 | Widened behaviours + allow-list | 10 | Each widened case has a written expectation and a TI-parity rationale |
| 5.2.11 | Retire the three evaluators; remove their caps | 6 | Sources deleted; **bss drop measured** - deletion is what banks it |
| 5.2.12 | On-device verification, both boards | 8 | Stack peak, lift timing vs today, bss delta, serial-injection differential |
| | **Total** | **146** | |

**On the estimate.** This is roughly double the 73 hrs written before the
inventory, and it is a consequence of the four decisions rather than of scope
creep. The phase number **stays 5.2** (decided 2026-08-09): provenance is the
criterion - this turned up as a consequence of Phases 4 and 5 rather than being
a goal of its own. Recorded explicitly so the convention's size test is seen to
have been considered rather than ignored.

## 5. Sizing — DONE 2026-08-09 (task 5.2.1). The premise was wrong.

**§F's long-standing concern does not survive measurement.** Since 2026-07-24,
idea F has carried the caveat that a tagged union over
`{real, complex, matrix-ref, list-ref}` is *"larger per node than any of today's
three narrower working types"*, and the first draft of this section repeated it.
Measured on the target (`arm-none-eabi-g++ -mcpu=cortex-m0plus`, 4-byte
pointers):

| type | bytes |
|---|---|
| `Complex` | 16 |
| **`matexpr::Value` — today** | **32** |
| **tagged union — proposed** | **24** |
| same fields without a union | 32 |

The union is **8 bytes smaller than what `matexpr` already uses**, because
`matexpr::Value` carries a full `Complex` *and* a `const Array*` without
overlapping them (`{bool is_matrix; Complex s; const Array* m;}`,
`mat_expr.cpp:113`). Only `complexexpr`'s bare `Complex` is narrower, and it
carries no tag at all. **Go on the union width.**

### The budget item is the explicit stack, not the value

D48's constraint (§3) moves depth out of call frames and into an array, so the
real question is bss, not per-value width. Against that, retiring the three
evaluators **frees** memory:

| owner | bss |
|---|---|
| `listexpr` string scratch (`g_step` 3072, `g_seq` 1536, `g_literal` 1280, `g_top` 768, `g_reduce` 768, `g_range` 512, `g_clift` 512, …) | ~8.4 KB |
| `matexpr` (`parse_matrix_literal::vals` 1024, `parse_scalar_span::span` 256, `g_ctext` 64, …) | ~1.4 KB |
| `complexexpr` (`parse_scalar_span::span` 160, …) | ~0.2 KB |
| **total across 19 symbols** | **10,053 B** |

Most of that is an artifact of the architecture being replaced: **`listexpr`
works by string rewriting**, not by evaluating over values —
`substitute_reductions(char* buf, …)`, `eval_lift`'s `rw` ("rewritten
expression"), `extract_operands`' spans. Those buffers hold intermediate *text*
that a value-based evaluator never produces.

An explicit stack of 64 operand slots costs `64 x 24 = 1,536 B`, plus a small
operator stack. **So 5.2 is expected to free several KB of bss rather than
consume any** — the opposite of the assumption Phase 6's headroom argument was
resting on, and worth re-checking against 6B's 48 KB MicroPython heap once the
real numbers land.

**Measured as built, through 5.2.8** (Pico 1, `.bss`):

| item | bytes |
|---|---|
| operand stack `Value[64]` | 1,536 |
| `Program` (code 1,024 + consts 1,024 + 16 bookkeeping and the pending store name) | 2,064 |
| operator stack `OpTok[64]` | 896 |
| list tier: 6 `Array` handles + pool flags + arena pointer | ~160 |
| **total** | **~4.3 KB, against the 10,053 B retiring the three evaluators frees** |

The list tier costs no buffers: its 256-element chunk staging overlays the
shared compute arena (`scratch.hpp`) and its result temporaries take storage
from the existing `ArrayStore`, which is what `listexpr`'s `g_op`/`g_temp`/
`g_result` do today. `Program` shed 8 B in 5.2.6 with `n_elem_slots`, the field
the superseded element-slot lift needed (§3), and took them back in 5.2.8 for
the pending `-> name` store target — which is text, not a ref, precisely so the
registry entry is created when the store *commits*.

**Nothing is banked until 5.2.11 deletes the old evaluators**: measured `.bss`
is **217,396 B before and after 5.2.8**, and identical again at 5.2.6, because
the linker garbage-collects the whole evaluator while no screen calls `run()`.
The figures in this section are therefore the sizes of the types, not of the
image; the image number that matters is the one 5.2.11 produces.

A depth of 64 is also far more generous than what the call stack affords today:
`matexpr` is capped at **3** (D48, 84 B of margin before the leaf fix),
`complexexpr` at 7/4, tinyexpr at 7. Lifting those caps is the user-visible
payoff of the whole phase.

### What this does not settle

The 24 B figure is the *working value*. If the evaluator grows an AST or node
pool rather than evaluating straight off the stack, that is a separate budget
and needs its own measurement — do not carry this number over to it.

## 6.1 Migration strategy — decided (P5.2-1, 5.2.1)

**Parallel implementation behind `PICOCALC_UNIFIED_EVAL`, cut over per tier, then
delete.** Not in-place per-tier rewriting.

The deciding argument is that ~1,200 host checks currently pin the three
evaluators' *separate* behaviours, and those checks are the only specification
of what the unified evaluator must reproduce. Rewriting in place destroys the
reference while you are trying to match it.

Two things that were not available when this trade-off was first framed make the
parallel route cheap:

1. **bss is freed, not spent** (§5), so carrying both evaluators through the
   transition is affordable — the thing that made a flag look expensive.
2. **Differential testing is now possible.** Rather than rewriting the existing
   suites, run every expression they already contain through *both* evaluators
   and assert identical results. That turns 1,200 hand-written checks into a
   conformance harness for free, and it fails loudly at exactly the inputs that
   matter instead of wherever someone thought to look. Phase 5.1's serial
   injection extends the same idea to hardware: two firmware builds, the same
   scripted expression list, diff the outputs.

Sequence: build the evaluator behind the flag (5.2.2-5.2.3) → add the
differential harness → bring tiers up one at a time (5.2.4-5.2.7), each gated by
its own suite passing *differentially* → the store grammar (5.2.8) → flip the
default → delete the old evaluators and their caps (5.2.11). Deletion is what
banks the bss saving, so it is not optional cleanup.

**Deviation so far, recorded 2026-08-09**: the harness (5.2.9) has not been
built yet, so 5.2.4-5.2.6 were gated by *mirrored* checks in `test_unified`
rather than differential ones — each tier's behaviours restated against the
old suite's expectations by hand. That is weaker, and it is why 5.2.9 must land
before the default flips rather than after.

**Where differential testing does not apply**: cases that are new by
construction (§7's cross-tier behaviours — complex-element matrices, list⊗matrix)
have no old behaviour to compare against. Those need hand-written expectations
and a TI-parity judgement, and should be listed explicitly rather than absorbed
into "the suite passes".

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
  **Done 2026-08-09 — §6.2.**
- **Scope creep into `evaluate_real()`.** *Mitigation*: §2 is a hard boundary;
  any proposal to touch the graphing path is a separate decision requiring its
  own performance measurement.

### 6.2 The `static`-buffer audit — done 2026-08-09 (5.2.8)

Every `static` buffer reachable from a home-screen Enter, and what the unified
evaluator does to the argument it rests on:

| owner | buffers | verdict |
|---|---|---|
| `engine.cpp` | `preprocess::tmp`, `eval_internal::processed`, `Engine::evaluate::body`, `rebuilt`, `names[26][2]` | **Unreachable.** The unified evaluator calls `engine()` only for `vars()` — the variable store, never the parser. Absorbing the catalogue (5.2.5) removed the `eval_field` escape that made these reachable from three evaluators at once. |
| `complex_expr.cpp`, `mat_expr.cpp` | `parse_scalar_span::span` (both), `parse_matrix_literal::vals` | **Unreachable** — inside the parsers being retired. |
| `list_expr.cpp` | `s_va`/`s_vb` (the vector ops) | **Unreachable**; `dot`/`cross`/`norm` were reimplemented over the temp pool in 5.2.7. |
| `home_screen.cpp` | `g_eval`, `g_hist_io` | Unchanged. Dispatcher-owned display and IO scratch, one Enter at a time. |
| unified evaluator | `g_ops`, `g_stack`, `g_temps`, the arena overlay | Non-reentrant **by design**, and the design is what makes it safe: one program, no nested compile, no evaluation recursion. |

The audit found one real problem, and it was not a buffer. **`mat2list` writes
list slots the operand stack may still be holding by reference**:
`l1 * mat2list([A], l1)` reads l1's *new* contents, because a `Value` names the
slot rather than a snapshot. Unification is what made this reachable —
`matexpr` forbade the composition outright ("mat2list must stand alone") and
5.2.7 dropped the restriction along with the whole-expression parsing it was
tangled in. It is back, now as a shape rule on the compiled program: a call that
writes its arguments must be the last instruction and cannot be stored. The
in-place sort needs no such rule because it only writes when it *is* the whole
program.

Stated as an invariant so a later change cannot quietly break it: **`kStore`
executes with exactly one value on the operand stack**, and the machine asserts
it. A store emitted inside a quoted body would violate it.

## 7. Open questions

| # | Question | Notes |
|---|----------|-------|
| ~~P5.2-1~~ | ~~Migration strategy~~ | **DECIDED 2026-08-09 (5.2.1): parallel behind a flag, with differential host testing.** See §6.1. |
| P5.2-2 | What *new* cross-tier behaviours become reachable, and are they all wanted? | Unification makes complex-element matrices and list⊗matrix ops fall out for free. Some may be undesirable or need TI-parity checks before being exposed. |
| P5.2-3 | Does the explicit stack live in bss or the CAS arena? | The CAS `ExprPool` is already two-ended with LIFO scratch (D45) and may be the natural home rather than a second allocator. |
| P5.2-4 | Does idea H (polymorphic variables, D40) become cheap once this lands? | §H notes unified storage "almost certainly means a fourth format change". Worth re-costing after, not before. |
| P5.2-5 | Do the retired parsers' error strings need to be preserved verbatim? | Host tests assert on exact strings ("Too deeply nested", "Dim mismatch"). Changing them is a test churn cost to budget. **Partly answered 2026-08-09 (5.2.8)**: the criterion is provenance — a string that states a decision is kept verbatim ("e is reserved (Euler's e)"), a string that fell out of a parser accident is replaced. Both store-grammar cases are listed below. |

### Widenings found so far — the running list 5.2.10 signs off

Each is a case the retired evaluator rejected and the unified one answers. They
are pinned by value in `test_unified` already, so the widening is deliberate;
what 5.2.10 owes each row is a TI-parity judgement.

| # | Was | Now | Where it comes from |
|---|---|---|---|
| W1 | `sin(l1)` on a complex list → *"Complex lists support only +, -, scalar \* and /"* | maps elementwise | `eval_clift`'s narrow grammar is gone (5.2.6) |
| W2 | `l1*l1` on complex lists → *"Complex lists: one list per term"* | elementwise product | same |
| W3 | `2/l1` on a complex list → *"Cannot divide by a list"* | elementwise reciprocal | same |
| W4 | `sum(l1)+1` on a complex list → *"Complex sum/mean must stand alone"* | composes | a reduction returns a `Value`, not spliced text (5.2.6) |
| W5 | `sqrt({4,-1})` → NaN element | promotes the list to complex | scalar `sqrt(-4) = 2i` applied elementwise (5.2.6) |
| W6 | `sort_asc(sort_asc(sort_asc(sort_asc(...))))` → *"Too deeply nested"* at 3 levels | evaluates | the cap was a call-frame budget (D47), and there are no frames now (5.2.6) |
| W7 | `2*dim([A])`, `sum(dim([A]))` → *"dim/eigenvals must stand alone"* | compose | `matexpr::Value` could not hold a list; this one can (5.2.7) |
| W8 | `eigenvals` of a complex-conjugate spectrum → unstorable display text (D30/P4-7) | a complex list | "lists are real-only" stopped being true in 4D.24 (5.2.7) |
| W9 | `det(([A]*([A]+[A]))+[A])` → *"Too deeply nested"* (depth 4) | evaluates | `matexpr`'s cap is 3 with 84 B of margin — the same call-frame budget (5.2.7) |
| W10 | `list2mat(range(1,3), l2)` → *"list2mat takes l1-l6 args"* | takes any list expression | its arguments are values now, not tokens (5.2.7) |
| W11 | `mat2list([A], costs)` → *"mat2list targets are l1-l6"* | named lists work as targets | one ref numbering since 4D.13 (5.2.7) |
| W12 | `l1 -> a` → *"Syntax error"* from whichever parser claimed the line | *"Store target mismatch"* | one store grammar instead of four rightmost-`->` searches (5.2.8) |
| W13 | `<matrix expr> -> name` → no such form; matrices stored only to `[A]`-`[J]` and lists only from `listexpr` | every value kind reaches every target it fits | the target grammar stopped being per-evaluator (5.2.8) |

**Both narrowings 5.2.7 left open are closed** (5.2.8), neither silently:

- `sort_asc(l4)` with a bare list argument sorts `l4` **in place** again. The
  expression tier still evaluates it by value; the in-place half is recovered as
  an *implicit store* back to the same ref, emitted only when the whole program
  is `push-ref; sort`. That is where it belongs — `listexpr`'s in-place form was
  always a statement, and statements are what the store grammar covers.
  `sort_asc(l4) -> l5` writes both refs, as `list_expr.cpp:1387` does.
- A complex *result* built from real data (`i*[B]` in REAL mode) is rejected
  again, and the layering question that held it up has an answer: **the gate is
  on the mode, not on the layer.** `Mode::kCommit` refuses a non-real result
  ("Non-real result", D30); `Mode::kProbe` computes it and writes nothing. The
  three retired evaluators split this three ways — `matexpr` and `listexpr`
  gated because they wrote their own slots, `complexexpr` never gated because
  its caller committed for it — and the split was always about who owned the
  commit, not about what the value was. One evaluator owns both now.
  Intermediates are still never gated: `i^2` is `-1` and `abs(3+4i)` is `5` in
  any mode.

One deliberate narrowing is **added**, from §6.2's audit: `mat2list` may not
compose or be stored ("mat2list must stand alone"), which restores `matexpr`'s
rule for the reason `matexpr` had it.

Two divergences in error *strings*, which is the concrete part of P5.2-5's
answer. Both are cases where the old text came from a parser accident rather
than a decision, and both are pinned in `test_unified`:

- `1->a->b` and `1->a+2` reported *"Syntax error"* from inside tinyexpr, because
  the rightmost-arrow search silently left the arrow in the body. Both are
  *"Bad store target"* now.
- `2->l1` keeps `listexpr`'s *"Store target needs a list"*; every other kind
  mismatch keeps `matexpr`'s *"Store target mismatch"*. Two strings, each for
  the input it is pointed about, rather than one generic one.

What 5.2.8 deliberately did **not** take is the *display* strings —
`mat2list`'s "Done (n lists)" and the `num⇒a` store echo are formatting, and the
evaluator now hands a dispatcher everything they need (the count as the value,
`Commit::lists_mask`, `Commit::var`). They belong with the dispatcher in 5.2.10.

`matexpr`'s *"Undefined result"* for a non-finite scalar is **not** carried over:
the engine and `complexexpr` both commit `inf`/`nan` to Ans today, so gating it
here would change the majority behaviour rather than preserve it. If that is
wanted it is a display decision for 5.2.10, applied to all four kinds at once.

## 8. Non-goals

- Touching `evaluate_real()` / tinyexpr (§2).
- Idea H, polymorphic variables — unscheduled, revisit only if real usage demands
  it (D40, and P5.2-4 above).
- Raising any depth cap as a standalone change. The caps exist for measured
  reasons; they disappear when the parsers they guard do (5.2.11), not before.

## References

- [design-departures-matrix-complex.md](../notes/design-departures-matrix-complex.md) §F
- [decisions.md](../notes/decisions.md) D37, D40, D45, D46, D47, D48
- [phase4-spec.md](phase4-spec.md) §5.2 — the performance guardrail
- [pre-phase5-review.md](../notes/pre-phase5-review.md) — the SRAM levers §5 may need
