# Unified evaluator — behaviour change register (Phase 5.2)

**Last updated**: 2026-08-09 (through task 5.2.8)
**Status**: living document, accumulated as each tier lands. **5.2.10 signs off
every row**; the finished register is a byproduct deliverable at 5.2 closure.

**Purpose.** Phase 5.2 replaces three home-screen evaluators
(`math::matexpr`, `math::complexexpr`, `math::listexpr`) with one. Where the
unified evaluator answers differently from what ships today, that is a decision
to record, not a test to quietly relax
([phase5.2-spec.md](../phases/phase5.2-spec.md) §6). This file is the single
place those differences live, in a form that can be **checked against** rather
than read.

Related: [phase5.2-spec.md](../phases/phase5.2-spec.md) is the design and the
task plan; [decisions.md](decisions.md) carries the numbered decisions this
rests on (D30, D37, D46, D47, D48, D49);
[design-departures-matrix-complex.md](design-departures-matrix-complex.md) §F is
where the idea came from.

---

## 1. How to check this list

Three independent uses, which is why the table has an `Input` and a `Pinned by`
column rather than prose:

1. **Host** — every row is pinned by a named check in
   `tests/host/test_unified.cpp`. A row with no pin is not a decision, it is an
   accident waiting to be found on hardware.
2. **Differential (5.2.9, built)** — `tests/host/test_differential.cpp` runs
   **259 expressions harvested from the suites that pin the three retired
   evaluators, in both number modes**, through both pipelines from the same
   seeded state, and compares the result *and* everything each one wrote.
   **This register is exactly its allow-list** (`differential_allow.inc`, one
   row per id): a divergence with no row fails, and a row that never diverges
   fails too. Adding a row is a decision recorded here, not a way to quiet the
   test.
3. **On device (5.2.12)** — the `Input` column *is* the serial-injection replay
   script. Sent to a firmware built with the old pipeline and one with the
   unified evaluator, the observed diff must equal this table, row for row.
   (§4 of this file, and phase5.2-spec.md §9 for the timing pass that rides
   along with it.)

**Classes.** The id prefix is the class:

| prefix | meaning |
|---|---|
| **W** | **Widening** — rejected today, works now |
| **N** | **Narrowing** — works today, rejected now |
| **F** | **Fix** — today's answer is *wrong*, and unification corrects it |
| **G** | **Grammar** — what the parser accepts, or how something is spelled |
| **E** | **Error text** — same accept/reject, different message |
| **P** | **Preserved on purpose** — looked like it would change, deliberately did not |
| **D** | **Deferred** — owed by a later task, listed so it is not lost |

**F is the class this phase exists for** and it was added on 2026-08-09, when
5.2.9's first differential run produced one. D46 — the DEGREE-mode trig
disagreement — would have been an F row had it not been fixed before this file
existed. An F row is not a risk to sign off; it is the payoff.

"Today" throughout means shipped behaviour on `main`, not an intermediate state
of this branch.

---

## 2. The register

### Widenings

Every one of these is a case a retired evaluator rejected. What 5.2.10 owes each
is a TI-parity judgement, not a re-test — they are pinned already.

