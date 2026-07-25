# Start here — next session

**Last session:** 2026-07-26 — **Phase 4D started.** Two blocks: **(1)** a
full 4D planning pass, every open question resolved — recorded as
**`decisions.md` D38** (4D.16 found already shipped in 3D/D27, closed
zero-work; 4D.21 closed; P4-10 = named user lists full integration; P4-12 =
full u/v/w + cross-reference; P4-13 = rref-nullspace; units = typed
`convert()` only; APD = soft-sleep v1 + `settings.dat`; two scope adds —
`MatAns` → 4D.14, fnInt shading in curve color → 4D.11; **sequencing =
risk-first batches**; `phase4-spec.md` §7.3/§8/§11 updated, 4D subtotal
~165 h / Phase 4 ~300 h). **(2) Batch 1 implemented and code-complete:
4D.15 complex Variables/Ans + 4D.24 complex lists** — parallel `imag[]` on
`Variables`, real-only consumers error (never truncate, P4-11/D37), a
`sweep_slot` compile exclusion so a stale complex `x` can't block graphing,
`2i->a` now stores (**variables.dat bumped to PCV1** → one-time variables
reset), `Dtype::kComplex` PSRAM-only arrays (5000-elem cap), complex brace
literals + narrow vector lift (`+`/`−`/scalar `*`,`/`), standalone
`sum`/`mean`, list-editor complex entry/display with in-place real→complex
migration, stats/plots erroring or skipping per D37 scope (lists stay
PCL2 — the dtype byte was already there). Host suite 12/12 binaries,
**1305 checks** (test_lists 134→196, test_complex_expr 44→74). Also fixed
two pre-existing lint failures from 2026-07-25 (framebuffer int-to-ptr
NOLINT, main.cpp dead store). Flashed to the **Pico 1** and boot-verified
(bss 213,332, ~57 KB headroom). Full detail: `worklog.md` 2026-07-26 entry,
`decisions.md` D38.

## The next job

