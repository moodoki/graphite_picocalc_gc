# Design departures — first-class matrices, vectors, and complex numbers

**Date**: 2026-07-21
**Status**: Ideas for consideration, **not decisions**. Nothing here is
committed; each idea needs its own scoping pass (and its own D-numbered
decision) whenever a phase actually picks it up. Written alongside the
[TI parity stocktake](ti-parity-2026-07-21.md) and feeding the
[Phase 5 CAS spec draft](../phases/phase5-spec.md).

## Why depart from TI at all

TI's model treats matrices, complex numbers, and lists as three walled-off
data types with their own modes and their own rules about where they're
allowed to appear. A matrix can't hold a complex number. A complex result
can't go in a list. `A` the real variable and `[A]` the matrix are
different namespaces by construction. This is a reasonable design for a
1990s calculator ASIC, but it's not a law of nature — it's a consequence of
TI-BASIC's type system being as simple as possible. We're not bound by
that constraint, and the parity stocktake already surfaced two places
where our own as-built decisions (D28, D30) landed on the TI-shaped wall
by default rather than by an actual seamlessness argument:

- **D28** (matrices): *"No matrix literals on the home screen (editor is
  the entry path)"* — a tradeoff accepted to ship 4A, not a design goal.
- **D30** (complex): *"Complex results can't be stored"* —
  `Variables::vars` stays `calc_t`-only, explicitly flagged as "out of
  scope" for 4C, not as "shouldn't happen."

Both were the right call for shipping incrementally. Whether to now spend
effort closing them is exactly the question this doc is for.

## The current shape: three parallel mini-evaluators

Worth naming plainly, because it's the thing that makes "seamless"
non-trivial rather than a display tweak:

- `math::Engine` (tinyexpr++) — the real-only hot path. Untouchable:
  graphing, tables, and stats all depend on it staying fast (`calc_t`
  in, `calc_t` out, no allocation).
- `math::matexpr` (D28) — a hand-written recursive-descent evaluator over
  a tagged scalar/matrix `Value`, invoked when a `[X]` token or
  `identity(` appears.
- `math::complexexpr` (D30) — a second hand-written recursive-descent
  evaluator over `Complex`, invoked when `i` appears or number mode is
  non-REAL. D30's own notes call this out: it *"structurally mirrors
  matexpr... but simpler — no tagged scalar/matrix Value, everything is
  Complex."*
- `math::listexpr` — a third evaluator for list-valued (`l1`..`l6`)
  expressions.

Each one falls back to `eval_field` (the real engine) for the "everything
else" case — constants, the general function catalog — via the
scalar-subterm technique D30 borrowed from D28. That reuse is good; what's
missing is reuse *between* matexpr, complexexpr, and listexpr themselves.
Right now none of them know about each other: a matrix can't contain `i`,
a complex expression can't reference `[A]`, a list reduction can't hold a
complex element. Every pairwise combination that should "just work" is
presently a wall.

## Departure ideas, cheapest to most ambitious

### A. Home-screen matrix literals

`[[1,2][3,4]]` typed directly, not just built in the matrix editor.
Closes the D28 gap. Mechanically small — mostly `matexpr` parser work to
recognize a bracketed literal and materialize it into a scratch `Array`
(likely `MatAns` or a new literal slot). Lowest risk of anything here;
doesn't touch storage layout or the ArrayStore pools.

### B. Complex-valued variables and `Ans`

Widen `Variables::vars` (and the `Ans` cache) from `calc_t` to a tagged
scalar that can hold either a real or a `Complex`. This is the direct fix
for the D30 gap (`2i->a` currently errors). Touches:

- `Variables::vars` storage and every real-engine call site that assumes
  `calc_t` (D19's case-sensitivity and reservation rules for `i` already
  set the precedent for "some identifiers are special").
- The store-target validation shared across `Engine::evaluate`,
  `matexpr::parse_store_target`, `listexpr`'s `seq` var, and
  `solve_expr`'s solve var (D30 §2) — all currently reject `i` as a
  target; a complex-capable `Variables` doesn't change that, but the
  *value* stored for `a`, `b`, ... would need a tag.
- Downstream: anywhere a variable is read back into `eval_field`
  (matexpr scalar subterms, listexpr) would need to either reject a
  complex-valued variable in a real-only context (mirroring REAL mode's
  domain-error behavior) or promote silently — same design question 4C
  already answered once for expressions, now needing an answer for
  storage.

Moderate risk: it's a real widening of the variable-storage type, not just
a new evaluator branch, but it doesn't touch the graphing hot path (D30's
own dual-path precedent — `evaluate_real()` vs. `evaluate_complex()` —
generalizes directly: variable *storage* can be tagged without variable
*reads on the graphing path* ever seeing anything but `calc_t`).

### C. Complex-valued lists

Same idea as B, applied to `l1`..`l6`. Bigger: `Array`'s element type is
presently a flat `calc_t` buffer sized against the `ArrayStore` slab pools
(D28 grew those SRAM slabs to 28 to fit matrices; Pico 1 bss is already
the tightest resource in the project — ~188 KB of 264 KB per D28–D31, a
recurring watch item in every recent session). A complex element is two
`calc_t`s: either double the per-element footprint for *all* lists
(wasteful — most lists are real), or add a parallel complex-tagged Array
variant (more code, memory allocated only where used). The second is
probably right but is real design work, not a mechanical extension.
**This is the idea most likely to actually pinch Pico 1 headroom** — flag
for a feasibility check before committing, not just a design doc.