| # | Input | Today | Unified | Origin | Pinned by |
|---|---|---|---|---|---|
| W1 | `sin(l1)`, l1 complex | *"Complex lists support only +, -, scalar * and /"* | maps elementwise | `eval_clift`'s narrow grammar is gone (5.2.6) | `test_complex_lists` |
| W2 | `l1*l1`, l1 complex | *"Complex lists: one list per term"* | elementwise product | same | `test_complex_lists` |
| W3 | `2/l1`, l1 complex | *"Cannot divide by a list"* | elementwise reciprocal | same | `test_complex_lists` |
| W4 | `sum(l1)+1`, l1 complex | *"Complex sum/mean must stand alone"* | composes | a reduction returns a `Value`, not spliced text (5.2.6) | `test_complex_lists` |
| W5 | `sqrt({4,-1})` | NaN element | promotes the list to complex | scalar `sqrt(-4) = 2i` applied elementwise (5.2.6) | `test_list_chunking` |
| W6 | `sort_asc(sort_asc(sort_asc(sort_asc({3,1,2}))))` | *"Too deeply nested"* at 3 levels | evaluates | the cap was a call-frame budget (D47); there are no frames now (5.2.6) | `test_list_wrappers` |
| W7 | `2*dim([A])`, `sum(dim([A]))` | *"dim/eigenvals must stand alone"* | compose | `matexpr::Value` could not hold a list; this one can (5.2.7) | `test_matrix_bridge` |
| W8 | `eigenvals([D])`, complex-conjugate spectrum | unstorable display text (D30/P4-7) | a complex list | "lists are real-only" stopped being true in 4D.24 (5.2.7) | `test_matrix_bridge` |
| W9 | `det(([A]*([A]+[A]))+[A])` | *"Too deeply nested"* (depth 4) | evaluates | `matexpr`'s cap is 3 with 84 B of margin — the same call-frame budget (5.2.7) | `test_matrix_depth` |
| W10 | `list2mat(range(1,3), l2)` | *"list2mat takes l1-l6 args"* | takes any list expression | arguments are values now, not tokens (5.2.7) | `test_matrix_bridge` |
| W11 | `mat2list([A], costs)` | *"mat2list targets are l1-l6"* | named lists work as targets | one ref numbering since 4D.13 (5.2.7) | `test_matrix_bridge` |
| W12 | `l1->a` | *"Syntax error"* from whichever parser claimed the line | *"Store target mismatch"* | one store grammar instead of four rightmost-`->` searches (5.2.8) | `test_store_targets` |
| W13 | `dim([A])->mydim` | no such form — `matexpr`'s store targets have no named-list branch and `listexpr` never sees `dim([A])` | stores the list | the target grammar stopped being per-evaluator (5.2.8) | `test_store_targets` |
| W15 | `conj(a)`, and `real`/`imag`/`arg` of a real | *"Syntax error"* in REAL mode (tinyexpr has no `conj`), works in RECT (`complexexpr` does) | works in both | `complexexpr`'s complex-only table is one of the three the evaluator absorbed (5.2.5); it is no longer behind a mode | `test_differential` |
| W16 | `{1}+{2}+{3}+{4}+{5}` | *"Too many list terms"* | evaluates | `listexpr`'s `kMaxOperands` term cap. The temp pool still bounds this, but it bounds *live* temporaries rather than terms in the input (5.2.6) | `test_differential` |
| W14 | a non-finite scalar result on the matrix path, e.g. `det([A])/0` | *"Undefined result"* | returns `inf`, committed to Ans | the engine and `complexexpr` already commit `inf`/`nan`; gating only the matrix path was the odd one out (5.2.8) | — see N/D below |

**W14 is the one widening nobody asked for.** It is listed as a widening because
it accepts what was rejected, but the old behaviour was arguably better. Making
it uniform is a display decision across all four kinds and belongs to 5.2.10
(D3) — not to a single tier quietly keeping or dropping a gate.

### Fixes

| # | Input | Today | Unified | Origin | Pinned by |
|---|---|---|---|---|---|
| F1 | `(-2)^2` | **-4** on the real path, **4** on the complex path — the two shipped evaluators disagree | **4** | tinyexpr's `factor()` cannot tell `-2^2` from `(-2)^2`; the unified compiler keeps grouping (5.2.9) | `test_differential` |

**F1 in full**, because it is the second instance of the bug class that
justified this phase and it was found by the harness on its first run.

Type `(-2)^2` on the home screen today and the answer depends on the number
mode: **-4** in REAL, **4** in RECT — or in REAL if the expression happens to
mention `i` or reference a complex variable, since that is what routes an input
to `complexexpr`. `test_complex_expr.cpp:318` pins the complex path's `4`;
nothing pinned the real path's `-4`.

