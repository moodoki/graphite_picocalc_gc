# Phase 2 test drive (task 2.24) — observations

Session started 2026-07-17, Pico 2 (RP2350) unit. Firmware: `picocalc_graphcalc.uf2`
built from HEAD (b6cda7b), flashed via 1200-baud reset. Serial capture running on
the host (scratchpad `testdrive-serial.log`). **Fix-nothing rule in effect: record
only, fixes start when the developer says so.**

## Findings so far (host-side analysis, pre-checklist)

### 1. PSRAM/SD self-test FAIL sticks after D14 rail-settle window — firmware bug

- Observed: F6 diag screen showed `PSRAM: FAIL (readback mismatch)` and SD card
  failure after (presumed cold) boot. After a warm reboot (`picotool reboot -f`),
  **both show OK** — hardware is fine.
- Cause: the late-init retry loop (`src/main.cpp:203`) only re-runs
  `run_self_tests()` when a subsystem whose *init failed* comes up late. Two gaps
  on a Pico 2 cold boot (D14, ~5–8 s rail settle):
  1. Boot init "passes" in the marginal window but the fuller 64-word readback
     test fails → nothing marked down → never retested → FAIL frozen on screen.
  2. Init fails, late-init `reinit()` succeeds while rails still marginal, the
     one-shot retest fails → status flag now true → no further retries.
- Proposed fix (NOT applied): keep re-running failing self-tests inside the 30 s
  late-init window, not just on init-state transitions.

### 2. Charging flag never reads true — decode bug suspected (KIV item confirmed live)

- Observed: unit plugged in via USB, `(charging)` never shown. This is the
  queued worklog retest ("charging color once battery <95%").
- `src/platform/system.cpp:62` decodes charging from bit 7 of the **low** byte
  (`raw & 0x80`) — flagged in Session 6 worklog as an *assumption*. The low byte
  is most likely the STM32's echoed register ID (0x0B, bit 7 always 0), which
  would make charging permanently false. Percentage demonstrably lives in the
  high byte, so the charging flag is most likely bit 7 of the same value byte,
  i.e. `raw & 0x8000` — currently masked off by `& 0x7F`.
- Battery % at time of test: not yet recorded — if >=95% the charger may be idle
  and the test is inconclusive (same as Session 6).
- Proposed fix (NOT applied): `info.charging = (raw & 0x8000) != 0;` plus a
  one-time serial print of the raw register value to confirm the byte layout.

### 3. Serial capture: boot printfs unrecoverable over USB

- Boot-time prints race USB enumeration (known); two reboot cycles captured
  nothing at boot. No boot-print buffering exists in the firmware. Runtime
  prints (key echo, graph recompute timings) capture fine.

## Developer observations during the drive

(recorded as reported)

- **2026-07-18** (after the warm reboot that cleared the stuck FAILs):
  - Parametric mode (step 3): looks right.
  - Polar mode (step 4): looks right.
  - Tables (step 5): auto table scrolls infinitely. (ASK mode, setup
    edits, LEFT/RIGHT column scroll, scroll latency: not yet reported.)
  - Split-screen (step 6): works.
  - Help / keymap (steps 7–8): help shows graph F4 → table and
    table F4 → graph; bindings behave accordingly.
  - Tables, continued (step 5): LEFT/RIGHT column scroll works; setup
    (Start/Step) configuration works. **Scroll latency: a little laggy
    when arrow keys are held, and scrolling continues for a few more
    rows after key release. "Not ideal, but still acceptable."**
    - Analysis note: the after-release overrun points at key events
      queueing faster than the per-row recompute+render drains them
      (backlog in the STM32 FIFO / poll loop), so the fix lever is the
      known 2.25 one — per-row compile-once-per-regenerate — and/or
      draining/coalescing repeat events per frame. Not fixed (per
      fix-nothing rule).
  - Table ASK mode (step 5): works — AUTO/ASK toggle via F1 setup,
    ENTER adds values, F5 deletes. (Empty-state hint not explicitly
    reported.)

## UI oddities reported (feeds the F-key layout rethink, feedback item 7)

All three verified against the code; none fixed (fix-nothing rule).
Any change here obligates updating the help KEYS tab content (2.27).

1. **Home F1 always opens the Y= editor** (`home_screen.cpp:209`),
   regardless of graph mode. Feels wrong in PARAM/POLAR — should be
   mode-dependent, like graph F5 already is
   (`graph_screen.cpp:396-403` dispatches to param/polar/Y= editor by
   mode).
