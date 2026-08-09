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

**Status**: **In progress — tasks 5.2.1-5.2.11 done 2026-08-09. `matexpr`,
`complexexpr` and `listexpr` are gone.** One evaluator compiles and runs every
home-screen value kind — real and complex scalars, lists and matrices of either
— resolves the whole callable surface natively, and commits its own results.
**3,903 lines deleted**, four parsers down to two, and the three depth caps with
them. The ~770 checks those suites carried were *kept*, ported onto the new
evaluator through a shim (`tests/host/eval_shim.hpp`); the differential harness
that blessed the migration retired with the thing it compared against. Net
against the pre-phase baseline: **-6,888 B of bss, +1,500 B of text**. **What is
left is on-device verification (5.2.12).**
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
substantial — §4 now sizes it at **~150 hrs**, larger than Phase 6B — so the
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

**Three of those four caps no longer exist** (5.2.11): `matexpr`'s 3,
`complexexpr`'s 7/4 and `listexpr`'s `kMaxRec` of 3 went with the parsers they
guarded. tinyexpr keeps its 7 — it still owns the graphing path (§2). Depth is
now 64 operand-stack slots in bss, and the inputs those caps rejected —
including `det(([A]*([A]+[A]))+[A])`, the expression that hard-faulted the
Pico 1 — evaluate.

## 2. Scope boundary — four parsers become two, not one

**`evaluate_real()` (tinyexpr) is never touched.** `phase4-spec.md` §5.2
established the guardrail when 4C landed: *"making the default numeric path
complex would double arithmetic cost on the hot graphing loop… keep two
evaluation entry points"*. That generalizes directly — a unified tagged-`Value`
evaluator must stay **strictly home-screen-only**, exactly as
`evaluate_complex()` is today. Graphing, tables and stats keep the narrow, fast
real path.

**The boundary holds, but its stated reason does not — recorded 2026-08-09
(D50), so a later reader does not re-derive it.** "Would double arithmetic cost"
describes the design 4C was weighing, a `Complex` numeric value type. This
evaluator keeps a real tier: real ⊕ real never touches complex arithmetic. What
actually argues for the boundary is different and is written down in D50 and
P5.2-7 — fixed-size `Program`s, non-reentrant compile/run singletons, the
`kCompute` arena invariant, and the absence of any differential corpus off the
home screen. The question is revisited after this phase closes, with §9's
measured numbers.

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
paragraph** — §9, rows M2 and M3.

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
| ~~5.2.9~~ | ~~Differential harness~~ **DONE 2026-08-09** (494/518 agree; 3 real bugs found) | 12 | Snapshot/restore of Ans, matrices, lists, named lists, vars between runs; the [change register](../notes/unified-evaluator-changes.md) is its allow-list |
| ~~5.2.10~~ | ~~Widened behaviours + allow-list~~ **DONE 2026-08-09** (default flipped; bss -6,720 already) | 10 | Every register row signed off with a TI-parity rationale; the register's D-rows (display strings, REAL-mode probe sequencing) discharged |
| ~~5.2.11~~ | ~~Retire the three evaluators; remove their caps~~ **DONE 2026-08-09** (3,903 lines deleted, ~770 checks kept) | 6 | Sources deleted; **bss drop measured** - deletion is what banks it |
| 5.2.12 | On-device verification, both boards | 12 | Stack peak, bss delta, serial-injection differential, and **§9's A/B latency measurement against the v0.3.1 release binary** |
| | **Total** | **150** | |

**On the estimate.** This is roughly double the 73 hrs written before the
inventory, and it is a consequence of the four decisions rather than of scope
creep. The phase number **stays 5.2** (decided 2026-08-09): provenance is the
criterion - this turned up as a consequence of Phases 4 and 5 rather than being
a goal of its own. Recorded explicitly so the convention's size test is seen to
have been considered rather than ignored.

**146 → 150, 2026-08-09**: 5.2.12 gains 4 hrs for §9's A/B latency pass against
the old pipeline. The phase replaces the home screen's evaluator, so "is it
faster or slower than what it replaces" needs a measurement rather than an
argument — and §3 has been promising exactly that measurement since 5.2.6. The
4 hrs is the timing harness and the analysis; obtaining the baseline costs
nothing, because it is the v0.3.1 release `.uf2` (§9).

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

**Where the saving actually lands — measured 2026-08-09 at the 5.2.10 flip, and
not where this section predicted.** Through 5.2.9 `.bss` was **217,396 B**,
unchanged from 5.2.6, because the linker garbage-collects an evaluator no screen
calls. Flipping the default moved it to **210,676 B**: the same collector now
drops `listexpr`'s ~8.4 KB of string scratch, because nothing calls
`listexpr::evaluate` any more. Text went the other way, +4,216 B.