The mechanism is upstream, in `drivers/tinyexpr/tinyexpr.c`'s `TE_POW_FROM_RIGHT`
build of `factor()`:

```c
if (ret->type == (TE_FUNCTION1 | TE_FLAG_PURE) && ret->function == negate) {
    te_expr *se = ret->parameters[0];
    free(ret); ret = se; neg = 1;      /* hoist the negation out of the power */
}
```

By the time `factor()` runs, `(-2)` has already been parsed and the parentheses
are gone — the node is simply a `negate`, indistinguishable from the one `-2`
produces. So the negation is hoisted out of the exponentiation and re-applied
after: `-(2^2)`. `(0-2)^2` gives `4` on both paths, which is the same expression
with the negation spelled so it does not reach that test.

The unified evaluator has no such case: parentheses close an operand in the
shunting-yard, so `(-2)` is a finished value before `^` is applied. It agrees
with `complexexpr`, with the pinned test, and with the arithmetic.

**This does not reach graphing.** `evaluate_real()` is tinyexpr and §2 of the
spec makes it out of scope, so after 5.2 the home screen answers `4` and
`Y1=(-2)^X` still plots the tinyexpr reading. That trades a home-screen
disagreement for a home-vs-graph one. Fixing it properly means patching the
vendored parser — see **P5.2-6** in the spec, which is a decision, not
something a tier should take on its own.

### Narrowings

| # | Input | Today | Unified | Origin | Pinned by |
|---|---|---|---|---|---|
| *(none outstanding)* | | | | | |

Two narrowings were open between 5.2.6 and 5.2.8 and **both are closed**, which
is why this table is empty rather than absent:

- `sort_asc(l4)` sorting `l4` in place. The expression tier evaluates by value;
  the in-place half came back in 5.2.8 as an **implicit store**, emitted only
  when the whole program is `push-ref; sort`. See P4.
- Rejecting a complex *result* built from real data in REAL mode (`i*[B]`).
  Closed in 5.2.8 by putting the gate on the **mode** rather than the layer:
  `Mode::kCommit` refuses it, `Mode::kProbe` computes it. See P3.

A third briefly existed inside this branch and never shipped: 5.2.7 dropped
`matexpr`'s "mat2list must stand alone", and 5.2.8's audit put it back (P5).

### Grammar

| # | Change | Today | Unified | Pinned by |
|---|---|---|---|---|
| G1 | The store arrow is **compiled**, not string-searched | each evaluator scans for the *rightmost* `->` and re-trims the body around it (`engine.cpp:402`, `complex_expr.cpp:454`, `list_expr.cpp:1306`, `mat_expr.cpp:978`) | the first `->` ends the expression; the rest is the target | `test_store_compile` |
| G2 | Five target forms in one grammar | which targets exist depends on which evaluator claimed the line | `-> a`, `-> theta`, `-> l1`-`l6`, `-> name`, `-> [A]`-`[J]`, always | `test_store_compile` |
| G3 | `1->a->b` | parsed as `(1->a) -> b`, then failed inside tinyexpr | *"Bad store target"* — stores do not chain | `test_store_compile` |
| G4 | Depth limits | four separately-discovered call-frame caps: `matexpr` 3, `complexexpr` 7/4, tinyexpr 7 (D45/D47/D48) | one operand/operator-stack depth of 64, reported as *"Too deeply nested"* / *"Expression too complex"* | `test_compile_depth`, `test_depth_budget` |
| G5 | Scalar spans | escape to `eval_field` → `Engine::evaluate_at` → tinyexpr, the only place `pi`/`e`/`ans`/`theta` and `catalog.cpp` resolve | resolved natively; the escape hatch is gone, which is what makes "four parsers → two" true | `test_vm_constants`, `test_builtins` |
| G6 | `norm(x)` | Frobenius in `mat_expr`, Euclidean in `list_expr` — one name, two implementations that could never disagree because they could never meet | one entry, dispatched on the argument's kind | `test_matrix_bridge` |
| G7 | Whole-expression forms | `dim`/`eigenvals`/`mat2list`/`sort_asc`/`dot`/`cross`/`norm` are recognised by matching the *whole input string* | ordinary calls in one grammar (`mat2list` excepted — P5) | `test_matrix_bridge` |

