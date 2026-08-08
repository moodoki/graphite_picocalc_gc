# Risch algorithm — reading list

Reference material for anyone who wants to understand *why* Phase 5's
symbolic integrator stops where it does, or who is tempted to push it
further.

## Why this is here

[phase5-spec.md](../phases/phase5-spec.md) §13 lists "general-purpose
(Risch-class) symbolic integration" as a non-goal, and §9 calls it
"infeasible on this hardware". **That second phrasing is misleading and worth
correcting**: the real obstacle is not memory, flash or CPU. Risch is a
differential-algebra research topic — reasoning about towers of field
extensions and square-free factorization over those towers — before it ever
reaches arithmetic. The barrier is implementation effort and mathematical
depth, not the RP2040.

The evidence that hardware is not the binding constraint: DB48X fits a full
symbolic engine, an RPL runtime and an equation library into **700 KB** on
hardware with *less* RAM than the Pico 1 (128 KB vs. 264 KB). And no
calculator CAS attempts full Risch — not TI-Nspire, not the HP-50G. That
boundary is common across the entire calculator-CAS space, not something
specific to this project.

What genuinely shapes Phase 5 on this hardware, in descending order of how
much it actually matters:

- **`Expr::NUM` holds a `calc_t` (double).** A Sympy/Maxima-class CAS keeps
  arbitrary-precision integers and rationals so exactness never degrades.
  The fixed 32-byte node with a union has no bignum arm, and adding one means
  variable-length node payloads — a redesign of the pool allocator, not "buy
  more RAM". **This is the one genuinely architectural limit**; see
  [ti-parity.md](../notes/ti-parity.md) §8.
- No FPU on the Pico 1, so softfloat puts worst-case CAS operations around
  ~0.5 s (spec §12) rather than milliseconds. A latency cost, not a
  feasibility cutoff.
- Pool placement: the `ExprPool` overlays the shared scratch arena rather
  than reserving new SRAM (D41), which bounds a single operation to 22,528
  bytes. That is a placement decision, not a ceiling on what a CAS can do.

Everything else excluded from Phase 5 — systems of equations, limits, series,
partial and implicit derivatives, unit arithmetic — is **scope chosen
deliberately**, not a hardware verdict.

## Reading order, easiest to hardest

1. **Wikipedia — Risch algorithm**
   <https://en.wikipedia.org/wiki/Risch_algorithm>

   Start here: why integration of elementary functions is *decidable* at all,
   why differentiation is trivial and integration is not, and where the
   algorithm fails to terminate in practice.

2. **Bronstein — Symbolic Integration Tutorial** (ISSAC'98 course notes)
   <https://www-sop.inria.fr/cafe/Manuel.Bronstein/publications/issac98.pdf>

   The actual on-ramp. Written as course notes, meant to be read start to
   finish rather than referenced.

3. **Bronstein — *Symbolic Integration I: Transcendental Functions***
   (book, 2nd ed., Springer)

   The reference textbook. **Chapter 2 alone is worth reading in isolation**
   — rational function integration via Hermite reduction plus
   Rothstein-Trager / Lazard-Rioboo-Trager. It is the fully-worked, tractable
   slice, before differential fields and Liouville's theorem for the
   transcendental case.

4. **MathWorld — Risch Algorithm**
   <https://mathworld.wolfram.com/RischAlgorithm.html>

   Quick-reference summary, useful once the shape is already clear from (1)
   and (2).

5. **SymPy's Risch implementation** (source — browse the repo)

   Reading real code alongside Bronstein's pseudocode is the recommended way
   to actually absorb it. It is the only widely-used open-source
   implementation that is genuinely complete rather than a heuristic subset.
   Discussion thread: <https://groups.google.com/g/sympy/c/bYHtVOmKEFs>

## Past the base algorithm

Generalization of Risch's algorithm to special functions (erf, Ei, …):

- Springer: <https://link.springer.com/chapter/10.1007/978-3-7091-1616-6_12>
- arXiv preprint: <https://arxiv.org/pdf/1305.1481>

## What Phase 5 does instead

The shipped integrator (`src/math/cas/integrate.cpp`, task 4D.16-4D.19) is a
table lookup plus linearity, linear substitution, a power-rule
generalization, and one level of integration by parts, with a numeric
fallback for definite integrals it cannot do symbolically. That covers the
overwhelming majority of high-school and early-college calculus, which is the
stated target — see [ti-parity.md](../notes/ti-parity.md) §8 for the full
comparison against TI-Nspire CX II CAS.
