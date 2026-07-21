# Start here — next session

**Last session:** 2026-07-22, two blocks of work. **(1) Task 3D.14**, the
deferred Pico 1 combined pass (D18): the Pico 1 (RP2040) was swapped back in
after three days on Session 7 firmware; rebuilt and reflashed
`build/pico/picocalc_graphcalc.uf2` from current HEAD (Session 19's
font/glyph build — no code changed), then ran the full Phase 2 sweep, the
Session 8+9 fix list, and the Phase 3 acceptance checklist against it.
Everything passed except two non-blocking findings (factorial `!` syntax
error, list-editor/5000-point-scatter perf feel — see below). **This closes
task 3D.14, resolves D18, and closes Phase 3** — retro in `phase3-retro.md`.
**(2) Phase 4A-4C on-device eval, same board/build**: ran the Phase 4A
(matrices+solver), 4B (CALC menu), 4C (complex numbers) checklists on the
Pico 1 — the first hands-on functional pass of any of these three on
*either* board. All passed (two reports turned out to be intentional design
per D28, not bugs; one real UX gap — no home-screen `MatAns` token; one
feature request — fnInt shading should follow curve color). **Closed the
Session 16/17/18 Pico 2 HW-PENDING rows as a formality** (board-independent
logic, harder rendering case already passed) — see "Key things to note".
**Root-caused the factorial `!` bug by code scan, no hardware needed**:
`complexexpr` lacks `engine`'s postfix-`!`→`fac()` rewrite, so `5!` fails in
non-REAL Number mode on any board — not a keyboard quirk. Fix candidate
identified but **not yet applied** (see "The next job" #1). Logged verbatim
in `session3D14-pico1-observations-verbatim.md` and
`phase4abc-pico1-observations-verbatim.md`. No other code changed this
session (docs/observations/investigation only); host test count (1219
checks) and both-boards build status are unchanged from Session 19. Full
detail in `worklog.md` (two 2026-07-22 entries) and `decisions.md` (D18
resolution).

## The next job

1. **Fix the `!` factorial bug** (root-caused 2026-07-22, not yet applied):
   `math::complexexpr::evaluate` (`src/math/complex_expr.cpp`) needs the same
   postfix-`!`→`fac()` rewrite `math::engine`'s `preprocess()` already has
   (`src/math/engine.cpp`), or bare-`!` expressions need to route through
   `math::engine()` even in non-REAL Number mode. Small, well-understood fix
   — good to knock out before or alongside Phase 4D. See the worklog
   2026-07-22 entry for the full trace.
2. **Phase 4D (GC completeness)** is the next major work, per
   `phase4-spec.md` §7 (weeks 32-35) — the closing pass that rounds Phase 4
   out into the project's pre-release milestone (sequence graphing, fuller
   zoom/shading, list↔matrix bridge, scientific constants, unit conversions,
   home-screen matrix literals, complex-valued variable/Ans storage, device
   polish). Its own task list may want to pick up two things surfaced
   2026-07-22: a home-screen `MatAns` token (currently editor-only, a UX
   gap relative to scalar `ans`) and fnInt shading following curve color
   (feature request, alpha-blend or hatching if true alpha isn't feasible).
   **Phase 5 (CAS engine)** follows 4D per `phase5-spec.md` — symbolic
   simplify/expand/solve, using the Session 18 `Complex` type as the numeric
   backing for complex roots (§4.1 hook; quadratic/polynomial solves with
   negative discriminant emit `i`-valued symbolic roots). **Phase 6
   (non-calculator functions)** follows Phase 5 — an app-launcher framework
   (6A) with MicroPython as its first app (6B), replacing the old 4E plan.
   This ordering (4 → 5 → 6) and the phase split itself were decided
   2026-07-21 (D32, D33) — see `phase4-spec.md`, `phase5-spec.md`,
   `phase6-spec.md`, and `decisions.md` D32/D33. MicroPython's phase slot
   (an open question as of D32) is now resolved: Phase 6 sub-phase 6B.
3. **Low-priority, not blocking**: Pico 2 perf feel for Phase 3/4 features
   (lists, StatPlots, CALC-menu rendering, matrix ops) has never been
   re-measured against current code — only the pre-Phase-3 2.25 baseline
   exists. Worth a quick spot-check next time the Pico 2 is in hand,
   especially given the Pico 1 found two sluggish spots (list editor,
   5000-point scatter) on 2026-07-22 while big-matrix ops felt fine there.
4. **After 4D ships (its on-device eval), before Phase 5 starts in
   earnest: decide the remaining matrix/complex "first-class" departures**
   — see
   [design-departures-matrix-complex.md](design-departures-matrix-complex.md).
   Ideas A (home-screen matrix literals) and B (complex variable/Ans
   storage) are already scheduled as 4D.14/4D.15 — nothing to decide
   there. What's still open: **C/D** (complex-valued lists/matrices —
   gated on a Pico 1 memory feasibility check the doc itself calls for;
   4D.15's actual measured bss cost for widening `Variables` storage is
   exactly the data point that check needs, so this can't be scoped well
   before 4D ships), the **vector-ops half of E** (`dot`/`cross`/`norm` —
   never made it into 4D's task list, only the list↔matrix bridge half
   did, 4D.12), and **F** (unifying `matexpr`/`complexexpr`/`listexpr`
   into one tagged-value evaluator — explicitly a "wait for duplication
   pain" trigger, not a calendar one; check whether it's fired once
   4D.15 and whichever of C/D/E get picked up have shipped). Do this as
   a short scoping pass, not mid-4D — it needs 4D's real numbers, not
   guesses.