G4–G7 are mechanism changes with user-visible consequences; G5 has none by
design and is listed so the differential harness knows to expect none.

### Error text

The criterion is **provenance** — the concrete part of open question P5.2-5. A
string that states a decision is kept verbatim; a string that fell out of a
parser accident is replaced.

| # | Input | Today | Unified | Pinned by |
|---|---|---|---|---|
| E1 | `1->a+2` | *"Syntax error"* (from tinyexpr — the arrow was left in the body) | *"Bad store target"* | `test_store_compile` |
| E2 | `2->l1` | *"Store target needs a list"* (`listexpr`) | unchanged | `test_store_targets` |
| E3 | `[A]->a`, `2->[C]`, `l1->[C]` | *"Store target mismatch"* (`matexpr`) | unchanged | `test_store_targets` |
| E4 | `2->A`, `2->e`, `2->i` | *"Variables are lowercase a-z"*, *"e is reserved (Euler's e)"*, *"i is reserved (imaginary unit)"* | unchanged, verbatim — these state decisions (D1/D11/D19/D30) and the silent case-fold one of them replaced was a wrong answer | `test_store_compile` |
| E5 | `sum(1)`, `det(1)` | *"Syntax error"* — help-only catalog rows with no implementation | *"Expected a list"* / *"Expected a matrix"* | `test_vm_errors` |
| E6 | `2->Theta` | *"Syntax error"* — the uppercase name matched no target, so the arrow was left in the body for tinyexpr | *"Bad store target"* | `test_differential` |
| E7 | `{1}->ans` | *"Syntax error"*, same mechanism | *"Bad store target"* — `ans` is a reserved name, not a target | `test_differential` |
| E8 | `{1,foo}` | *"Bad list element"* | *"Syntax error"* | `test_differential` |

E8 is the one row where the replacement is arguably less pointed, and it is
kept: `foo` is an unknown identifier *anywhere*, and the unified compiler
resolves identifiers before it knows what encloses them. "Bad list element"
described the position, not the problem.

### Preserved on purpose

Rows that a reader — or a reviewer of the diff — would reasonably expect to have
changed. Each one is a place unification made a behaviour *reachable* that had
been prevented structurally.

| # | Behaviour | Why it survives |
|---|---|---|
| P1 | REAL mode never reads complex data: a complex `[X]`, a complex literal, a complex list element | 4D.25's rule. Gating reads, not just results, is what stops a real part being taken silently. `test_matrix_complex` |
| P2 | `m_sin`/`m_cos`/… angle-mode wrappers scale the whole complex argument | **D46 itself** — the bug that motivated this phase. Without them DEGREE mode is ignored whenever Number mode is not REAL. `test_vm_complex` |
| P3 | REAL mode never commits or displays a non-real *result* | D30. In the unified evaluator the gate is on `Mode::kCommit`; intermediates are never gated, so `i^2` is `-1` and `abs(3+4i)` is `5` in any mode. `test_store_real_gate` |
| P4 | `sort_asc(l4)` sorts `l4` in place; `sort_asc(l4)->l5` writes both refs | `listexpr`'s in-place form is a statement. Restored as an implicit store; persistence keys off `Commit::lists_mask`, which is the distinction the D35 sort gap turned on. `test_store_in_place_sort` |
| P5 | `mat2list` may not compose or be stored (*"mat2list must stand alone"*) | It writes list slots the operand stack may still hold **by reference**: `l1 * mat2list([A], l1)` would read l1's *new* contents. Found by 5.2.8's static-buffer audit; `matexpr` had the rule for the same reason. `test_matrix_bridge` |
| P6 | A returned list `Value` is valid only until the next `run()` | `listexpr::Result::list` has had this contract since Phase 3A. A stored or sorted result names its slot instead, and a matrix result names MatAns. `test_store_targets` |
| P7 | `evaluate_real()` / tinyexpr is untouched on the graphing, table and stats path | `phase4-spec.md` §5.2's performance guardrail. The unified evaluator is strictly home-screen-only, exactly as `evaluate_complex()` is today. |

