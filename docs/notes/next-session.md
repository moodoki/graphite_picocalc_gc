# Start here — next session

**Last session:** 2026-08-03 — **Stage 4 follow-ups, flashed to the
Pico 2.** Three gaps found on the first exact-form flash, decision
**D44**. (1) **Alt+Enter is the decimal escape** — with an expression
entered it evaluates with the probe suppressed (same as `>dec`); with the
input empty and the newest result being an exact form, it re-runs that
expression as a decimal, so an amber `sqrt(2)` becomes `1.414213562`
without retyping. **Was Shift+Enter, rebound after HW testing**: the diag
screen showed that chord arriving as key code 59 (`kInsert`), not 52
(`kEnter`) with `shift_held` — the STM32 *translates* Shift chords into
their own scan codes (Shift+Enter to 0xD1, same family as Shift+F1..F4 to
F6..F9) rather than reporting base-key + modifier, so a Shift binding
never fires. Now recorded in `platform/keyboard.hpp` beside the D12
arrow note. Alt passes its flag through intact (Alt+UP/DOWN already
scrolls history), and Insert stays free for a real binding. (2) **Exact trig at special
angles**: `sin(pi/3)` → `√3/2`, `tan(pi/6)` → `√3/3`, `cos(pi/3)` → `1/2`,
via a 24-entry table indexed in *twelfths of $\pi$* (covering the $\pi/6$
and $\pi/4$ families; `cos(x)=sin(x+pi/2)` is an index shift). **Angle-mode
aware** — in DEGREE mode `sin(60)` folds the same way. (3) **Non-REAL
number modes now get exact forms** for real-valued results (the D43 v1
limitation, which had no technical reason behind it) — the probe moved
into a shared `apply_exact_form` helper used by both dispatch branches.
Also: **"interesting" now compares formatted strings, not doubles**, so
`sin(pi)` shows `0` instead of `1.224646799e-16` and `cos(pi/2)` shows
`0` instead of `6.123233996e-17`, while `tan(pi/4)` (whose
`0.9999999999999999` already formats as `1`) stays out of the amber path.
Host suite green, `test_cas` **238 → 272**; Pico 1 bss **201,096, still
flat**; flash +5.3 KB; lint/format clean. **Flashed to the Pico 2, clean
boot confirmed over serial** — interactive confirmation still pending.
On-device HELP gained the Alt+Enter line and an `#EXACT FORMS` block.
Full detail: worklog's 2026-08-03 "Stage 4 follow-ups" entry,
`decisions.md` D44.

**Previous session:** 2026-08-03 — **Phase 5 Stage 4: exact-form (surd)
display, source changes, host-verified.** Home-screen results with a clean
closed form now show that form instead of a decimal — `sqrt(2)` → `√2`,
`sqrt(8)` → `2√2`, `1/sqrt(2)` → `√2/2`, `pi*2` → `2π`, `1/3` → `1/3`
(tasks 4D.23/4D.24, **D43**, which also resolves **P5-5 → always-on** and
**P5-6 → yes, `pi` included**). Recognition lives in a new
`src/math/cas/exact.cpp`, deliberately *not* in `simplify()` (which runs
inside integrate/solve/factor/derivative loops — a `POW(NUM,1/2)` rewrite
there is a §13 Risk 1 hazard for zero benefit); it works in
`POW(u,1/2)` space so the existing simplifier does the factor collection
free (`sqrt(2)*sqrt(2)`→2, `sqrt(2)+sqrt(8)`→`3√2`, `1/sqrt(2)` and
`sqrt(1/2)` share one rationalization path). The home-screen probe
mirrors D30: it runs *after* `engine().evaluate()` has committed
Ans/store and can only change the displayed string. **Five gates** bound
it — finite non-store result + no `>dec`; every literal in the parsed
input is an integer; no variables anywhere; a whitelist grammar
(rational coeffs + square-free `sqrt` + `pi`) that must be "interesting";
and agreement with the numeric result to 1e-9. Gate 2 is what makes
always-on safe (`2.5` stays `2.5`, not `5/2`; `0.1+0.2` stays `0.3`, not
`3/10`); gate 3 is not optional (the CAS parser has no `ans`/`e`, so
`ans` would parse as `a*n*s`). Layout builder gained a bare radicand
(`√2` not `√(2)`, except before `^`) and implicit multiplication before a
radical or symbol glyph (`2√2`, `2π`) — `is_call()` was relaxed to accept
the bare shape, with an explicit anti-regression test so `sqrt(2)/2`
still stacks as a fraction. **Behavior changes to judge on device**:
`1/3` now renders as an amber stacked fraction (was `0.3333333333`) and
`pi` renders as `π`; `>frac` results stay white flat text; no exact forms
for expressions naming a variable/`Ans`, or in non-REAL number modes (a
~6-line follow-up). Host suite green — `test_cas` **199 → 238**,
`test_layout` **44 → 54**, 0 failures, and the 199 pre-existing CAS
checks unchanged (the proof that staying out of `simplify.cpp` worked).
Both boards build clean; Pico 1 bss **201,096 bytes, exactly flat**;
lint/format clean. **Not flashed to either board yet** — folds into
Stage 5. Full detail: worklog's 2026-08-03 "Phase 5 Stage 4" entry,
`decisions.md` D43.

