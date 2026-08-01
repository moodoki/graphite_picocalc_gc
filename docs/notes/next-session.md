# Start here — next session

**Last session:** 2026-08-02 — **D10 leg A, source change, HW-verified on
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

**Previous session:** 2026-08-02 — **feature follow-on, source changes,
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

**Two sessions ago (same day):** 2026-08-02 — **bugfix session, source
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

**Three sessions ago (same day):** 2026-08-02 — **Pico 2 hardware session, no
source changes** (one doc-only wishlist addition). Reflashed the Pico 2
from the stale Session 19 build (9 builds behind) to then-HEAD (`dadc7cf`)
and ran a hardware-observation interview. First boot showed the expected
one-time data reset under the PCV1/PCL2/PCM2 format bumps; general UI perf
feel was reported snappy; three `graph recompute:` serial-instrumented
stress probes (7 functions + 8001-pt scatter = 50.8 ms; 1 function/10
nested trig calls = 28.1 ms; 1 function/20 nested trig calls = 33.7 ms —
notably sub-linear with nesting depth, unexplained) all stayed well under
the ~146 ms push-budget floor, so no compute-bound stall was produced on
this board. Display pipeline showed no tearing/flicker/stutter; APD 5-min
dim/wake worked as expected. **One notable discrepancy** (root-caused two
entries above, then the underlying by-design stance itself reversed in
"Last session" — D39): `MatAns` *persisted* across a power cycle on the
Pico 2 this session, contradicting the Pico 1 finding from 2026-07-27 —
same code on both boards at the time, so it looked like an open
board-to-board discrepancy until root-caused as warm-reset RAM retention
of a transient global, not a source bug (and MatAns persistence is now the
intended behavior on both boards regardless). Also added "no copy/paste in
expression editors" to `wishlist.md`. This addressed "The next job" #3
(Pico 2 perf spot-check) informally. Full detail:
`testdrive-2026-08-02-observations.md`.

**Four sessions ago:** 2026-07-27 — **on-device eval only, no code
changes.** Hands-on test-drive on the Pico 1 (current Phase 4D build)
covered the last three open HW-PENDING rows — Batch 2 (complex matrices),
Batch 3 (sequence graphing), Batch 4 (zoom + shading) — all PASS. Batch 3
found two minor bugs (the SEQ trace/color-swatch bugs fixed two entries
above) and a third was found in passing on Batch 5's `MatAns` (now fixed —
see "Last session" above, D39). Also re-confirmed the `π` tick-label fix
(2026-07-26) renders correctly. Two UI-friction feature requests logged, no
fix yet: cap displayed decimal digits on matrix results; multi-character
constant names are hard to read in the constants picker (kiv). **All nine
D38 batches (Phase 4D) are now hardware-verified on the Pico 1** — see
`worklog.md`'s 2026-07-27 entry and
`testdrive-2026-07-27-observations.md` for the full report. Phase 4D itself
is not yet declared closed — see "The next job" below for what's left. Full
detail on the 2026-07-26 Phase 4D kickoff session (planning pass D38, Batch
1 complex vars/lists): `worklog.md`'s 2026-07-26 entry, `decisions.md` D38.

## The next job

1. **Phase 4D is CODE-COMPLETE and its on-device eval backlog is now
   CLEAR (all 9 D38 batches hardware-verified on the Pico 1 — Batch 1
   and Batches 5-9 on 2026-07-26, Batches 2-4 on 2026-07-27).** What's
   left before Phase 4D can be declared closed:
   - Run the **F-evaluator follow-on check (D37)**.
   - Revisit **idea H (polymorphic variables)** — stays undecided.
   - Update **`ti-parity.md` and the README status table** (README
     Status blurb + Features bullet + Project-status table row all
     still say "code-complete, evals pending" — flip to reflect the
     evals passing once the above two items are also resolved, or
     sooner if the developer wants that split into two steps).
   - **Before Phase 5/6: a code-review + size-optimization pass.** SRAM
     headroom has been shrinking each phase (Pico 1 bss now 222,520
     bytes, ~48 KB headroom, down from ~57 KB pre-4D) and Phase 5 (CAS)
     and Phase 6 (app framework + MicroPython) will both add
     significant static footprint — worth a dedicated pass to review
     for dead weight / bloat and trim before piling on more, rather than
     finding out mid-Phase-5 that the budget is gone. Research starting
     point: `size-optimization-ideas.md`.
   - Then **Phase 5 (CAS)** per D32/D33.
   - **Three follow-up items from the 2026-07-27 eval — all now FIXED
     and HW-VERIFIED (2026-08-02, Pico 2)** (all non-blocking):
     - **SEQ-mode trace (F4) float noise — FIXED + HW-verified.** The
       readout read x/y back from the pixel-quantized cache; the trace
       now reads exact values straight from `seqexpr::value`
       (`graph_screen.cpp` `draw_trace`). Covers **both** seq styles —
       TIME `(n, u(n))` and WEB cobweb vertices `(u(k-1), u(k))` /
       `(u(k), u(k))`. (First cut only did TIME; on-device the board was
       in WEB style — which persists in GraphState/PCG5 across the format
       resets — so the readout stayed noisy until the WEB branch was
       added. Lesson: seq style is sticky, test both.)
     - **SEQ editor recursive rows drawn red — FIXED + HW-verified.** The
       editor validated row text with the plain engine, which can't
       resolve `u(n-1)` self-refs, so every recurrence looked "broken"
       (red) and explicit forms white — this was the "color swatch"
       symptom. Added a stateless `seqexpr::compiles()` (lag-rewrite +
       compile, no iterator side effects) and a
       `SlotEditorScreen::field_valid()` hook the seq editor overrides.
     - **`MatAns` doesn't survive a power cycle — FIXED + HW-verified
       (D39).** Briefly called by-design earlier the same day (`mat_ans()`
       was a transient global, `g_mresult`, `mat_expr.cpp:27`, never
       written to SD) — the developer decided MatAns should persist like
       the named `[A]..[J]` matrices instead. Now saved/restored via
       `math::matexpr::save_ans`/`load_ans` to its own
       `/picocalc/matans.dat` (same PCM2 format, shared `save_matrix_file`/
       `load_matrix_file` helpers in `matrices_persist.cpp`); confirmed
       surviving a physical power-off/on on the Pico 2.
   - **Two UI-friction feature requests, no fix proposed yet**: matrix
     results with many decimal places are hard to read (consider
     capping displayed digits); multi-character constant names are
     hard to read in the constants picker (kiv — design thought
     needed).
   Task table: `phase4-spec.md` §8; decisions: `decisions.md` D37/D38 (the
   idea A-G → task-ID map is in D37 and
   [design-departures-matrix-complex.md](design-departures-matrix-complex.md);
   idea H — polymorphic variables — stays undecided, revisit after 4D
   ships, same checkpoint as F). **Phase 5 (CAS)** then **Phase 6
   (app framework + MicroPython)** follow 4D per D32/D33.
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