1. **HW-PENDING: 4D Batch 1 hands-on eval on the Pico 1** (flashed
   2026-07-26, boot-verified over serial; the full checklist is in
   `worklog.md`'s HW-PENDING table). Short form:
   - RECT mode: `2i->a` stores; `a` recalls as `2i`; `a+1`, `a*a`,
     `conj(a)` work; `fac(a)` errors "Non-real variable"; REAL mode: `a`
     gives "Non-real result".
   - `{1+i,2-i}->l1`; recall `l1`; `l1+l1`, `2i*l1`, `l1/2`, `l1+l2`
     (real l2); `sum(l1)`=`3`; `stdev(l1)`/`sort_asc(l1)` error; REAL mode
     recall errors.
   - List editor: complex cells render; entering `3+2i` into a real list
     migrates it; delete/clear behave; complex l1 survives a power cycle;
     variables reset once (PCV1) then persist — incl. a complex `a`
     surviving a reboot.
   - Graph sanity: with a complex value stored in x, `Y1=sin(x)` still
     graphs (the sweep-slot exclusion).
2. **Batch 2 per the D38 plan: complex matrices (4D.25)**, ~22 h est —
   generalize `matops` (det/inverse/rref/ref/rank/augment/reshape/
   identity/power/transpose/solve_linear) to `Complex` via the
   static-row-buffer kernels; magnitude-based pivoting; matrix editor
   complex entry/display reusing 4D.24's storage tier; `eigen_core` stays
   real-input (D37). Remaining batch sequence after that (D38, risk-first):
   sequence graphing (4D.6-8, carries the single PCG5→PCG6 bump) →
   zoom/shading (4D.9-11) → data/catalog glue (4D.12/14/17/18/22) → named
   lists (4D.13) → display/formatting (4D.1-5) → eigenvectors (4D.23) →
   device polish (4D.19-20, needs the board). Task table:
   `phase4-spec.md` §8; decisions: `decisions.md` D37/D38 (the idea A-G →
   task-ID map is in D37 and
   [design-departures-matrix-complex.md](design-departures-matrix-complex.md);
   idea H — polymorphic variables — stays undecided, revisit after 4D
   ships, same checkpoint as F). **Phase 5 (CAS)** then **Phase 6
   (app framework + MicroPython)** follow 4D per D32/D33.
3. **D10 follow-ups, non-blocking** (both from 2026-07-25, no phase home):
   - **Extend the display pipeline to Pico 2** — its full-framebuffer push
     is still synchronous on core 0 (RP2350 board wasn't in hand). When it
     is: route the full-frame push through core 1 and re-verify the
     RAM-residency fix on the RP2350.
   - **Compute parallelization candidate**: the pipeline gives ~0 benefit
     on compute-bound screens (render > ~146 ms push budget; a heavy graph
     redraw measured 1.17 s). Those want `GraphScreen::recompute_function`
     (`src/apps/graph_screen.cpp:313`) parallelized — needs a second
     engine/vars context (shared `X` mutation), not just a spawned task.
4. **Low-priority, not blocking**: Pico 2 perf feel for Phase 3/4 features
   has never been re-measured against current code (only the pre-Phase-3
   2.25 baseline exists). The D35 fixes are board-generic so the Pico 2
   should already benefit, but that's unverified — it's now **two builds
   behind** (still on the Session 19 build; lacks D35, the D10 pipeline,
   and 4D Batch 1). Worth a reflash + spot-check next time it's in hand.

Mind the §8 strip-safety rule (idempotent `render()`) for any new
screens touched during the on-device passes.

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Firmware on the Pico 2 is still Session 19's font/glyph build**
  (Terminus default, `-DPICOCALC_FONT=terminus`; flashed 2026-07-21,
  boots healthy, telemetry clean over serial) — it was **not reflashed
  this session** either, so it has neither the D35 perf fixes, nor the
  2026-07-25 work (`!` factorial fix, D10 pipeline, die temp/build id),
  nor 4D Batch 1 (complex variables/lists, PCV1) that are now on the
  Pico 1; it's several builds behind. Its build layers on top
  of Sessions 11/12/15/16/17/18 (3A lists, 3B stats, 3D inference/plots,
  4A matrices/solver, 4B CALC menu, 4C complex numbers). **All of their
  hands-on on-device evals are now closed as a formality (2026-07-22)** —
  board-independent logic, and the harder rendering case (Pico 1) passed
  the identical checklists the same day (3D.14 for 11/12/15, the Phase
  4A-4C pass for 16/17/18); see `worklog.md`'s 2026-07-22 entries.
  What's genuinely still open, Pico-2-specific (not closeable by this
  reasoning): its own perf re-baseline for Phase 3/4 features, now also
  covering whether the D35 fixes help there too once it's reflashed (see
  "The next job" #3) — **deliberately deferred**, not part of this
  session's scope. Session 15's storage-health row (hot-plug/retry-forever,
  Y=-editor truncation) is now fully closed — confirmed on both boards.
  **Session 10 round 2 is now also closed (2026-07-22)**: `L` toggle
  surviving a reboot, `rand()` showing a sensible varying value, ZTrig
  short tick labels (`1.571`-style), and `F` ZoomFit auto-fit all
  confirmed on the Pico 1. The HW-PENDING table is now clear except the
  deferred Pico 2 perf re-baseline and the still-informal Session 19 font
  sweep.
- **The Pico 1 now carries 4D Batch 1** (complex variables/Ans + complex
  lists, 2026-07-26) on top of the 2026-07-25 work (factorial fix, D10
  RAM-residency fix + dual-core pipeline, die temp/build id) and the D35
  state. Flashed and boot-verified over serial (psram-bulk OK, battery +
  temp heartbeats, idle die temp 28-31 C); bss **213,332 bytes** (+1,148
  over 212,184 — the `imag[]` array + PCV1 image + editor scratch),
  ~57 KB headroom — keep watching per the D28 watch item. Phases 2-3 and
  4A-4C are HW-verified on this board. **No open bug findings; one open
  HW-PENDING row: the Batch 1 hands-on eval** ("The next job" #1).
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
  Phase 4A-4C pass, both still open: no home-screen `MatAns` token
  (editor-only, UX gap vs. scalar `ans`); fnInt shading should follow curve
  color (feature request). All logged in
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