**Also 2026-08-03 (parallel, separate worktree):** a **documentation
branch `docs/site`** was seeded off `main` (commit `0f1e8ef`, pushed; no
PR). Plain-markdown source tree under `docs-site/` with `SUMMARY.md` as
the single nav source, driving three outputs: `scripts/gen-wiki.sh`
(flattened GitHub-wiki tree + `_Sidebar.md`), `scripts/gen-offline.sh`
(concatenated markdown always, plus self-contained HTML + PDF when
pandoc is present), and `scripts/gen-doc-reference.py` (generates the
function catalog from `src/math/catalog.cpp` and the key/syntax
references from `src/apps/help_screen.cpp` — firmware stays the source
of truth). CI: `validate-docs` now covers `docs-site/`, and a new
`.github/workflows/docs.yml` validates, fails on stale generated
reference pages, uploads the offline bundle, and has a wiki-publish job
gated on a `WIKI_TOKEN` secret (`GITHUB_TOKEN` cannot push to wikis —
the PAT setup is documented in `docs-site/README.md`). **Prose chapters
are stubs** — this was a scaffold-and-generators seed only.

**Two sessions ago:** 2026-08-03 — **Bugfix, source changes: home-screen
history persistence.** Root-caused and fixed the suspected home-screen I/O
persistence bug flagged at the end of the 2026-08-02 Stage 3 session:
symbolic CAS results were losing their `ResultKind` on reload (always came
back `kPlain` — plain white text instead of the typeset amber fraction),
because `history.txt` only stored `expr<TAB>result` and `load_state`
hardcoded `kPlain` for every reloaded line. Fixed by adding a third
tab-separated kind column (`expr<TAB>result<TAB>S|P\n`, backward
compatible with legacy two-field lines). While auditing the load/save path
also found and fixed two pre-existing latent bugs (predate Phase 5): a
head-vs-tail read bug (`load_state` read from file offset 0 despite its
own comment claiming "tail," so a `history.txt` past 8 KB restored the
*oldest* entries on reboot, not the newest — fixed with a new
`Storage::file_size()` + a seek to the true tail) and unbounded file
growth (no compaction ever existed — fixed with a new
`HomeScreen::compact_history()`, trims to the last 8 KB once the file
exceeds 24576 bytes). Both boards build clean; Pico 1 bss **201,096
bytes**, flat (shared `g_hist_io` buffer replaces the old function-local
static); `lint.sh`/`format.sh` clean; full host suite green (`test_cas`
199 unchanged — firmware-only path); a standalone host logic check of the
round-trip ran 600 checks, 0 failures. **D4 amended in place** (its own
"Revisit when" clause fired) rather than a new decision number. On-device
confirmation of history-survives-reboot is still open — folds into Stage
5's Pico 1/Pico 2 flashing. Full detail: worklog's 2026-08-03 entry,
`decisions.md` D4.