2. **Graph is F3 on home but F4 everywhere else.** Home F3 → graph
   (`home_screen.cpp:215`); table F4 → graph, graph F4 → table (D16
   scheme). Inconsistent muscle memory between screens.
3. **No WINDOW access from the graph screen.** Graph binds F1 trace,
   F2/F3 zoom, F4 table, F5 editor, F9 split — window settings are
   only reachable via home F2, so tuning the window means exiting the
   graph and coming back. Clunky.

## Improvement request: expression input for window/setup params

- WINDOW fields (and table setup Start/Step) parse input with plain
  `std::strtod` (`window_screen.cpp:81`, `table_setup.cpp:43`), so
  `2*pi`, `pi/10`, etc. silently truncate (strtod stops at `*`,
  `2*pi` → 2.0). Expressions would be genuinely useful here — theta/T
  ranges are naturally multiples of pi (and the endpoint-gap finding
  above shows step values like 2pi/64 matter).
- Proposed (NOT applied): run the field text through
  `math::engine()` eval on commit (the engine is already linked and
  used everywhere else); reject on parse error instead of silently
  taking the strtod prefix.
- Developer refinement (2026-07-18): applies to *all* numeric entry
  fields, not just WINDOW (e.g. table setup too). Desired behavior:
  evaluate the expression immediately on commit and **replace the
  field's displayed text with the evaluated numeric value** (type
  `pi/180`, field then shows `0.0174533`).

## Cold power cycle (step 2, partial) — 2026-07-18 ~05:52

- Battery removed for a few seconds, then powered on. **PSRAM and SD
  both came up OK on this cold boot** — the D14 marginal window didn't
  bite this time (timing-dependent; a fully-discharged rail may boot
  cleaner than a quick cycle). The stuck-FAIL retry gap found earlier
  remains real but reproduces only on an unlucky boot.
- Serial log: silent across the cycle — **expected**, because the
  firmware has exactly two printf sites (graph recompute timing,
  diag key echo). **Instrumentation gap:** the boot / D14 late-init /
  persistence-load path has no logging at all, so next-session.md's
  "watch serial for the late-mount + load" is not currently
  observable. Worth adding LOG_DEBUG lines when fixes open up.
- Still to confirm for step 2: persisted state fully restored
  (curves, mode, T/TH ranges, table config, Y= funcs); battery % and
  charging flag on USB.

## Further checklist results (2026-07-18, after cold cycle)

- **Step 3 parametric editor details: PASS** (auto-focus, pair
  checkbox/auto-enable, UP/DOWN pair switching — developer confirms
  step 3 done).
- **Step 7 help: PASS** ("help works").
- **Step 8 MODE persistence: PARTIAL — graph mode persists across
  reboot, DEG/RAD does NOT.** Confirmed in code: angle mode
  (`math::angle_mode()`) is not part of the persisted graph state
  (nothing in graph_state.hpp / save path), so it resets to RADIAN
  every boot while FUNC/PARAM/POLAR rides in graphstate.dat. Gap to
  fix: include angle mode in persistence.
- **Split-screen trace entry: BUG/UX gap.** Trace *sync* works, but
  trace can't be activated from within split view — only if trace was
  already on before entering split. Code inspection says F1 *is*
  forwarded to the focused graph pane (`split_screen.cpp` on_key
  default → pane dispatch) and graph F1 toggles trace; suspected
  dirty/invalidate gap — graph F1 sets no dirty flag, and the split
  screen may not repaint on the forwarded toggle, making it invisible.
  Probe: toggle F1 in split then press an arrow — if the cursor
  appears, it's purely a redraw bug. Root-cause when fixes open.
- **Step 1 regression parity: PASS** ("feels ok" — function graphing,
  Y= editor, home basics).
- **Step 5 leftovers: ALL PASS** (2026-07-18) — ASK empty-state hint,
  F2 jumps straight to Step in setup, detail line full precision,
  `<`/`>` column markers + scrolling with 4 enabled functions.
- **Step 2: effectively PASS.** Post-cold-cycle testing proceeded on
  the restored state (graph mode persisted; curves/ranges/table config
  in use without loss reported). Known exception: DEG/RAD resets
  (gap logged above). Migration-artifact check on SD (graphstate.dat
  present, old files ignored) not directly verifiable on-device — see
  the SD-listing feature request.
- **Pico 2 test drive (task 2.24) COMPLETE** except items explicitly
  deferred below.
