# Design departures — first-class matrices, vectors, and complex numbers

**Date**: 2026-07-21 (idea H added 2026-07-24)
**Status**: **A-G closed 2026-07-24.** Every idea A-G now has a decision
and, except F, a task ID in `phase4-spec.md`'s 4D task table — see
D32/D33 (A/B's original pickup), D36 (E, G), and D37 (C, D, F's
sequencing) in `decisions.md`. **Idea H is new and *not* decided** — raised
the same day as a follow-on question ("how much work to make variables
fully polymorphic, MATLAB-style?") after A-G closed. It's scoped below but
deliberately left as an open idea, not folded into 4D or given a D-number,
pending its own go/no-go. Written alongside the
[TI parity stocktake](ti-parity.md) and feeding the
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

**Status (2026-07-24): go, picked up into Phase 4D as 4D.24**
(see `decisions.md` D37, `phase4-spec.md` §7.3/§8). Resolved the
feasibility flag below by routing complex-valued lists **exclusively
through the PSRAM region tier** — never the fixed 28-slab SRAM pool (56 KB
of bss, ~67 KB headroom left as of D35) — so bss doesn't grow at all,
trading away the SRAM fast path even small real lists get. v1 scope:
storage, display, elementwise ops, `sum`/`mean`; `stdev`/regression/`sort`
error on complex input (deferred, not silently promoted).

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

**Status (2026-07-24): go, picked up into Phase 4D as 4D.25** (see
`decisions.md` D37, `phase4-spec.md` §7.3/§8), reusing 4D.24's PSRAM-only
storage answer. v1 goes all the way to full complex linear algebra
(det/inverse/rref/ref/rank/solve, complex Gauss-Jordan with
magnitude-based pivoting) — the more ambitious of two scoping options
considered. Explicitly excludes eigenvalues/eigenvectors *of* a
complex-valued matrix (a complex Hessenberg+QR core — a much bigger
algorithmic lift than generalizing arithmetic to `Complex`, and distinct
from 4C's existing complex-eigenvalues-*from-a-real-matrix* feature,
D30) — that stays a future item if ever wanted.

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

**Status (2026-07-24): vector-ops half picked up into Phase 4D as 4D.22**
(see `decisions.md` D36 and `phase4-spec.md` §7.3/§8) — raised again in a
soak-feedback session as "lists and matrices being walled off feels
clunky." The list↔matrix bridge half already shipped as 4D.12.

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

**Status (2026-07-24): committed as a real follow-on after 4D ships, not
indefinite parking** (see `decisions.md` D37). With B/C/D/E/G all landing
in 4D, this idea's own trigger condition below is expected to fire once
4D closes — treated as the de facto next architecture pass, though not
yet given its own phase/week slot the way A-E/G now have.

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

Historical note (pre-2026-07-24 framing): "not recommending this now... worth
revisiting as a refactor once/if two or more of B–E actually ship and the
duplication becomes visible in the code, rather than speculatively
building it now." That trigger condition is exactly what's expected to
fire once 4D ships (all of B–E and G are now in 4D's scope) — see the
Status line above and D37: F moves from "maybe, watch for a trigger" to
"planned, timing tied to that trigger," a real commitment rather than an
open-ended one.

### G. Matrix eigenvectors (new, 2026-07-24 — not part of the original A-F list)

4A/4C ship eigen*values* only (`eigenvalues()`, real; `eigenvalues_complex()`,
full spectrum via `Kind::kText`) — there's no way to recover the
corresponding eigenvectors. Raised in the same soak-feedback session as E.
Bigger than E: needs its own numeric method (nullspace of `A - λI` via the
existing `rref`, or inverse iteration) rather than a thin wrapper, and has
its own open question about repeated/defective eigenvalues (P4-13,
`phase4-spec.md` §11). Picked up into Phase 4D as **4D.23**, real
eigenvalues only for v1 — complex eigenvectors explicitly deferred, mirroring
the real-only precedent D28 set for eigenvalues before D30 added the
complex spectrum.

### H. Polymorphic variables — MATLAB-style unified namespace (new, 2026-07-24, NOT DECIDED)

Raised as a follow-on question right after A-G closed: what would it take
to let *any* variable (`A`-`Z`) hold a real, a complex, a list, *or* a
matrix — one polymorphic slot, the way MATLAB variables work — rather than
TI's three separate namespaces (`A`-`Z` scalars, `[A]`-`[J]` matrices,
`l1`-`l6` lists) that even C/D/F (above) leave intact?

**This is bigger than F, not a bigger version of it.** F unifies the three
*evaluators* but keeps the three *namespaces* — `[A]`, `l1`, and `A` still
route by token shape (`evaluate_input`'s matexpr → listexpr →
scalar/complex cascade, `src/apps/home_screen.cpp:179-260`, gated purely
on syntax before anything is evaluated). Idea H collapses the namespaces
themselves, which changes the dispatch model, not just its
implementation: `a * b`'s meaning would depend on `a`/`b`'s *runtime*
stored type, resolved after a variable lookup, not on how they're spelled.

What it actually touches, concretely, against the current code:

- **Storage capacity**: `Variables::vars` is a flat `calc_t vars[28]`
  today (`src/math/engine.hpp:10-21`) — 28 scalar slots, no Array
  ownership at all. `MatrixStore` (10 slots) and `ListStore` (6 slots)
  are separate, `ArrayStore`-backed stores. Letting any of `A`-`Z` hold a
  list or matrix means a variable can *own* an Array slot — worst case,
  up to 26 array-capable slots simultaneously instead of today's fixed 16
  (10+6). That's a real capacity replan against the 28-slab, 56 KB SRAM
  pool (~67 KB headroom as of D35) — bigger in kind than C/D's question,
  which only widened the *element type* of already-bounded slots, not
  the *number* of slots that can hold array data.
- **Dispatch rewrite**: not an extension of the existing token-shape
  cascade — a different model. Every operator needs to become
  type-polymorphic at runtime (`real×real`, `real×complex`,
  `complex×matrix`, `matrix×matrix`, `list×list` elementwise, and the
  invalid combinations
  that must error cleanly) instead of routing by which mini-evaluator's
  trigger token appeared.
- **Hot-path guardrail**: `evaluate_real()` (graphing/tables/stats) must
  stay `calc_t`-only and fast — today that guardrail only has to hold for
  *complex* (4C's dual-path). A fully polymorphic `a` means every
  real-only variable read anywhere needs a "is this actually real right
  now" check, not just the complex-tagged ones P4-11 already covers.
- **Persistence**: `save_variables()`, `ListStore::save`, `MatrixStore::save`
  are three separate formats (D35 just finished splitting them further,
  into per-item files, magics PCL2/PCM2). Unifying storage almost
  certainly means a fourth format change, unifying persistence too.
- **UI**: `matrix_editor.cpp`/`list_editor.cpp` are separate screens keyed
  to separate stores — would need to become "edit variable `a` as
  whatever type it currently holds," or stay separate views onto the
  same unified storage.
- **Backward compatibility, a product decision not an engineering one**:
  D28 deliberately chose TI-style `[A]`-`[J]` bracket syntax for muscle
  memory (a user pick, not a default). Does `[A]` collapse into being
  the same variable as `A` (so `5 -> A` then `A` holds a matrix errors
  or silently becomes typed?), or does it stay a distinct legacy alias
  layered on top of the new unified store? Needs an actual decision
  before implementation, not just a design.

**Rough size**: comparable to or larger than F alone, which was already
the highest-risk idea in this doc ("genuine rewrite of three working,
tested evaluators... real regression risk against ~1200 host-test
checks") — H adds a storage/namespace collapse, a capacity replan, a
persistence format change, and an unresolved TI-syntax-compat product
question on top of F's own rewrite. Ballpark: **100+ hours**, likely
comparable to or bigger than 4A+4C combined (~84 hrs) — not a tight
estimate, and deliberately not one, the same way C/D's algorithm choice
was left as an open question (P4-13) rather than guessed at. A real
number needs its own design pass first: pick the tagged-`Value`
representation, decide the capacity/eviction policy for array-backed
variables, decide the `[A]`/`A` namespace-collapse question, and size
persistence — before any hour estimate is more than a guess.

**Not scoped into a phase. Revisit after 4D ships** (decided 2026-07-24,
same session H was raised in) — sits after F in any plausible sequencing,
so the natural checkpoint is the same one F's own commitment is tied to.

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
   how tight bss headroom already is. **Done (2026-07-24): both go, as
   4D.24/4D.25 — feasibility resolved by routing complex arrays
   exclusively through the PSRAM tier, no SRAM growth. See D37.**
4. **E (vector ops + list↔matrix bridge)** is independent of the above
   and cheap; could land whenever, including opportunistically inside
   another matrix-touching session. **Done (2026-07-24): picked up into
   4D as 4D.12 (bridge, already shipped) and 4D.22 (vector ops).**
5. **G (eigenvectors)** — not in the original sequencing; picked up
   alongside E on the same 2026-07-24 pass, as 4D.23. Real-only for v1.
6. **F (unified evaluator)** — park it. Revisit only if B–E(–G)
   collectively start making the three-evaluator duplication a real
   maintenance cost. **Updated 2026-07-24 (D37): committed as a real
   follow-on once 4D ships** (its own trigger condition above is
   expected to fire then) — no longer open-ended parking, but still no
   phase/week slot.
7. **H (polymorphic variables, MATLAB-style)** — new 2026-07-24, **not
   decided, not scoped into a phase. Revisit after 4D ships** (same
   checkpoint as F). Sits after F in any plausible sequencing; needs its
   own go/no-go and a real design pass (tagged-`Value` representation,
   array-backed-variable capacity policy, the `[A]`/`A` namespace
   question, persistence) before an hour estimate is more than a rough
   100+ hr ballpark.

## Explicit non-goals

- Matching Nspire's full generality (arbitrary-rank tensors, symbolic
  matrices with unknowns, systems of complex linear equations solved
  symbolically) — out of scope for this doc and, per the parity stocktake
  §8, out of scope for Phase 5 CAS as currently drafted too.
- Changing `evaluate_real()`'s type or behavior in any of these ideas —
  every option above is additive to the home-screen/editor paths only.