**Three sessions ago:** 2026-08-02 — **Phase 5 (CAS) Stages 0-3: engine +
home-screen UI integration, source changes, HW-verified on the Pico 2.**
On the `phase-5` branch (not yet merged to `main`). Two sessions: the CAS
engine itself — expr tree/pool, parser, serializer, simplify, differentiate,
expand, factor, solve, integrate (`src/math/cas/`, tasks 4D.1-4D.19,
D41: pool overlays the shared scratch `kCompute` arena, SRAM not the
spec's sketched PSRAM) — landed host-tested-only in an earlier session
this same day; this session wired it into the home screen (Stage 3,
4D.4/4D.20/4D.21): an inline-call router (`diff()`/`integ()`/`factor()`/
`expand()`/`simplify()`/`solve()`) dispatches from `HomeScreen::evaluate_input`
(CAS is display-only, no `Ans`/store, per P5-1/P5-2), results typeset via
`serialize` → `render::build_layout` in an accent color (**D42**: reuses
the existing string layout builder instead of a dedicated `expr_to_layout`
tree-walker), plus an F6 CAS menu and typed `cas` command
(`src/apps/cas_menu.{hpp,cpp}`). A round of on-device fixes followed:
exact `p/q` fraction display instead of decimal coefficients, right-aligned
symbolic results, amber accent (was teal — too close to the input-line
gray), descending-degree sum order (TI convention), and a pannable
one-line window for results too long to fit. `test_cas` grew 153 → **199**
checks, 0 failures; both boards build clean; Pico 1 static RAM **201,096
bytes** (~67 KB headroom, essentially flat — the CAS pool overlays the
existing arena); `lint.sh`/`format.sh` clean. Flashed to the Pico 2 and
confirmed working interactively (inline ops, F6 menu, fractions, sum
order, accent, scroll all reported "looks good") — **the Pico 1 leg for
this branch's CAS work is still open**, see "The next job" below.
Decisions **D41**, **D42**. `PICOCALC_PHASE` stays `"4D"` (bumping to `"5"`
is a Stage 5 close-out task, not yet reached). Full detail: worklog's
2026-08-02 "Phase 5 Stages 0-3" entry.

**Four sessions ago:** 2026-08-02 — **CI fix + first release, docs/infra only, no
source changes.** The GitHub Actions "Build" workflow had two red jobs (the
board build jobs themselves always passed): Lint disagreed with local
clang-format because CI installed Ubuntu's apt `clang-format 18` against
local's Homebrew `22` — fixed by pinning **`clang-format==22.1.8`** in
`requirements-dev.txt` and having CI `pip install` that exact version (no
source reformatting needed); Validate-docs failed on 6 loose `×` characters
in `docs/notes/pre-phase5-review.md` — replaced with ASCII `x`. Also bumped
all workflow actions to current majors (clears Node 20 deprecation
warnings) and added a `release` job that publishes both boards' UF2s to a
GitHub Release on `v*` tags. Landed via PR #1 (merge commit `e4b53ab`); CI
is now fully green on every job. **v0.1.0 published** — the project's first
tagged release:
<https://github.com/moodoki/graphite_picocalc_gc/releases/tag/v0.1.0>. No
decision number consumed, no phase/sub-phase status change (Phase 4D stays
closed, Phase 5 CAS is still next — see "The next job" below). Full detail:
worklog's 2026-08-02 "CI fix" entry.

**Five sessions ago:** 2026-08-02 — **Pre-Phase-5 review pass: shared scratch
arena (−21.8 KB SRAM) + near-zero matrix chop, HW-verified on the Pico 2.**
Opened the pre-Phase-5 code-review/size-optimization pass. A per-symbol SRAM
audit (new `scripts/size-report.sh`) found ~40 KB tied up in per-module
256-element PSRAM-streaming scratch buffers that are never simultaneously
live (single-threaded on core 0). Collapsed the verified-mutually-exclusive
ones onto one arena (`src/math/scratch.{hpp,cpp}`), two disjoint regions:
**kCompute** (list_expr | stats | infer | matops — none calls another) and
**kListops** (listops, disjoint because list_expr calls it); rebound by
reference-aliasing so call sites are unchanged (matops RowBufs via
placement-new). **Pico 1 bss 222,528 → 200,704 (−21,824 B; headroom ~46 →
~68 KB)**, Pico 2 same. During the device spot-check, `[A]^-1*[A]` showed FP
roundoff (2.22e-16) as scientific noise; added a relative near-zero chop to
`format_matrix` (cell >~12 orders below the max snaps to 0) — NOT an
arithmetic bug and NOT the arena (all matrix ops compute correctly
on-device). Also measured (no code change): **`-Os`** gives −126 KB flash but
0 SRAM (not a lever for this pass — keep `-O3`); **Phase 6 MicroPython budget
re-verified — the arena is what makes Phase 6 fit on Pico 1** (pre-arena 46.7
KB free < the 56 KB lazy heap; post-arena 68 KB → fits, ~12 KB spare). Host
1627 + test_matrix +6 (381) green; both boards clean; lint/format clean;
device-verified on Pico 2. **No decision number** (measurement/trim, not a
design call). Full detail: `docs/notes/pre-phase5-review.md`, worklog's
2026-08-02 "Pre-Phase-5 review pass" entry. Commits `1073f4f` (doc de-stale),
`5f76851` (arena), `4edba81` (chop).

**Six sessions ago:** 2026-08-02 — **UI-friction polish, source changes,
HW-verified on the Pico 2 (build `0cfbe05-dev`).** Fixed the two
UI-friction feature requests logged in the 2026-07-27 eval, plus two
follow-ups raised during this session's on-device testing. Matrix results
now format cells with the compact number formatter (`format_matrix`, 4 sig
figs; new `format_complex_compact` for complex cells) instead of full
10-digit precision. The constants picker was relaid out into four fixed
non-overlapping columns (symbol | engine id | short value | summary,
truncated with an ellipsis) to fix overprinting on long values like
`hbar`. Follow-up 1: `>Frac` now works on matrix results too (new
`math::matexpr::format_matrix_frac`, real cells become `p/q`). Follow-up
2: the constants-picker relayout needed LEFT/RIGHT description scroll
(`desc_scroll_`) to keep long summaries fully readable after truncation.
Host suite green (`test_math` 230, `test_matrix` 375, 0 failures across 12
suites); both boards build clean; Pico 1 bss **222,528 bytes** (was
222,520, +8 from `desc_scroll_`); `lint.sh`/`format.sh` both clean.
**No decision number consumed** — polish inside the already-closed Phase
4D, not a new design call. Full detail: worklog's 2026-08-02 "UI-friction
polish" entry.

**Seven sessions ago:** 2026-08-02 — **Phase 4D CLOSED, docs-only (D40).** Resolved
the three-item Phase 4D close checklist carried below: the **F-evaluator
follow-on check (D37) fired** — idea B (complex vars, 4D.15), C (complex
lists, 4D.24), D (complex matrices, 4D.25), E (vector ops), and G
(eigenvectors, 4D.23) have all shipped and are HW-verified within 4D — and F's
**sequencing is now decided (D40): after Phase 5 (CAS)**, not immediately
following 4D (order: pre-Phase-5 code-review/size-optimization pass → Phase 5
CAS → F). **Idea H (polymorphic variables) deferred again**, stays
unscheduled — TI's three namespaces (`A`-`Z` scalars, `[A]`-`[J]` matrices,
`l1`-`l6`/named lists) stay as-is; revisit only if real usage demands it,
re-checkpoint after F. `ti-parity.md` and `README.md` flipped to reflect
Phase 4 (4A-4D) as complete and hardware-verified rather than "code-complete,
evals pending" — see "The next job" below for the new forward path. No
source changes this session. Full detail: worklog's 2026-08-02 "Phase 4D
CLOSED" entry, `decisions.md` D40 (cross-refs D37,
`design-departures-matrix-complex.md` §H).