| build | text | bss |
|---|---|---|
| `PICOCALC_UNIFIED_EVAL=OFF` | 461,852 | 217,396 |
| `PICOCALC_UNIFIED_EVAL=ON` | 466,068 | **210,676** |

**Final accounting, 5.2.11.** Deleting the sources moved it a further 168 B of
bss and, notably, *added* 64 B of text:

| stage | text | bss |
|---|---|---|
| before the flip (`OFF`) | 461,852 | 217,396 |
| 5.2.10, flipped | 466,068 | 210,676 |
| 5.2.11, deleted | **463,352** | **210,508** |
| **net** | **+1,500** | **-6,888** |

So "deletion is what banks it" was **wrong, and worth correcting rather than
quietly restating**. `--gc-sections` banks essentially everything as soon as the
call sites disappear; by 5.2.11 the linker had already dropped what deletion
would have removed, and the remaining text belonged to the formatters and MatAns
— which *moved* rather than vanished. Deletion's real return is not bytes:

- **3,903 lines** of source gone (`mat_expr` 1,595, `list_expr` 1,717,
  `complex_expr` 591, headers included);
- three of four depth caps gone with the parsers that needed them (§1);
- and the class of bug this phase exists to remove — two evaluators disagreeing
  — is now unrepresentable on the home screen, because there is one.

The phase still nets bss back, and the +1,500 B of text is the honest price of a
stack machine that does more than the three parsers did.

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

**The deviation this section carried from 5.2.4 is closed.** Tiers 5.2.4-5.2.8
were gated by *mirrored* checks in `test_unified` — each tier's behaviours
restated by hand against the old suite's expectations — because the harness did
not exist yet. It does now (`tests/host/test_differential.cpp`), and it was
built before the default flips, as this section required.

**It found three things on its first run**, which is the case for having
insisted on it: postfix `!` was missing from the compiler outright (shipped
syntax that both retired scalar paths reached by *rewriting* the input, so there
was no grammar rule to port); the REAL-mode commit gate tested `im == 0`
exactly, rejecting `e^(i*pi)`, where the dispatcher it replaces uses
`Complex::is_real()`'s tolerance; and the store-mismatch error strings split by
target kind when today they split by which evaluator claimed the line, which
made `[A] -> l1` report the wrong one — including in a `test_unified` assertion
written by hand two tasks earlier. Mirrored tests cannot find any of those,
because all three are places where a hand-written expectation was simply wrong.

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
| ~~P5.2-6~~ | ~~Should tinyexpr's `(-2)^2 = -4` be fixed at the source?~~ | **DECIDED 2026-08-09 (D50): yes, as a separate bugfix outside 5.2.** `factor()` in the `TE_POW_FROM_RIGHT` build hoists a negation out of a power without knowing whether parentheses closed it, so the two shipped evaluators disagree on this input today (register F1). ~5 lines, parse-time only. Split out because patched at the source it fixes graphing too, where 5.2 alone fixes only the home screen — and because it is not gated on this phase. Until it lands, 5.2 trades a home-screen disagreement for a home-vs-graph one. |
| P5.2-7 | Should the unified evaluator replace tinyexpr on the numeric path too? | **Raised 2026-08-09 by P5.2-6; DEFERRED past 5.2 closure (D50).** §5.2's "would double arithmetic cost" argues against a `Complex` numeric path, not against this evaluator, which keeps a real tier — so the guardrail's stated reason does not transfer. Four costs do: a fixed 2,064 B `Program` vs a malloc'd tree (~120 B for `sin(x)+2*x`); `compile()`/`run()` are non-reentrant singletons and the numeric path re-enters; the `kCompute` arena invariant (`stats`/`matrix`/`infer` own it); and no differential corpus off the home screen. Gains: F1 fixed everywhere, four parsers → one, tinyexpr's depth-7 cap gone from graphing, 7,897 B of flash. **The missing input is §9's M1** — per-sample latency, stack machine vs tinyexpr. |
| P5.2-5 | Do the retired parsers' error strings need to be preserved verbatim? | Host tests assert on exact strings ("Too deeply nested", "Dim mismatch"). Changing them is a test churn cost to budget. **Partly answered 2026-08-09 (5.2.8)**: the criterion is provenance — a string that states a decision is kept verbatim ("e is reserved (Euler's e)"), a string that fell out of a parser accident is replaced. Both store-grammar cases are listed below. |

### Behaviour changes — moved out 2026-08-09 (5.2.8)

The running list of widenings, narrowings, grammar changes and error-string
divergences now lives in
**[unified-evaluator-changes.md](../notes/unified-evaluator-changes.md)** — the
behaviour change register. It was a table in this section through 5.2.7 and
outgrew it: by 5.2.8 the same rows had to serve three consumers with different
needs, and a design spec is the wrong shape for all three.

