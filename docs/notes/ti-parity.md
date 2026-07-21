# Feature parity stocktake — TI-83/84+ (and TI-Nspire CAS for CAS)

**Last updated**: 2026-07-21 (originally written this date; kept current going
forward — see "Maintenance" below)
**Purpose**: a living assessment of how PicoCalc GraphCalc compares to its two
reference machines — the TI-83/84+ family for the general calculator, and the
TI-Nspire CX II CAS for the not-yet-built CAS phase. Used to inform phase
scoping (originally the Phase 5 CAS spec draft and Phase 6 app-framework/polish
scoping) and to give an honest "how far along are we" read at any point in the
project. Not a spec, not a decision log — see
[design-departures-matrix-complex.md](design-departures-matrix-complex.md) for
where we intend to diverge on purpose, and [decisions.md](decisions.md) for
anything actually decided.

**Maintenance**: updated in place (no dated filename/copies) at the end of
each phase, when a phase's feature set changes what's shipped vs. planned.
Bump "Last updated" when edited.

Legend: ✅ shipped/on par · 🟡 partial or different shape · ❌ not present ·
🚫 out of scope (deliberately not pursuing)

---

## 1. Core arithmetic & calculator basics

| Feature | TI-83/84+ | PicoCalc GraphCalc | Status |
|---|---|---|---|
| Natural math display (stacked fractions, exponents, roots) | ✅ (MathPrint, 84+ only — 83+ is line-mode) | ✅ (Phase 1, `render::LayoutNode` tree) | ✅ |
| Variables A–Z, $\theta$ | ✅ | ✅ (+ `Ans`, case-sensitive `i` reservation) | ✅ |
| `Ans` recall, chained entry | ✅ | ✅ | ✅ |
| Store operator (`STO→`) | ✅ | ✅ (`->`, rendered as `⇒`) | ✅ |
| Entry history / recall | ✅ (2nd-ENTRY, single-step back) | ✅ (full UP/DOWN shell-style history + scroll) | ✅ (deeper than TI) |
| DEG/RAD angle modes | ✅ | ✅ | ✅ |
| FLOAT/FIX/SCI display modes | ✅ (+ ENG on 84+) | ✅ FLOAT/FIX/SCI | 🟡 (no ENG notation) |
| Scientific/engineering constants (c, h, N_A, …) | ✅ (84+ CE has a constants menu) | ❌ | 🟡 planned: Phase 4D (task 4D.17) |
| Unit conversions | ✅ (84+ CE) | ❌ | 🟡 planned: Phase 4D (task 4D.18) |
| Fraction display / exact fraction answers | ✅ (84+ `Frac`) | ❌ | 🟡 planned: Phase 4D `▶Frac` (task 4D.2, bounded continued-fraction — not CAS); surd/exact-value display planned separately, Phase 5 §10.1 (see §8 below) |
| Catalog / function browser | ✅ (alphabetic CATALOG) | ✅ (`F5` HELP: catalog + key reference + syntax notes, richer than TI's plain list) | ✅ |
| On-calculator help/manual | ❌ (paper/PDF guidebook only) | ✅ (built-in HELP screen) | ✅ (departure — see below) |

## 2. Graphing

| Feature | TI-83/84+ | PicoCalc GraphCalc | Status |
|---|---|---|---|
| Function graphing (Y1–Y10) | ✅ | ✅ (Y1–Y7) | 🟡 (fewer slots, seldom binding) |
| Parametric graphing | ✅ | ✅ | ✅ |
| Polar graphing | ✅ | ✅ | ✅ |
| Sequence graphing (`u`, `v`, `w`) | ✅ | ❌ | 🟡 planned: Phase 4D (tasks 4D.6–4D.8) |
| Multi-function color plots | 🟡 (84+ CE: color; 83+/84+: mono) | ✅ (color on a $320\times320$ IPS panel) | ✅ (ahead — see below) |
| Discontinuity handling | ✅ | ✅ | ✅ |
| Trace | ✅ | ✅ (mode-aware readout: x/y, t, $\theta$) | ✅ |
| Zoom (in/out/standard/trig/box) | ✅ (ZBox, ZDecimal, ZSquare, …) | 🟡 (in/out, standard, trig — no ZBox/ZDecimal/ZSquare) | 🟡 |
| Value table | ✅ | ✅ (auto + ask, mode-aware columns, horizontal scroll) | ✅ |
| Split screen (graph+table) | ✅ (G-T mode) | ✅ | ✅ |
| Graph-screen CALC menu (value, zero, min/max, intersect, dy/dx, $\int$) | ✅ | ✅ (Phase 4B, D29) | ✅ |
| Shading (`Shade`, inequality shading) | ✅ | 🟡 (fnInt area shading only, function mode) | 🟡 |
| Stat plots overlaid on graph | ✅ | ✅ (Phase 3D) | ✅ |

## 3. Statistics

| Feature | TI-83/84+ | PicoCalc GraphCalc | Status |
|---|---|---|---|
| Data lists | ✅ (L1–L6, up to 20 named lists) | ✅ (L1–L6 fixed) | 🟡 (no named/user lists — wishlist) |
| 1-Var / 2-Var stats | ✅ | ✅ | ✅ |
| Regression models | ✅ (10 models) | ✅ (10 models, matches TI's set) | ✅ |
| Probability distributions (PDF/CDF/inv) | ✅ (normal, t, χ², F, binomial, Poisson, geometric) | ✅ (same seven) | ✅ |
| Inference (tests, CIs, ANOVA) | ✅ | ✅ (full suite per Phase 3D) | ✅ |
| Stat plots (histogram, box, scatter) | ✅ (+ modified box, xyLine, normal prob. plot) | ✅ histogram/box/scatter | 🟡 (missing xyLine, normal prob. plot) |
| List↔matrix conversion (`List►matr`, `Matr►list`) | ✅ | ❌ (no bridge yet) | 🟡 planned: Phase 4D (task 4D.12) |

## 4. Matrices

| Feature | TI-83/84+ | PicoCalc GraphCalc | Status |
|---|---|---|---|
| Named matrix variables | ✅ `[A]`–`[J]` | ✅ `[A]`–`[J]` (D28, TI syntax by design) | ✅ |
| Arithmetic (+, −, $\times$, scalar mul) | ✅ | ✅ | ✅ |
| Determinant, inverse, transpose | ✅ | ✅ | ✅ |
| `rref`/`ref`, rank | ✅ (`rref`; TI-84 has no built-in `rank`, Nspire does) | ✅ (both, plus `rank`) | ✅ (ahead of 84+) |
| Eigenvalues/eigenvectors | ❌ (not on 83/84+ at all — Nspire CAS only) | ✅ eigenvalues (real + complex spectrum, D28/D30) | ✅ (ahead of 84+, eigenvectors still missing vs. Nspire) |
| Matrix editor UI | ✅ | ✅ | ✅ |
| Home-screen matrix literals (`[[1,2][3,4]]`) | ✅ | ❌ (editor-only entry, D28 tradeoff) | 🟡 planned: Phase 4D (task 4D.14, departures idea A) |
| Complex-valued matrices | ❌ (83/84+ real-only; Nspire CAS: yes) | ❌ | ❌ → see departures doc |
| Augment / identity | ✅ | ✅ | ✅ |
| Numeric equation solver | ✅ (`Solver`, single equation) | ✅ (form screen + inline `solve()`, D28) | ✅ |

## 5. Complex numbers

| Feature | TI-83/84+ | PicoCalc GraphCalc | Status |
|---|---|---|---|
| `a+bi` / `re^(θi)` polar display modes | ✅ | ✅ (`a+bi` / `r∠θ`, D30) | ✅ |
| REAL mode domain errors instead of complex results | ✅ ("NONREAL ANS") | ✅ ("Non-real result") | ✅ |
| Complex arithmetic + elementary functions | ✅ | ✅ (sqrt/exp/ln/trig/inverse-trig/abs/arg/conj/real/imag) | ✅ |
| Complex values stored in variables | ✅ (any of A–Z can hold a complex scalar) | ❌ ("Complex results can't be stored" — D30 §5) | 🟡 planned: Phase 4D (task 4D.15, departures idea B) |
| Complex values in lists | 🟡 (TI-84+ allows it in practice, undocumented/limited) | ❌ | 🟡 → see departures doc |
| Complex matrix eigenvalues | ❌ (not on 83/84+) | ✅ (`Kind::kText` spectrum, D30) | ✅ (ahead) |
| `i` as a real reserved constant | ✅ | ✅ (case-sensitive, D30) | ✅ |

## 6. Programming

| Feature | TI-83/84+ | PicoCalc GraphCalc | Status |
|---|---|---|---|
| TI-BASIC program editor | ✅ | ❌ | ❌ (Phase 6/MicroPython, sub-phase 6B, not started) |
| Assembly/C "Apps" | ✅ (via App framework, needs a computer + TI-Connect) | 🚫 | 🚫 (no plan for native app sideloading) |
| On-calculator scripting | ❌ (only TI-BASIC, no general-purpose language) | 🟡 planned (MicroPython, full Python-family language) | 🟡 → ahead in ambition, not yet built |
| Calculator API from scripts (`calc` module) | 🚫 (TI-BASIC *is* the calc API) | 🟡 planned (Phase 6 §4.2) | 🟡 |

## 7. Storage, connectivity, hardware

| Feature | TI-83/84+ | PicoCalc GraphCalc | Status |
|---|---|---|---|
| Display | $96\times64$ mono (83+) / $320\times240$ 16-color (84+ CE) | $320\times320$ RGB565 IPS | ✅ (ahead of the whole 83/84 line) |
| Persistent storage | Flash (~24KB user RAM on 84+; CE has more) | 8 MB PSRAM + 32 GB SD card | ✅ (ahead by orders of magnitude) |
| Battery | AAA $\times4$ or rechargeable (CE) | LiPo, own charge/status system | ✅ roughly on par |
| Computer link / OS updates | ✅ (TI-Connect, USB) | 🚫 (USB is dev/serial only; firmware updates are BOOTSEL reflash) | 🚫 (different model — see below) |
| Keyboard | TI 83/84 fixed 2nd/alpha layout | 67-key QWERTY (STM32-scanned) | 🟡 different, not worse — QWERTY vs. TI's alpha-shift |

## 8. CAS — comparison against TI-Nspire CX II CAS

TI-83/84+ has **no CAS at all** (that's the TI-89/Nspire CAS tier), so the
right reference for symbolic math is the Nspire CAS. This section previews
where the not-yet-started Phase 5 (CAS, formerly 4D) will land relative to
that machine, based on the design already sketched in the old phase4-spec
§6 (now [phase5-spec.md](../phases/phase5-spec.md)).

| Feature | TI-Nspire CX II CAS | PicoCalc GraphCalc (Phase 5, planned) | Status |
|---|---|---|---|
| Symbolic simplify | ✅ (full CAS, Derive-derived engine) | 🟡 planned: rule-based rewriting to a fixed point, not a general normal form | 🟡 (scoped-down by design) |
| Expand / factor | ✅ (arbitrary degree, multi-variable) | 🟡 planned: expand any degree; factor via GCD/diff-of-squares/quadratic formula/rational-root theorem, degree $\leq 4$, single-variable | 🟡 (deliberately bounded — see phase5-spec §6.6 rationale) |
| Symbolic differentiation | ✅ (arbitrary, including partials, implicit) | 🟡 planned: single-variable, standard rule table incl. chain/product/quotient | 🟡 (no partials/implicit diff planned) |
| Symbolic integration | ✅ (Risch-derived, very capable) | 🟡 planned: table lookup + linearity + linear substitution + power-rule generalization + one-level IBP; falls back to numeric | 🟡 (deliberately bounded — Risch is out of scope, documented in phase5-spec §6.8) |
| Symbolic equation solving | ✅ (`solve`, `cSolve`, systems, arbitrary degree) | 🟡 planned: linear, quadratic (complex-aware), degree 3–4 via rational roots, standard inverse-function isolation, numeric fallback | 🟡 (no symbolic systems of equations planned) |
| Complex CAS (`cSolve`, complex simplify) | ✅ | 🟡 planned: `i` as a reserved symbolic constant with `i²=-1` rewrite rule, feeding the existing numeric `Complex` type (Phase 4C) | 🟡 (narrower — see design-departures doc for how far to push this) |
| Systems of equations / linear algebra CAS (`solve` on systems, symbolic `rref`) | ✅ | ❌ not planned | ❌ (would need multi-equation solve; not in phase5-spec) |
| Calculus: limits, series, sums (symbolic) | ✅ | ❌ not planned | ❌ |
| Exact-value display ($\sqrt{2}$ stays $\sqrt{2}$, not 1.414) | ✅ (native) | 🟡 planned: Phase 5 §10.1 (closed-form recognition on simplified results, home-screen only) | 🟡 (narrower — recognition-based, not full symbolic evaluation throughout) |
| Unit/dimensional arithmetic (`3 m/s` stays symbolic through arithmetic) | ✅ (native) | ❌ | ❌ (Phase 5 non-goal — materially bigger than exact-value display) |
| CAS/numeric mode toggle | ✅ (Nspire has an "Auto/Approximate" setting) | 🟡 planned: CAS is explicit-invocation only (menu or `diff`/`integ`/`solve`/`factor`/`expand`/`simplify` keywords) — never silently replaces the numeric evaluator | 🟡 (by design — the numeric fast path must stay untouched for graphing) |

**Read**: Phase 5 is scoped to be a **competent high-school/early-college
CAS** — on par with a scientific-calculator-class symbolic engine, not a
Nspire-class general-purpose computer algebra system. The phase4-spec's own
words on factoring apply project-wide: *"this handles the vast majority of
problems encountered in a high-school / early-college calculus context...
it will not factor quintics or higher-degree polynomials with irrational
roots — that requires Galois theory and is well beyond what even the HP-50G
or TI-Nspire CAS attempt."* Matching Nspire feature-for-feature (systems,
limits, series, unit/dimensional arithmetic) is explicitly not a goal for
Phase 5 as scoped; if wanted later it would need its own follow-up phase.
(Plain exact-*value* display — $\sqrt{2}$ staying $\sqrt{2}$ — is the one
exception: it's narrow enough to fold into Phase 5 core scope, §10.1.)

---

## Where we're already ahead of TI-83/84+

Worth naming explicitly, since a parity table reads as a deficit list
otherwise:

- **Display**: $320\times320$ color IPS vs. TI's $96\times64$ mono (83+) or
  $320\times240$ (84+ CE) — the panel alone outclasses the whole 83/84 line.
- **Storage**: 8 MB PSRAM + 32 GB SD vs. ~24 KB usable RAM on a stock 84+.
  Six lists could easily become sixty; the SD card is nearly free.
- **Built-in help browser** — TI ships a paper/PDF guidebook, nothing
  on-device; ours has a catalog, key reference, and syntax notes at `F5`.
- **Matrix rank and eigenvalues** — neither exists on 83/84+ at all (that's
  Nspire CAS territory); we have both, including a full complex spectrum.
- **Font system** — a build-time choice of five fonts with a shared
  math-glyph slot map (`π θ σ Σ μ λ ≠ √ ∠ ⇒`), vs. TI's fixed system font.
- **Entry history** — full scrollable shell-style history vs. TI's
  single-step 2nd-ENTRY recall.
- **Open, hackable firmware** — MIT-licensed application code (GPL only
  where vendored drivers require it), buildable from source; TI's firmware
  is closed.

## Where we're deliberately not chasing TI

- **TI-Connect / computer link ecosystem**: no OS-update-over-cable model —
  firmware updates are a BOOTSEL UF2 reflash. Different distribution model,
  not a gap to close.
- **Native App sideloading** (TI's on-device Apps like Cabri Jr., Periodic
  Table, Finance): MicroPython (Phase 6, sub-phase 6B) is the
  general-purpose answer instead of a bespoke app-store model.
- **Nspire-class CAS generality** (systems, limits, series, unit/
  dimensional arithmetic): explicitly out of Phase 5's scope — see §8
  above. (Sequence graphing, listed as unscoped in an earlier version of
  this doc, has since been picked up — Phase 4D, tasks 4D.6–4D.8.)

---

## Net read

Phases 1–3 are at or above TI-83/84+ parity in their respective areas
(scientific calculator, graphing, statistics) — often ahead on
storage/display/matrix-function breadth. Phase 4A–4C brings matrices,
interactive graph analysis, and complex numbers to parity or beyond. The
remaining gaps this stocktake found (sequence graphing, zoom/shading
breadth, list↔matrix bridge, scientific constants, unit conversions,
home-screen matrix literals, complex value storage, device power/settings
polish) are exactly what sub-phase **4D (GC completeness)** closes — see
[phase4-spec.md](../phases/phase4-spec.md) §7 and
[decisions.md](decisions.md) D33. Once 4D ships, **Phase 4's completion is
the project's pre-release milestone**: a feature-complete TI-83/84+-class
graphing calculator, independent of what comes next. What comes next is
where the *real* white space against the reference machines lives: **Phase
5 (CAS)** closes the 83/84+ → Nspire CAS gap (partially, by design — see
§8), and **Phase 6** (non-calculator functions, MicroPython as its first
app) leapfrogs TI-BASIC entirely rather than matching it.