**Eight sessions ago:** 2026-08-02 — **D10 leg A, source change, HW-verified on
the Pico 2/RP2350 (`1a45763-dev`).** The dual-core display
pipeline — core-1-offloaded panel pushes — now covers the Pico 2's
full-framebuffer path, closing the "extend to Pico 2" half of the D10
follow-up item below. `start_display_service()` launches the core-1
service on both boards now (was Pico-1-only); the Pico 2's `render_frame`
hands its band push to core 1 asynchronously via the existing
`submit`/`drain_acks` machinery instead of blocking core 0 with a
synchronous `push_rect` (single `frame_buf`, so each frame's `drain_acks`
waits out the previous push before reusing it; a synchronous fallback
covers the pre-service boot window). This exercised the RP2350 XIP/USB
wedge risk the 2026-07-25 Pico 1 RAM-residency fix had never been tested
against on this chip — flashed clean, sustained boot with USB enumerated
throughout, no wedge/fault/drop; developer interactive pass (rapid nav,
fast typing, graph pan/zoom under key-repeat) came back clean. Both boards
build clean, full host suite green (multicore TU isn't in the host build).
D10 **leg B** (compute-parallelize `recompute_function`) is the one
remaining open D10 item — see "The next job" #2. Full detail: worklog's
2026-08-02 "D10 leg A" entry, `decisions.md` D10.

**Nine sessions ago:** 2026-08-02 — **feature follow-on, source changes,
HW-verified on the Pico 2 (build on top of `e5f2a10-dev`).** `MatAns` now
persists across a power cycle (**D39**): reverses the by-design-transient
stance the bugfix session below landed the same day. Save/load reuses the
`[A]..[J]` PCM2 file format via new path-based `save_matrix_file`/
`load_matrix_file` helpers (`matrices_persist.cpp`/`matrix.hpp`); MatAns
gets its own `/picocalc/matans.dat` written on every matrix-result commit
(`home_screen.cpp`) and restored at boot (`main.cpp`, same D14 late-init
retry contract as the named matrices). Host suite green (`test_matrix`
unchanged at 369 — no new host coverage, this path is firmware-only, same
as `MatrixStore`'s own persistence); both boards link clean, Pico 1 bss
unchanged at 222,520; cold-boot survival confirmed on the Pico 2. Full
detail: `worklog.md`'s 2026-08-02 "MatAns now persists" entry,
`decisions.md` D39.

**Ten sessions ago (same day):** 2026-08-02 — **bugfix session, source
changes, HW-verified on the Pico 2 (`e5f2a10-dev`).** Fixed the two minor bugs found in the
2026-07-27 eval: SEQ-mode trace (F4) now reads exact values straight from
`math::seqexpr::value()` instead of the pixel-quantized point cache (was
showing float noise instead of the table's exact integers) — covers both
TIME and WEB seq plot styles (the first cut only handled TIME; the test
board turned out to be in WEB style, which is sticky across reboots via
GraphState/PCG5 — test both styles on seq work going forward). And the
sequence editor no longer draws every recursive row red: added a stateless
`math::seqexpr::compiles()` (lag-rewrite + compile, no iterator side
effects) plus a `SlotEditorScreen::field_valid()` hook the seq editor
overrides, so `u(n-1)`-style self-references validate correctly instead of
failing the plain-engine compile check every other editor uses. Also
corrected three stale claims left in this file by earlier sessions: the
home-screen `MatAns` token and "fnInt shading follows curve color" were
each listed as open gaps but actually shipped as 4D.14/4D.11; `MatAns` not
surviving a power cycle (plus its Pico 2 discrepancy, see "Two sessions
ago" below) was called by-design at the time — `mat_ans()` was a transient
global (`g_mresult`, `mat_expr.cpp:27`) never written to SD. **That call
was reversed later the same day — see "Last session" above (D39): MatAns
now persists.** 12 new host checks (`test_seq` now 63); both boards
rebuilt clean; `clang-format` clean. Full detail: `worklog.md`'s
2026-08-02 bugfix entry.

The 2026-08-02 Pico 2 hardware session (informal perf spot-check — general
UI felt snappy, `graph recompute:` stress probes up to 33.7 ms stayed well
under the 146 ms push-budget floor; the MatAns power-cycle discrepancy it
found was root-caused and then superseded by D39 above), the 2026-07-27
on-device eval (Batches 2-4 PASS, closing all nine D38 batches on the
Pico 1), and the 2026-07-26 Phase 4D kickoff session are further back than
this rolling summary keeps — see `worklog.md`'s 2026-08-02, 2026-07-27, and
2026-07-26 entries (`testdrive-2026-08-02-observations.md`,
`testdrive-2026-07-27-observations.md`, `decisions.md` D38) for full
detail.

## The next job

0. **Seeded but unfinished: the `docs/site` branch** (2026-08-03, off
   `main`, commit `0f1e8ef`, pushed, no PR). Scaffold + generators + CI
   only — every prose chapter under `docs-site/guide/` is still a TODO
   stub, and `docs-site/reference/error-messages.md` is unwritten. Next
   steps whenever it's picked up: write the getting-started and guide
   prose (README's "Using the calculator" is the seed), create the wiki
   `WIKI_TOKEN` PAT if wiki publishing is actually wanted (see
   `docs-site/README.md`), and decide whether to rebase onto `main` after
   Phase 5 merges so the CAS chapter can be written. Independent of the
   Phase 5 work below — it does not block Stage 5.
1. **Phase 5 (CAS) is in progress on the `phase-5` branch — Stages 0-4
   code-complete; 0-3 HW-verified on the Pico 2 (2026-08-02), Stage 4
   host-verified only (2026-08-03).** The engine
   (tree/pool/parser/serializer/simplify/diff/expand/factor/solve/integrate,
   4D.1-4D.19) and the home-screen UI integration (inline CAS calls, F6
   menu, `cas` command, 4D.4/4D.20/4D.21) are both done and pushed; see
   "Last session" above and worklog's 2026-08-02 "Phase 5 Stages 0-3"
   entry. **One stage remains, per `phase5-spec.md` §11:**
   - **Stage 4 — exact-form display (4D.23/4D.24): DONE 2026-08-03
     (D43) + follow-ups the same day (D44). Flashed to the Pico 2, clean
     boot confirmed; interactive confirmation still pending.** Remaining
     on-device script, to run on both boards during Stage 5:
     - Amber typeset exact forms: `sqrt(2)`, `sqrt(8)`, `sqrt(12)`,
       `1/sqrt(2)`, `sqrt(1/2)`, `sqrt(2)+sqrt(8)`, `1/3`, `2/6`,
       `1/3+1/7`, `pi`, `pi/2`, `pi*2`, `1/pi`, `1+sqrt(2)`;
       trig `sin(pi/6)`, `sin(pi/3)`, `cos(pi/3)`, `tan(pi/6)`,
       `tan(pi/3)`, and `sin(pi)`/`cos(pi/2)` → a clean `0`.
     - DEGREE mode: `sin(30)`, `sin(45)`, `sin(60)`, `cos(30)`, `tan(60)`
       fold; `sin(37)` does not.
     - RECT/POLAR number mode: real-valued results still get exact forms;
       genuinely complex ones stay decimal.
     - Unchanged white decimals: `2.5`, `0.1+0.2`, `2+2`, `4/2`,
       `sqrt(4)`, `sin(1)`, `sin(pi/5)`, `tan(pi/2)`, `1/3>dec`,
       `5->a` then `a/3`. `1/3>frac` still works the old way.
     - **Alt+Enter**: on a typed expression → decimal; on an empty line
       after an amber result → re-runs it as a decimal. (Shift+Enter was
       the first binding and does *not* work — it arrives as `kInsert`;
       see "Last session" above.)
     - Reboot and confirm amber forms reload amber (also covers the
       2026-08-03 history fix).
     - **Judgement calls while it's in hand**: whether `1/3` as a stacked
       fraction and `pi` as `π` are welcome or intrusive. D43's "Revisit
       when" names the escape hatch (require a `sqrt`/`pi` flag rather
       than any flag, dropping bare rationals back to decimal).
   - **Stage 5 — hardening + on-device verification (4D.22), not started.**
     Stress/edge-case tests, a pool-capacity guard (abort above ~80%
     capacity, spec Risk 2), the Risk-1 termination cycle set exercised at
     scale (not just unit-test scale); then flash the Pico 2 again and
     **flash the Pico 1 for the first time on this branch** (watch bss
     headroom there specifically — the Pico 1 is the tighter budget).
     Once Stage 5 closes: bump `PICOCALC_PHASE` `"4D"` → `"5"` in
     `CMakeLists.txt`, do the phase-close docs pass (ti-parity.md gets its
     CAS-section sweep at this point, README status flip), and open the
     `phase-5` → `main` PR. **Fold in on-device confirmation of the
     2026-08-03 history-persistence fix** (below) while the Pico 1/Pico 2
     are on the bench for this stage anyway — it's a firmware-only path,
     not covered by the host suite.
   - **BUG flagged 2026-08-02, RESOLVED 2026-08-03**: the suspected
     home-screen history I/O persistence bug was root-caused (symbolic CAS
     results lost their `ResultKind` on reload) and fixed, along with two
     related latent bugs (head-vs-tail read, unbounded file growth) found
     during the investigation. See "Last session" above, worklog's
     2026-08-03 entry, and `decisions.md` D4 (amended in place). Still
     open: on-device confirmation that history now survives a reboot
     correctly (see the Stage 5 bullet above) — the fix is host-logic
     verified (600 checks) but this path itself isn't in host coverage.
   - The pre-Phase-5 SRAM levers noted before CAS started remain relevant
     background (all still deferred, none urgent — watch Pico 1 bss as
     Stage 4/5 lands): (a) MicroPython heap 48→40 KB if the ~12 KB spare
     gets eaten by CAS + 6A framework growth (spec Risk 6); (b) ArrayStore
     slab cut (~12-16 KB, more with a PSRAM-fallback prerequisite); (c)
     persistence `g_chunk` fold (~6 KB); (d) arena debug owner-guard. Full
     write-up: `docs/notes/pre-phase5-review.md`.
   - **After Phase 5 closes**: F (the unified evaluator, D37/D40 —
     deliberately sequenced after CAS so a possible 4th symbolic evaluator
     is known before unification), then revisit idea H (polymorphic
     variables, D40 — unscheduled, only if real usage demands it).
   - Phase 4D itself has been closed since 2026-08-02 (D40, all 9 D38
     batches HW-verified) — see worklog's 2026-08-02 "Phase 4D CLOSED"
     entry if the pre-CAS history is needed; `phase4-spec.md` §8 and
     `decisions.md` D37/D38/D40 have the full task/decision map.
2. **D10 follow-ups** (originally from 2026-07-25, no phase home):
   - **Extend the display pipeline to Pico 2 — DONE + HW-VERIFIED
     2026-08-02 (leg A).** `start_display_service()` now launches the
     core-1 service on both boards; the Pico 2 full-framebuffer push
     routes through core 1 asynchronously (`submit`/`drain_acks`) instead
     of blocking core 0. The RAM-residency fix's XIP/USB wedge risk was
     re-verified on the RP2350 (sustained boot, USB enumerated, no
     wedge/fault/drop) and a developer interactive pass (rapid nav, fast
     typing, graph pan/zoom under key-repeat) came back clean — no
     tearing, no corruption, no freeze. See `decisions.md` D10, worklog's
     2026-08-02 "D10 leg A" entry.
   - **Compute parallelization candidate (leg B), still open**: the pipeline gives ~0 benefit
     on compute-bound screens (render > ~146 ms push budget; a heavy graph
     redraw measured 1.17 s on the Pico 1). Those want
     `GraphScreen::recompute_function` (`src/apps/graph_screen.cpp:313`)
     parallelized — needs a second engine/vars context (shared `X`
     mutation), not just a spawned task. 2026-08-02 Pico 2 stress probes
     (up to 20 nested trig calls, 33.7 ms) didn't reach this regime either
     — see `testdrive-2026-08-02-observations.md` for a nesting-depth
     scaling anomaly worth another look if this is picked up.
3. **Pico 2 perf spot-check: done informally, 2026-08-02.** Reflashed to
   current HEAD; general UI felt snappy, and `graph recompute:` stress
   probes (up to 33.7 ms) stayed well under the 146 ms push-budget floor —
   no compute-bound stall observed. This was an interview-driven spot
   check, not a rigorous side-by-side comparison against the pre-Phase-3
   2.25 baseline — a systematic re-measurement remains optional/
   low-priority if ever wanted. Full detail:
   `testdrive-2026-08-02-observations.md`.

Mind the §8 strip-safety rule (idempotent `render()`) for any new
screens touched during the on-device passes.

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **2026-08-03's history-persistence fix has NOT been flashed to either
  board yet** — built/linted/host-tested only (this is a firmware-only
  path, no host UI to exercise it against real SD I/O). It's a persistence
  *format* change (`history.txt` gained a third tab-separated kind
  column) but is backward compatible — no one-time reset, old
  two-field lines still parse as `kPlain`. Confirm on-device (both
  boards) alongside the Stage 5 flash, see "The next job" #1.
- **Firmware on the Pico 2 was reflashed on `phase-5` (2026-08-02) with
  Phase 5 Stages 0-3** — the CAS engine + home-screen UI integration
  (inline `diff()`/`integ()`/`factor()`/`expand()`/`simplify()`/`solve()`,
  F6 CAS menu, `cas` command). This is the first CAS build to reach either
  board. HW-verified: clean boot, all six inline ops, the F6 menu, exact
  fraction display, right-aligned/descending-order/amber-accent results,
  and the pannable long-result window all confirmed working interactively.
  No persistence format change, no one-time reset (CAS results are
  display-only, never written to SD). **The Pico 1 has NOT been flashed
  with any Phase 5 code yet** — that's part of Stage 5, see "The next job"
  #1. See "Last session" above for the full commit list.
- **Firmware on the Pico 2 was reflashed again same-day (2026-08-02) with
  this session's UI-friction polish** (`0cfbe05-dev`) — compact matrix
  cell formatting, matrix `>Frac`, and the constants-picker relayout +
  description scroll. HW-verified: clean sustained boots at every step,
  no faults; `>Frac` on matrices and the constants-picker scroll both
  confirmed working as intended. No persistence format change, no
  one-time reset. See "Last session" above.
- **Firmware on the Pico 2 was reflashed again same-day (2026-08-02) with
  the D10 leg A change** (`1a45763-dev`) — the display pipeline now
  offloads the Pico 2's full-frame push to core 1 (previously
  synchronous on core 0, see the bullet below which now describes a
  superseded state for that one item — "The next job" #2 and
  `decisions.md` D10 have the current picture). HW-verified: sustained
  boot with USB enumerated, no wedge/fault/drop, and a clean interactive
  pass (rapid nav, fast typing, graph pan/zoom under key-repeat).
- **Firmware on the Pico 2 was reflashed to current HEAD on 2026-08-02**
  (`dadc7cf`; was 9 builds behind, still Session 19's font/glyph build)
  — it now carries the same code as the Pico 1: the D35 perf fixes, the
  2026-07-25 work (`!` factorial fix, D10 display pipeline — core-0-sync
  path only, see "The next job" #2, die temp/build id), and all of Phase
  4D (Batches 1-9). First boot showed the expected one-time reset under
  the PCV1/PCL2/PCM2 format bumps. Its build still layers on top of
  Sessions 11/12/15/16/17/18 (3A lists, 3B stats, 3D inference/plots, 4A
  matrices/solver, 4B CALC menu, 4C complex numbers). **All of their
  hands-on on-device evals remain closed as a formality (2026-07-22)** —
  board-independent logic, and the harder rendering case (Pico 1) passed
  the identical checklists the same day (3D.14 for 11/12/15, the Phase
  4A-4C pass for 16/17/18); see `worklog.md`'s 2026-07-22 entries. The
  Pico 2 perf re-baseline ("The next job" #3) is now done informally as
  of 2026-08-02 — see the top of this file and
  `testdrive-2026-08-02-observations.md`. **Item from that session, now
  resolved**: `MatAns` persisted across a power cycle on the Pico 2,
  contradicting the Pico 1 finding (2026-07-27) on identical code — first
  root-caused as warm-reset RAM retention of a transient global (not a
  source bug), then later the same day the underlying by-design-transient
  stance itself was reversed: **MatAns now persists on both boards by
  design (D39)**, so the discrepancy question is moot going forward — see
  "The next job" #1. Session 15's storage-health row
  (hot-plug/retry-forever, Y=-editor truncation) is fully closed —
  confirmed on both boards. **Session 10 round 2 is also closed
  (2026-07-22)**: `L` toggle surviving a reboot, `rand()` showing a
  sensible varying value, ZTrig short tick labels (`1.571`-style), and `F`
  ZoomFit auto-fit all confirmed on the Pico 1. The HW-PENDING table is
  now clear except the still-informal Session 19 font sweep.
- **The Pico 1 now carries ALL of Phase 4D (Batches 1-9, 2026-07-26)**
  on top of the 2026-07-25 work and the D35 state. Flashed and
  boot-verified over serial after every batch (temp + psram-bulk
  heartbeats healthy throughout); final bss **222,520 bytes**, ~48 KB
  headroom — keep watching per the D28 watch item. One-time resets
  already absorbed on this board: PCG6 (Batch 3 flash). New SD files
  since: `listdir.dat`/`nlist<idx>.dat` (named lists, Batch 6) and
  `settings.dat` PCS1 (Batch 9 — created on first `settings` change).
  **Batch 9's APD defaults to 5 min**: an idle unit now dims its screen;
  any key wakes it (the wake key is swallowed). Phases 2-3 and 4A-4C
  are HW-verified on this board; **all nine 4D Batch 1-9 checklists are
  now cleared** (Batch 1 + 5-9 on 2026-07-26, Batches 2-4 on 2026-07-27
  — see worklog table). Three non-blocking findings from the 2026-07-27
  pass (SEQ trace snap, SEQ color swatch, `MatAns` not persisting) were
  all fixed and HW-verified 2026-08-02 — see "The next job" #1. All five font headers were regenerated with glyph slot 141 in
  Batch 7 — the non-default font builds (`build/pico2-jm|io|uni|term`)
  remain stale as before (the default `build/pico2` Terminus build is
  current as of the 2026-08-02 reflash, see above).
- **Persistence change 2026-07-26: `variables.dat` bumped to magic PCV1**
  (header + vars + imag parts). The old raw 224-byte file is ignored →
  **expected one-time variables reset on first boot** under this firmware
  (same precedent as PCL2/PCM2/PCG bumps), then persistence resumes.
  GraphState still **PCG5**; list/matrix formats still PCL2/PCM2 — but
  complex lists now write 16 B/elem payloads under the unchanged PCL2
  header (the dtype byte was always there); older firmware treats a
  complex list file as corrupt and skips it gracefully.
- **Flash-path notes 2026-07-26**: the BOOTSEL-volume `cp` failed with
  "Permission denied" this time (a new failure mode vs. the old xattr
  complaint) — `picotool load -f` remains the reliable path. Also
  **`picotool info` segfaults in picotool v2.3.0**; `load`/`reboot` work
  fine, just don't use `info` to check state.
- **List/matrix persistence changed shape this session (D35)**: the old
  single `lists.dat` / `matrices.dat` (magics PCL1 / PCM1) are replaced by
  one file per store — `/picocalc/list1.dat`..`list6.dat` (magic PCL2) and
  `/picocalc/matrix1.dat`..`matrix10.dat` (magic PCM2). Old images simply
  aren't read under the new paths (same "old files ignored" precedent as
  prior format bumps) — expect a one-time reset to empty lists/matrices on
  first boot under this firmware, already confirmed as expected. If a load
  ever misbehaves, deleting the relevant `listN.dat`/`matrixN.dat` resets
  just that one store.
- **Non-default font builds** (`build/pico2-jm|io|uni|term`) are stale
  relative to this session's non-font changes (eig alias, list scroll)
  — rebuild before re-comparing fonts. `build/pico2` (Terminus) is the
  canonical default and what's currently flashed.
- **D14 cold boot (~5-8 s rail settle):** PSRAM/SD may fail early init on
  a cold power-on; self-tests retry inside the 30 s late-init window and
  serial prints `late-init: ...` lines (including `lists loaded`).
  Large lists are simply absent until then; the editor shows "List
  memory unavailable" if a >256-element append beats PSRAM bring-up.
  Stats on a not-yet-loaded list just sees fewer/empty elements.
- **Flash path (revised Session 9, reconfirmed Session 12):** `stty -f
  /dev/cu.usbmodem* 1200` reboots to BOOTSEL, the **RP2350 volume
  mounted in ~5 s this time**, then
  `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/`
  (auto-reboots; cp exited 0 this session — the Session 11 xattr
  complaint didn't recur). Keep `picotool load` + `picotool reboot` as
  the fallback for when the volume doesn't mount at all.
- **Battery/charging: fully verified 2026-07-18.** Refresh cadence is 5 s
  by design — stability over snappiness; don't "optimize" it back down.
- **Boot printfs still race USB enumeration** — only prints after ~2 s
  (late-init, battery, recompute) are capturable. Don't chase "missing"
  early boot output.
- **STM32 caution unchanged (both boards):** never poll STM32 registers
  back-to-back; a wedge needs a physical power cycle. Fw is v1.6.

## Pico 1 pass: DONE (3D.14 + Phase 4A-4C, D18 resolved 2026-07-22)

The combined pass decided 2026-07-18 (D18) ran 2026-07-22 as task 3D.14: the
Pico 1 was reflashed to current HEAD (Session 19) and put through the full
Phase 2 sweep — headline **split-pane clipping on the strip renderer**, no
bleed — plus the Session 8+9 fix list and the Phase 3 acceptance checklist.
All passed; two non-blocking findings (factorial `!`, list-editor/scatter-plot
perf) were logged at the time. The perf finding was fixed later the same day
(D35 — see "Open design threads" and the fourth 2026-07-22 worklog entry); the
factorial bug remains open in "Backlog" below. Full detail: worklog 2026-07-22 entry,
`phase3-retro.md`, `session3D14-pico1-observations-verbatim.md`. **This
closes Phase 3.** Guardrail carried forward: Phase 3+ render code must stay
strip-safe (idempotent, may run ~20x/frame) — rule recorded in
`phase3-spec.md` §8; it held up cleanly this pass. Pico 1 bss was ~188.8 KB of
264 KB as of Session 19 (D28/D29/D30/D31 combined, essentially flat) — no
headroom pinch observed during 3D.14.

**Same session, second block**: Phase 4A-4C (matrices/solver, CALC menu,
complex numbers) also got their first-ever hands-on pass, on this same
Pico 1/build — all passed (details in "Last session" above and the worklog
Phase 4A-4C entry). This also **closed the Session 16/17/18 Pico 2
HW-PENDING rows as a formality** (board-independent logic, harder rendering
case already passed) — Pico 2 has no genuinely open board-specific gap left
except its own perf re-baseline (low priority, "The next job" #3). Map-file
re-check (the Pico 1 bss watch item, knob `ArrayStore::kSlabCount`) can now
be considered done for this generation of features — no headroom pinch
observed across either pass.

## Open design threads

- **List UX watch-items (Session 11, judge on device)**: F8 clear-list is
  immediate (no confirm); list history results truncate at ~40 chars
  (`,...`); `lists`/`stats` are typed-command-only entries (now with
  `list`/`stat` aliases, D24) — decide whether stats deserves an
  F-key/menu slot now that the screen exists. (Resolved by D24:
  reductions bare-arg limitation; mean/median/stdev promotion.)
- **Stats watch-items (Session 12, judge on device)**: results are
  plain text lines (no two-column layout for 2-Var's 17 lines).
  (Resolved by D24: "Computing..." indicator — verify its visibility
  on a 10000-element 1-Var.)
- **Session 13 caps to watch**: 4 lift operands per expression, 64
  elements per brace literal — revisit if real use pinches (D24).
- F3 MODE vs ZOOM (TI's F3 slot) — judge after real use (D20 KIV).
- D16 trace-sync option b (trace steps by table-step) — after more split
  use.
- **4B CALC watch-items (Session 17, judge on device)**: min/max "Guess?"
  step is UI-only, doesn't feed Brent's bracket — decide if that's fine or
  needs wiring through (D29). (Resolved by D29: P4-6 intersect = cursor-
  cycle; P4-8 polar fnInt = area only, no arc length — both from
  `phase4-spec.md` §11, tracked here and in `decisions.md` rather than
  editing the spec's open-questions table.)
- **4C watch-items (Session 18, judge on device)**: whether "Non-real
  result" is clear phrasing for REAL-mode domain errors; no
  complex-valued variable storage (`2i->a` errors) — decide if that's
  ever actually wanted (D30). (Resolved by D31: the ASCII `<` polar
  stand-in is now a real ∠ glyph, Terminus default.)
- **Font/glyph watch-items (Session 19, judge on device)**: informally
  spot-checked during the 2026-07-22 Phase 4A-4C pass and reported looking
  correct — not a dedicated sweep, so still worth a proper pass if time
  allows, but no longer a blind spot. Open sub-items: whether `√` read as
  inline-only (`√(x)`, no vinculum) is acceptable; whether the shared
  Unifont-derived `i`/⇒ glyphs look consistent against Terminus's own glyph
  shapes; big-radical display and true subscripts (`Sₓ`, `σₓ`) remain
  KIV/wishlist items (D31).
- **Pico 1 watch-items (task 3D.14 + Phase 4A-4C, 2026-07-22)**: `!`
  (factorial) syntax error in non-REAL Number mode — **FIXED 2026-07-25**
  (`5852c35`, `complexexpr` now shares engine's postfix-`!` rewrite;
  `5!`/`4!` HW-verified). The list editor and 5000-point scatter plot sluggishness
  are **fixed as of the same day (D35)**: bucketed stat-plot point cache,
  list-editor dirty-band narrowing, and (the real bottleneck behind "large
  lists feel sluggish to enter") one-file-per-list/matrix SD persistence —
  all flashed and developer-confirmed on the Pico 1, see `worklog.md`'s
  fourth 2026-07-22 entry and `decisions.md` D35. Two more from the
  Phase 4A-4C pass, **both since shipped in Phase 4D** (this list was
  written 2026-07-22, before 4D landed them): the home-screen `MatAns`
  token arrived as **4D.14** (`matans` is a real expression token now —
  `mat_expr.cpp:591`, `decisions.md` D-line 60), and **fnInt shading now
  follows the curve color** — darkened palette per slot, `4D.11`
  (`graph_screen.cpp:1146`, "was a fixed blue"). All originally logged in
  `session3D14-pico1-observations-verbatim.md`,
  `phase4abc-pico1-observations-verbatim.md`, and `phase3-retro.md`.
- Backlog: D14 rail settle ([next-bench-session.md](next-bench-session.md) —
  the last deferred HW item); 340-point curve cache cap; audio HAL; licensing (D17 —
  display/keyboard rewrites remain); dual-core display service (D10
  addendum — **DONE 2026-07-25: root-caused + fixed + pipeline shipped,
  see "The next job" #1b; two non-blocking follow-ups there — Pico 2
  full-frame pipeline, and compute-parallelizing `recompute_function`**);
  **stale diag-screen label — DONE 2026-07-25**: `main.cpp` header comment
  de-staled; diag title line now shows `Phase 4C [<hash>-dev]` right-aligned
  (build id via CMake `PICOCALC_BUILD_ID` = `git rev-parse --short HEAD` +
  dirty check); leftover per-strip `Frame:` counter removed. Also added a
  **die-temperature read** (`platform::die_temp_c()`, on-chip ADC ch 4) on
  the diag screen + a `temp:` 30 s serial heartbeat (idle ~28-31 C).

## Feature wishlist

Desired-but-unplanned features live in **[wishlist.md](wishlist.md)**. Complex
numbers and TI-84 CALC-menu graph analysis graduated into Phase 4 (sub-phases
4C and 4B — both code-complete). The 2026-07-21 stocktaking session (D32/D33)
graduated most of the rest: eight items into Phase 4D, one into Phase 6 §9,
and the old "symbolic display" item split in two — pi-ticks/`▶Frac` into 4D,
surd/exact-value display into Phase 5 §10.1. What's left unscheduled:
antialiased font rendering (revisit once the Phase 6 desktop-emulator
candidate exists) and the SD list-data-file/CBL-CBR half of the old
"beyond 6 lists" item. See [wishlist.md](wishlist.md) for current detail.

## Hardware debugging kit (reminder)

- Serial: **plain `cat` reads nothing** — pico stdio_usb only transmits
  with DTR asserted. Interactive: `./scripts/monitor.sh` (screen).
  Non-interactive/agent: `./scripts/serial-capture.py [seconds]
  [match-substring]`. Lines: `late-init:` (incl. `lists loaded`),
  `battery:` (change + 30 s heartbeat), `psram-bulk:` (30 s heartbeat),
  `graph recompute: N us`.
- Flash: see the Pico 2 notes above; Pico 1 BOOTSEL volume is `RPI-RP2`.
- `picocalc_diag` target = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
- Session protocol: read this file first when starting fresh; update it
  before ending a session.