- **Host** — every row names the check in `test_unified.cpp` that pins it.
- **Differential (5.2.9)** — the register *is* the harness's allow-list. A diff
  that appears there is expected; a diff that does not is a bug, and nothing may
  be added to the allow-list without a row.
- **On device (5.2.12)** — the register's `Input` column is the serial-injection
  replay script (§9).

It also carries what this section never did: behaviours **preserved on purpose**
where unification made a change reachable (the D46 angle-mode wrappers, 4D.25's
complex-read gate, `mat2list`'s statement rule), and the coverage statement for
regions with no rows at all. **5.2.10 signs off every row**, and the finished
register is a byproduct deliverable at 5.2 closure.

Two entries are worth keeping here because they are design conclusions rather
than test rows:

- **Both narrowings 5.2.7 left open are closed** (5.2.8). `sort_asc(l4)` sorts
  in place again, as an implicit store — `listexpr`'s in-place form was always a
  statement, and statements are what a store grammar covers. And a complex
  *result* built from real data (`i*[B]` in REAL mode) is rejected again,
  because **the gate is on the mode, not on the layer**: `Mode::kCommit` refuses
  a non-real result (D30), `Mode::kProbe` computes it and writes nothing. The
  three retired evaluators split that three ways — `matexpr` and `listexpr`
  gated because they wrote their own slots, `complexexpr` never gated because
  its caller committed for it — and the split was always about who owned the
  commit, not about what the value was.
- **P5.2-5's criterion is provenance.** A string that states a decision is kept
  verbatim ("e is reserved (Euler's e)"); a string that fell out of a parser
  accident is replaced (`1->a+2` reported "Syntax error" from inside tinyexpr
  only because the rightmost-arrow search left the arrow in the body).

## 9. On-device performance measurement (5.2.12)

**This phase replaces the home screen's evaluator, so it must be measured
against the one it replaces, not merely against a budget.** §3's list-lift
design says so explicitly — the broadcast rewrite trades extra streaming passes
for once-per-element evaluation, and "5.2.12 measures it on hardware; that
measurement, not this comment, is what settles it".

**The baseline is a released binary, not a rebuild** (decided 2026-08-09). The
**v0.3.1** release carries `picocalc_graphcalc-pico.uf2` and
`picocalc_graphcalc-pico2.uf2`, and it is the right baseline by construction:
tagged 2026-08-08 at `48881a9`, it contains all of Phase 5.1 — serial injection
and the 5.1.7 `mode` command — and **none** of Phase 5.2. So the old pipeline
stays flashable indefinitely and there is **no ordering constraint on 5.2.11**;
an earlier draft of this section claimed one, on the assumption that both
pipelines had to be buildable from one tree. They do not. Nothing else needs
keeping either — the repository holds no compiled artifacts, and it does not
need to.

Checked rather than assumed: v0.3.1's injection block is **byte-identical** to
today's (`src/main.cpp`, the whole `inject:` region), and `stack: peak` is at
`main.cpp:549` there. One host script parses both builds with no conditionals.

**Method.** One script, three outputs, riding the mechanism Phase 5.1 already
built: serial injection auto-echoes `inject: "<expr>" -> "<result>" kind=…`, and
`stack: peak` prints on any new high-water mark (`main.cpp:713`, `main.cpp:549`).

**Timing is measured host-side**, as the round trip from writing the line to
reading its echo. That follows directly from the baseline being pre-built: a
firmware-side elapsed field cannot exist in a binary released before it was
written. A host round trip includes USB latency and render-loop scheduling,
which is why it is only usable as a *difference* — the same overhead sits on
both sides, so enough repetitions cancel it. Median of N per line; discard each
line's first run (cold caches, PSRAM wake).

A firmware-side elapsed field is still worth adding to the new build, not as the
A/B number but to **bound** it: the gap between the two says how much of the
round trip was never evaluation. Without it a small M1 delta cannot be
distinguished from USB jitter.

**What to measure**, chosen so each line isolates one claim:

| # | Input shape | What it answers |
|---|---|---|
| M1 | `2+3*4`, `sin(30)+ln(2)` | **Scalar entry latency.** The one nobody has costed: today a plain scalar goes through tinyexpr, and after this phase it does not. A regression here is felt on every keypress-to-answer, so it is the guardrail measurement, not a curiosity. |
| M2 | `sin(l1)+2*l2` at 999 elements | **The lift.** Three streaming passes here against `listexpr`'s one. §3's open trade. |
| M3 | `l1/sum(l1)` at 999 elements | **The correctness half of the same trade** — the loop-invariant reduction the element-slot design would have recomputed N times. Expected to favour the new evaluator; if it does not, the reasoning in §3 is wrong. |
| M4 | a 256-element list, and a 257-element one | **Chunk-boundary cost**, against the same pair on the old lift. |
| M5 | `det([A])`, `[A]*[B]` at 6x6 | **Matrix ops**, which should be unchanged — the same `matops` underneath, reached differently. A difference here means dispatch overhead, not arithmetic. It doubles as the **control** (see below). |
| M6 | `seq(x^2, x, 1, 200, 1)` | **Quoted-body re-entry**, the machine's only nesting, against `listexpr`'s per-element tinyexpr compile. |
| M7 | the register's replay script end to end | **Aggregate**, and the differential pass at the same time. |

**M5 is the control**, and it is what makes a released baseline sound. v0.3.1 is
a whole different commit, so any delta in principle includes everything else
that changed between it and the 5.2 build — not only the evaluator. M5 is the
row that should not move: same `matops`, same arithmetic, reached through a
different dispatcher. **If M5 moves, the other rows are not measuring the
evaluator**, and the run needs a closer baseline before its numbers mean
anything. That check costs one row and replaces an ordering constraint on a
whole task.

Comparing against a *shipped* build is also the more meaningful comparison, not
merely the cheaper one: it is what users have against what they will get.

**What counts as a pass.** M1 within noise of today or better; M5 unchanged
(else see above); M2 is the one genuinely open question and a regression there
is a finding to record and cost, not automatically a blocker — the phase's case
rests on correctness and ~10 KB of bss, and §3 already commits to reporting the
number either way. Both boards, because the RP2350's cache and the RP2040's lack
of one have diverged before (D14, the 4D.38 batch), and both `.uf2`s are in the
v0.3.1 release.

**Also in the same session**, unchanged from the task row: stack peak per input
(the whole point of moving depth off the call stack), the bss delta 5.2.11
banks, and the serial-injection differential.

### Amendment, 2026-08-09 — this method did not work; what replaced it (D52)

**Everything above about host-side round-trip timing is superseded.** It was
tried first, on hardware, and cannot resolve the evaluator:

- The round trip carries a **full-frame push** (`main.cpp:398` — "a full-frame
  push is ~200 ms"), giving a **~113 ms floor with ~80 ms spread** against an
  evaluator cost of 0.5-17 ms. The decisive observation is the ordering, not the
  ratio: the 999-element M2 row had a **lower minimum (80 ms) than `2+3*4`
  (104 ms)**.
- "Enough repetitions cancel it" is the specific claim that fails. The push cost
  depends on **the result being rendered**, so it is correlated with the thing
  under test, not independent overhead.
- Timing the whole `submit_line` fails too — it contains the SD history write.
  `2+3*4` was 19.0 ms of which **0.63 ms** was evaluation.

**What replaced it**: a firmware probe bracketing evaluation only (top of
`evaluate_input` to entry of `push_entry`, the funnel every branch passes
through), with the **baseline rebuilt from the v0.3.2 tag carrying the same
probe**, applied to both trees by one script so the instrumentation is provably
identical. `scripts/ab-measure.py` runs it.

**This also strengthens the M5 control rather than weakening it.** The released
binary was chosen partly so the comparison would not be confounded by unrelated
commits; the rebuilt baseline differs from `phase-5.2` by the evaluator work
alone, so M5's shift is dispatch overhead and nothing else. Note M5 *did* move
(+0.08 to +0.17 ms) — small in absolute terms, and §9 above already names a
difference there as dispatch overhead rather than arithmetic. Judge it in
milliseconds, not percent: on a 0.5 ms row a percentage says more about the
divisor than the change.

**The baseline is now the v0.3.2 release**, not v0.3.1 — it is what this branch
forks from, and it differs by exactly the thing under test.

Results, both boards, and the two regressions to record (M2 +36/+52%, M6
+54/+80%) are in **D52**, with the raw per-sample data and the reproduction
recipe at [`docs/notes/measurements/phase5.2/`](../notes/measurements/phase5.2/README.md)
— **the phase-closure write-up should cite that**. Depth results, including the one place the "depth costs
no call frames" claim needs a caveat, are there too.

## 8. Non-goals

- Touching `evaluate_real()` / tinyexpr (§2).
- Idea H, polymorphic variables — unscheduled, revisit only if real usage demands
  it (D40, and P5.2-4 above).
- Raising any depth cap as a standalone change. The caps exist for measured
  reasons; they disappear when the parsers they guard do (5.2.11), not before.

## References

- [unified-evaluator-changes.md](../notes/unified-evaluator-changes.md) — the
  behaviour change register: every widening, narrowing, grammar change and
  error-string divergence, each pinned to a host check and replayable on device
- [design-departures-matrix-complex.md](../notes/design-departures-matrix-complex.md) §F
- [decisions.md](../notes/decisions.md) D37, D40, D45, D46, D47, D48
- [phase4-spec.md](phase4-spec.md) §5.2 — the performance guardrail
- [pre-phase5-review.md](../notes/pre-phase5-review.md) — the SRAM levers §5 may need