- **Deferred to next session:** battery %/charging-bit check (needs
  battery <95% for a conclusive read; decode fix candidate
  `raw & 0x8000` already queued) and the **entire Pico 1 pass**
  (reflash `build/pico/…uf2` → RPI-RP2, functional sweep, split-pane
  clipping on the strip renderer).
- **Step 6 split-screen: PASS** (developer closes the step; the
  trace-entry-from-split gap above remains the one open issue).

## Improvement request: SD card contents listing on-device

- There is no way to list the SD card's contents from the calculator.
  Requested as a new function (would also have made the step-2
  migration check — graphstate.dat present, old yfuncs.txt/window.dat
  ignored-but-retained — verifiable without pulling the card).
- Natural home: a FILES tab/screen (e.g. under diag or a new
  screen) listing /picocalc/*; storage HAL would need a dir-iterate
  wrapper (FatFs f_opendir/f_readdir via platform::storage()).
- Not implemented (fix-nothing rule).

## Table screen + navigation-stack findings (reported on device)

1. **Table softkey bar style inconsistency (cosmetic):** the table
   screen draws a plain hint string ("F1:SETUP F2:STEP F3:GRAPH …") on
   a gray band instead of `ui::draw_softkeys`' divided cells — no
   vertical separators like every other screen.
2. **Table F3 and F4 both go to graph** (`table_screen.cpp:231-234`,
   one shared `pop()` case, together with ESC). The footer advertises
   F3:GRAPH while the D16 scheme and help say F4. Harmless redundancy,
   but the labels disagree with the keymap story.
3. **"Back to graph" wording is wrong, behavior is right:** ESC (and
   table F4) actually pop to the *previous* view, whichever it was —
   developer explicitly endorses the pop behavior ("good and
   intuitive") and wants the on-screen/help wording fixed to match
   (e.g. help_screen.cpp:51 "F4 back to graph").
4. **BUG — screen stack fills up; F4 toggling then stops working.**
   Root cause: `slot_editor.cpp:89` — F4 in the Y=/param/polar editor
   *pushes* a fresh graph_screen entry even when the editor was itself
   pushed from the graph screen, leaking one stack slot per editor→
   graph jump. `ScreenManager::kMaxDepth` is 8 and `push()` silently
   no-ops when full (`screen_manager.cpp:18-20`) — so once the stack
   tops out, every push-based key (graph F4→table, F1 setup, F9
   split…) dies silently until ESCs drain the stack. Matches the
   observed "stops working, ESC-ing through fixes it."
   Graph F4→table (push) / table F4 (pop) is itself balanced — the
   editor jump is the leak.
   - Proposed fix (NOT applied): view *toggles* must not stack — when
     the target screen is directly beneath, pop instead of push (or
     use `ScreenManager::replace`); apply the rule to editor F4 and
     any future cross-jumps. Optionally make a refused push visible in
     debug builds instead of silent.

## Improvement request: surface syntax errors in graph editors

- Applies to all three editors (Y=, parametric, polar). A function
  that fails to compile is silently skipped at recompute time
  (`graph_screen.cpp:154-158` and the param/polar equivalents set the
  slot inactive when `compile()` returns nullptr) — the user only
  finds out when nothing plots.
- Request: highlight the offending row in the editor itself (e.g. red
  row text/background) when its expression doesn't compile, instead
  of requiring a round-trip to the graph screen to notice. Validation
  can run on row commit (engine compile + free, same as recompute).
- Not implemented (fix-nothing rule).

## Home screen status bar: overdraw + staleness (reported on device)

Two distinct mechanisms, both confirmed in code (`home_screen.cpp`):

1. **History can draw over the status bar.** The history loop
   (`home_screen.cpp:247`) checks `y > kStatusH + lh` *before* laying
   out an entry, but a tall pretty-printed expression (fraction etc.)
   then subtracts its full layout height and renders at the resulting
   y — which can land inside (or above) the status-bar rows. The guard
   runs before the entry's height is known, so tall entries near the
   top corrupt the bar in the framebuffer itself.
2. **Battery/status staleness.** Rendering is event-driven (redraw
   only after a keypress — `main.cpp` dirty flag), and typing only
   invalidates the input band; only Enter's band includes the status
   bar (comment at `home_screen.cpp:32-34`). So even though
   `battery_status()` refreshes its cache every 30 s, the on-screen
   battery/charging indicator only updates on an evaluation or a
   full-screen invalidate — it can sit stale (and overdrawn, per #1)
   indefinitely while idle or during typing/history scrolling.

Proposed fixes (NOT applied, fix-nothing rule):
- Clip/skip history entries whose layout would cross kStatusH (check
  after `build_layout`, before render).
- Repaint the status band when the battery cache value actually
  changes: cheap timer check in the main loop that invalidates just
  rows 0..kStatusH and sets dirty (keeps the event-driven model; a
  16-row push is far below the 200 ms full-frame cost).

## 2.25 perf baseline — `graph recompute` capture (µs)

Captured over USB serial during the drive above (74 samples, in order).
Developer-reported sequence: circle → Lissajous → cardioid → rose →
split. Labels below follow that order; clusters marked (?) are
interpolated, not confirmed:

| cluster | samples | range (µs) | attribution |
|---|---|---|---|
| 1 | 16 | 4949–5197 | function mode, persisted Y-funcs (pre-parametric views/zooms) (?) |
| 2 | 1 | 22 | empty parametric set (just after switching to PARAM) |
| 3 | 10 | 931–1048 | parametric circle: X1T=cos(t), Y1T=sin(t) |
| 4 | 10 | 199–279 (one 1008 outlier) | circle with coarser Tstep or incomplete pair during editing (?) |
| 5 | 19 | 1317–1415 | Lissajous: cos(3t)/sin(2t) |
| 6 | 8 | 5189–5320 | polar, cardioid + rose enabled together, RADIAN (confirmed) |
| 7 | 1 | 17 | empty/cleared set at a mode or curve transition |
| 8 | 7 | 1971–2077 | polar, single curve after clearing — which of the two unconfirmed |
| 9 | 2 | 4019–4105 | split-screen (graph pane + table regenerate) |

Headline baseline numbers for 2.25: parametric single pair ≈ 1.0–1.4 ms;
function set (multiple Y-funcs) ≈ 5.0 ms; polar single curve ≈ 2.0 ms,
two curves ≈ 5.3 ms; split ≈ 4.1 ms per recompute. All are far below the
~200 ms full-frame push — recompute is not the perceived-latency
bottleneck anywhere.

**Checklist gap surfaced by this attribution:** all polar plots so far
were in RADIAN. The DEGREE-mode polar test (THmax=360, THstep≈5.7 →
same shapes; trace readout th in degrees) was run later — see below.

## DEGREE-mode polar (step 4, second half) — mostly passes, one anomaly

- Same shapes as radian mode; trace readout shows th in degrees. PASS.
- **Anomaly: cardioid missing the final arc, roughly the last 10° up
  to 360°.** Not yet explained by code inspection:
  `PolarSource` (`src/graph/polar_source.cpp:19`) computes
  `steps_ = floor(range/step + 1e-9)` and iterates the endpoint
  inclusively; the draw loop plots every cached point. With
  0/360/5.7 the last point lands at th=359.1 — max possible sweep gap
  is one step (5.7°), typically 0.9°. A ~10° gap (≈2 steps) needs
  another mechanism. Candidate: render-side clipping at the right
  edge (near th=360 the cardioid hugs x≈2, the extreme right tip;
  `clamp_px` clamps rather than culls, and split-pane clipping was
  also in play this session).
- **RESOLVED (root cause understood, not fixed):** setting WINDOW to
  THmin=0 / THmax=360 / THstep=5.7 makes the missing arc go away.
  Mechanism: the sweep emits points only at th = min + i*step with
  i <= floor(range/step), so when step doesn't divide the range the
  final partial step is dropped and the curve stops up to one full
  step short of THmax; coarse chords make the visual gap look bigger
  (a ~5.7° step reads as "~10° missing"). With 5.7 the residual gap is
  0.9° — below visibility, hence "fixed", though not actually closed.
- Prediction from the same math: the **radian defaults have the same
  gap** — THmax=2pi, THstep=0.05 (graph_state.hpp:73) → last sample at
  6.25 rad, ~1.9° short of closing the cardioid. Small enough that it
  went unnoticed in the radian test.
- Also noted while inspecting: **theta window values are not converted
  on angle-mode switch** — the radian-flavored defaults (max 6.283,
  step 0.05) are meaningless in DEGREE mode (a 6°-wide sliver) until
  manually re-entered. The checklist scripts the manual entry, but as
  UX this pairs with the window-access papercut above.
- Proposed fix (NOT applied): when the last step overshoots, emit one
  final clamped sample exactly at THmax (same for parametric t_max) so
  closed curves close regardless of step divisibility.

Raw log: `docs/notes/testdrive-serial-2026-07-18.txt` (copied from the
host capture; includes attach/detach markers around the cold cycle).