Mind the §8 strip-safety rule (idempotent `render()`) for any new
screens touched during the on-device passes.

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Firmware on the Pico 2 is Session 19's font/glyph build**
  (Terminus default, `-DPICOCALC_FONT=terminus`; flashed 2026-07-21,
  boots healthy, telemetry clean over serial). This build layers on top
  of Sessions 16-18 (4A matrices/solver, 4B CALC menu, 4C complex
  numbers). **Their hands-on on-device evals are now closed as a
  formality (2026-07-22)** — the logic is board-independent and the
  harder rendering case (Pico 1) passed the identical checklist the same
  day; see `worklog.md`'s 2026-07-22 Phase 4A-4C entry. The one thing
  that's genuinely still open and Pico-2-specific: perf feel hasn't been
  re-measured against current code (only the pre-Phase-3 2.25 baseline
  exists) — low priority, see "The next job" #3.
- **The Pico 1 is now also on the Session 19 build** (reflashed
  2026-07-22, task 3D.14 — no code changes since Session 19, so it's
  the identical binary): boots healthy, telemetry clean over serial.
  Phase 2 + Phase 3 are HW-verified on this board (3D.14 pass, closes
  D18/Phase 3), and **Phase 4A-4C are now also HW-verified on this board**
  (same session, second block of work) — full pass, see the Phase 4A-4C
  worklog entry. Font/glyph correctness was informally spot-checked
  during that pass and reported looking correct (not a dedicated sweep).
  Two non-blocking findings open: the `!` factorial bug (root-caused, fix
  not yet applied — see "The next job" #1) and list-editor/scatter-plot
  perf feel (not profiled) — see "Backlog".
- **No GraphState layout change this session** — still **PCG5** (last
  bumped Session 18/D30), no new one-time reset on this flash.
- **`lists.dat` / `matrices.dat` may not exist yet on the SD card** —
  first save creates them. If a load ever misbehaves, deleting the
  file resets that store (magics PCL1 / PCM1; bump on layout change).
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
perf) are open in "Backlog" below. Full detail: worklog 2026-07-22 entry,
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
  (factorial) throws a syntax error in non-REAL Number mode — **root-caused
  by code scan** (`complexexpr` lacks `engine`'s postfix-`!` rewrite,
  reproduces on any board, not a keyboard quirk), fix identified but not
  yet applied (see "The next job" #1). The list editor and a 5000-point
  scatter plot both feel sluggish on the Pico 1 — not profiled; could be
  strip-renderer overhead, RP2040 clock speed, or list/plot-specific cost
  (note: big-matrix ops felt fine on the same board, so it's not a blanket
  Pico 1 slowdown). Two more from the Phase 4A-4C pass: no home-screen
  `MatAns` token (editor-only, UX gap vs. scalar `ans`); fnInt shading
  should follow curve color (feature request). All logged in
  `session3D14-pico1-observations-verbatim.md`,
  `phase4abc-pico1-observations-verbatim.md`, and `phase3-retro.md`.
- Backlog: D14 rail settle ([next-bench-session.md](next-bench-session.md) —
  the last deferred HW item); 340-point curve cache cap; audio HAL; licensing (D17 —
  display/keyboard rewrites remain); dual-core display service (D10
  addendum); **stale diag-screen label** (`src/main.cpp:213`) — still
  hardcoded `"[milestone 1]"` from the Phase 1 bootstrap, never updated
  through milestones 2-5 or phases 2-4. Replace with the current phase
  (e.g. "Phase 4") and a build identifier: git short hash if the tree is
  clean, else `dev` — so GitHub Actions builds show a traceable hash and
  local dev builds (usually dirty) show `dev`. Needs CMake to capture
  `git rev-parse --short HEAD` + a clean/dirty check (`git status
  --porcelain`) and pass it through as a compile definition; the
  `main.cpp:6-8` header comment describing "Milestone 1 state" is also
  stale and should go.

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