### D. Complex-valued matrices

Needed for two things the parity doc already flags as gaps vs. Nspire
CAS: complex eigenvectors (we already return a complex eigen*value*
spectrum via `Kind::kText`, D30 §7) and complex linear-system solving.
Mechanically similar to C (element-type widening) but on `Matrix`/`Array`
in its matrix role rather than its list role — the two might even share
whatever "tagged Array" solution C lands on, since `Matrix` is already
"a linear-algebra view over `Array`" (phase4-spec §3.1 reconciliation).
Worth designing C and D together rather than sequentially, given they'd
likely share a storage answer.

### E. Vectors as first-class, not just narrow matrices

TI's model: a vector is just a $1\times n$ or $n\times 1$ matrix, with no
`dot`/`cross`/`norm` built in anywhere on 83/84+ (that needs a program). Two
independent moves here, either alone or together:

- **Vector operations**: add `dot(v1, v2)`, `cross(v1, v2)` (3-space),
  `norm(v)` to the matrix function set — cheap, no storage change, just
  new `matexpr` functions operating on $1\times n$/$n\times 1$ `Array`s.
- **List↔matrix bridge**: the parity doc flags TI's `List►matr`/
  `Matr►list` as a gap. A list is already conceptually a vector; letting
  `l1` participate directly in matrix expressions (as an implicit
  $n\times 1$ column) would remove the current hard wall between `listexpr` and
  `matexpr` for exactly the case where it's most natural — reusing
  collected data as a vector rather than manually re-entering it in the
  matrix editor.

Lower risk than C/D — this is mostly new functions and one conversion
path, not a storage-model change.

### F. The bigger structural move: unify the three mini-evaluators

Everything above is a bilateral patch (complex+variables, complex+lists,
complex+matrices, list+matrix). The pattern suggests the actual seamless
answer is architectural: replace `matexpr`, `complexexpr`, and `listexpr`
with **one recursive-descent evaluator over a single tagged `Value`**
(`{kind: real | complex | matrix | list, ...}`), the way `matexpr`
already tags scalar-vs-matrix and `complexexpr` already "structurally
mirrors" it (D30's own words). A matrix of complex numbers, a complex
scalar times a matrix, a list containing complex elements, a dot product
between two list-vectors — all of these stop being special cases and
become the same generic binary-op dispatch on a bigger tag enum.

This is the highest-leverage idea and also the highest-risk one:

- It's a genuine rewrite of three working, tested evaluators (D28: 199
  matrix + 27 solver checks; D30's complex suite; the list suite) into
  one, with real regression risk against ~1200 host-test checks that
  currently pin their separate behaviors.
- The performance guardrail 4C already established — *"making the
  default numeric path complex would double arithmetic cost on the hot
  graphing loop... keep two evaluation entry points"* (phase4-spec §5.2)
  — generalizes: a unified tagged-`Value` evaluator must stay strictly
  home-screen-only, exactly like `evaluate_complex()` today.
  `evaluate_real()` (tinyexpr++, graphing/tables/stats) is never touched.
- Memory: a tagged union big enough for `{real, complex, matrix-ref,
  list-ref}` is larger per node than any of today's three narrower
  evaluators' working types — needs a real sizing pass against Pico 1's
  headroom before committing, same caution as C.

**Not recommending this now.** It's the right long-term shape *if* B–E
keep getting requested piecemeal and the bilateral-patch pattern starts
showing its seams (e.g., needing both "complex variable" and "complex
list element" and "matrix of complex" all at once starts duplicating
promotion logic three times). Worth revisiting as a refactor once/if two
or more of B–E actually ship and the duplication becomes visible in the
code, rather than speculatively building it now.

## Suggested sequencing (not a commitment)

If any of this gets pulled into a phase:

1. **A (home-screen matrix literals)** is a clean, low-risk pickup
   whenever 4A/matrix work is revisited — closes a real usability gap
   with no storage-model risk.
2. **B (complex variables/Ans)** is the most-requested-shaped gap (it's
   the one wishlist/D30 already calls out as unmet scope) and is
   self-contained enough to scope on its own.
3. **C and D (complex lists, complex matrices)** should be scoped
   *together* — they likely share a storage answer — and need a Pico 1
   memory feasibility check before any implementation commitment, given
   how tight bss headroom already is.
4. **E (vector ops + list↔matrix bridge)** is independent of the above
   and cheap; could land whenever, including opportunistically inside
   another matrix-touching session.
5. **F (unified evaluator)** — park it. Revisit only if B–E collectively
   start making the three-evaluator duplication a real maintenance cost.

## Explicit non-goals

- Matching Nspire's full generality (arbitrary-rank tensors, symbolic
  matrices with unknowns, systems of complex linear equations solved
  symbolically) — out of scope for this doc and, per the parity stocktake
  §8, out of scope for Phase 5 CAS as currently drafted too.
- Changing `evaluate_real()`'s type or behavior in any of these ideas —
  every option above is additive to the home-screen/editor paths only.