### Deferred

| # | Owed | To |
|---|---|---|
| D1 | `mat2list`'s *"Done (n lists)"* display string. The evaluator returns the count as the value and reports the refs in `Commit::lists_mask`; formatting is the dispatcher's. | 5.2.10 |
| D2 | The `num⇒a` store echo, and exact-form display interaction with a store | 5.2.10 |
| D3 | Whether a non-finite result should report *"Undefined result"* uniformly across all four kinds (see W14) | 5.2.10 |
| D4 | REAL-mode retry / probe sequencing on the home screen, now that one evaluator can answer both questions in one run | 5.2.10 |
| D5 | The differential allow-list wiring: this register, machine-read | 5.2.9 |

---

## 3. Coverage — what is NOT in this register

**As of 2026-08-09 the differential harness reports 494 of 518 comparisons in
exact agreement, with all 24 divergences carrying a row above.** That number is
what makes the rest of this section a claim rather than a hope.

Stated so an empty region reads as "checked" rather than "not looked at":

- **Arithmetic, precedence and associativity.** Unchanged by construction: the
  compiler's precedence table is written against `mat_expr.cpp:812-865` and
  `complex_expr.cpp:366-404`, and `^` stays right-associative because tinyexpr
  is built with `TE_POW_FROM_RIGHT`. Pinned by `test_compile_associativity` and
  `test_vm_matches_real_path`.
- **The catalogue's 82 functions, tinyexpr's 24 builtins and the complex-only
  set.** Same names, same arities, same values — three tables reached natively
  instead of through `eval_field`. Pinned by `test_builtins`.
- **Number and angle modes**, beyond P1–P3. The whole corpus runs in both REAL
  and RECT, in DEGREES — D46's territory — and RECT produced **no divergence
  the register did not already carry**, which is the strongest single statement
  in this file: the complex tier and `complexexpr` agree across every expression
  the complex suite contains.
- **Postfix factorial.** Absent from the compiler until 5.2.9's first run found
  it (`5!` is shipped syntax that both retired scalar paths reached by rewriting
  the input before parsing, so no grammar rule existed to port). Now a postfix
  operator, pinned by `test_factorial`. Not a register row: it never shipped
  broken, it was a gap in the branch.
- **Everything off the home screen.** Graphing, tables, stats, the solver, the
  CAS and the editors do not go through this evaluator (P7).

## 4. Replaying the register on hardware

The `Input` column is the script. With serial injection (Phase 5.1) and two
firmware images — the **v0.3.1 release `.uf2`** for the old pipeline (tagged
2026-08-08, all of Phase 5.1 and none of 5.2, and its injection block is
byte-identical to today's) and a current build for the unified evaluator — each
row is one line in, one `inject:` echo out, on both:

```
inject: "<expr>" -> "<result>" kind=plain|symbolic|error
```

A row passes when the pair of echoes matches its `Today` and `Unified` columns.
Rows whose inputs need seeded state (`l1` complex, `[A]` complex, a 999-element
list) are set up by the lines preceding them in the script, so the script is
ordered, not a set.

This is the same mechanism as the differential pass in
[phase5.2-spec.md](../phases/phase5.2-spec.md) §6.1 and it runs in the same
session as the timing pass in §9 — one script, three outputs: results, stack
peaks, elapsed times.
