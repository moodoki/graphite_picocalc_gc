# Worklog

Running log of build sessions. Newest entries at the top of each section. This file is the
session-surviving source of truth for "where are we and what's next" — read it first when
resuming work. Task status checkboxes live in `docs/phases/phase1-plan.md` (and the Phase 0
checklist in `docs/phases/phase0-prep.md`); this file carries the narrative: what was done,
what was decided, what's blocked, and what to pick up next.

Conventions:

- One entry per work session or checkpoint, headed by date + summary.
- `HW-PENDING`: implemented and building, but acceptance requires real PicoCalc hardware
  that automated sessions don't have. These accumulate in the table below and get cleared
  manually by the developer.
- Decisions go to `docs/notes/decisions.md`; this log only references them.

---

## Current status

- **Phase**: 1 code complete + **first hardware bring-up done on Pico 1** (2026-07-10).
  Boots to the home screen; display and keyboard verified on real hardware.
- **Next up**: exercise the full calculator on-device (evaluate/graph/persist), get a
  FAT32 SD card in to verify persistence (boot showed sd=0), capture 5.6 graph-profiling
  numbers, then `docs/notes/phase1-retro.md`. Phase 2/3 specs are now in the tree
  (imported + reconciled 2026-07-11); phase roadmap is weeks 11–16 / 17–25 / 26–35.
- **Test-drive feedback logged 2026-07-11** (see entry below): all five actionable
  items **implemented in Session 3** (D11, D12) — on-device verification pending, see
  the polish rows in the HW-PENDING queue. KIV: F-key layout rethink (item 7).
- **Both boards build**: yes (`./scripts/build-all.sh`). Diagnostic target: `picocalc_diag`.
- **Host tests**: `./scripts/host-tests.sh` → 75 math + 21 layout = 96 checks, 0 failures

### Hardware bring-up debugging kit (learned 2026-07-10)

- Flash without touching the board: from running firmware, `stty -f /dev/cu.usbmodem* 1200`
  triggers the RP2040 1200-baud reset into BOOTSEL, then `cp build/pico/*.uf2 /Volumes/RPI-RP2/`.
- USB serial: `cat /dev/cu.usbmodem*` (pico_enable_stdio_usb is on). printf boot-tracing
  was how the boot hang was located.
- `picocalc_diag` (src/diag_main.cpp) is a vendored-only display test — the bisection tool
  that proved the panel/driver work, isolating bugs to our code.

## HW-PENDING verification queue

Firmware now boots to the **home screen**; the diagnostics screen is the F6 overlay.

**Verified on Pico 1 hardware 2026-07-10:** display (home screen renders, text + colors
correct), keyboard (keys read with correct ASCII over I2C), PSRAM word r/w, backlight,
boot to a usable home screen. Bugs found & fixed: bulk-PSRAM boot hang, dual-core
display stall, keyboard I2C timeout — all in D10.

Still to verify on hardware:

| Item | What to check on hardware |
|------|---------------------------|
| SD card (sd=0 at boot) | Insert a FAT32 card → `/picocalc` created, self-test file r/w OK, persistence works |
| 2.5 HomeScreen eval | `2+3*sin(pi/4)` ENTER → correct result; UP/DOWN walk past inputs (D12) |
| Polish: graph colors | Grid now dark gray (`kGridLine`), axes white, Y7 yellow — legible on the panel? |
| Polish: diag exit | ESC exits the F6 overlay. Also check whether a 2nd F6 press exits (original report said it didn't — HW quirk?) |
| Polish: history nav | Shift+UP/DOWN scroll the view — confirm the STM32 reports shift+arrow as arrow code with shift held (D12 revisit trigger) |
| Polish: HOME key | From graph/window/mode/diag → home screen; on home with text → cursor-to-start |
| Polish: e constant | `e` → 2.718281828; `5->E` → "E is reserved"; `2e3` → 2000 |
| F7-F10 scan codes | Press Shift+F2..F5 in the diag key echo → expect codes 0x87-0x8A (135-138); decode assumes the 0x81+n pattern |
| Store op | `2->A` then `A+1` → 3 — confirm the keyboard can type `-` and `>` |
| 3.6 Pretty math | `1/2`, `x^2`, `(1+2)/(3^4)`, `sin(x)` → stacked fractions/exponents/parens legible at 8x12 |
| 4.x Graphing | F1 Y= enter `x^2-3`, `sin(x)`; graph plots w/ axes+grid; trace, F2/F3 zoom, S/T presets |
| Graph perf (5.6) | Full replot time (firmware prints "graph recompute: N us" to serial); target <50 ms |
| 5.3 Mode/reboot | F4→MODE toggles; "Reboot to bootloader"+ENTER drops to BOOTSEL (mounts RPI-RP2) |
| 5.7 Full exit test | Power-cycle → Home → `2+3*sin(pi/4)` → F3 graph `sin(x)` trace+zoom → persistence |
| Perf latency | ~200 ms full-screen redraw per keypress (D10) — confirm acceptable or prioritize dirty-rects |

---

## 2026-07-11 — Session 3: Phase 1 polish from test-drive feedback

Implemented all five actionable items from the Session-2 feedback entry (decisions
D11, D12). Both boards build; host tests 96/96 (6 new `e` checks); clang-format run.

1. **Graph colors** — new `colors::kGridLine` rgb(60,60,60) for grid lines (axes stay
   white); Y7 palette slot dark green → yellow rgb(250,220,40).
2. **Diag screen exit** — ESC now pops the overlay (F6 already toggled); added an
   on-screen "F6 or ESC exits." hint. Removed `GraphScreen`'s unreachable F6 handler
   and fixed its softkey label (F6 said "Y=", now "DIAG").
3. **Input history (D12)** — UP/DOWN walk past inputs shell-style (in-progress line
   stashed and restored); Shift+UP/DOWN scroll the history view. Supersedes 5.5's
   UP-on-empty single recall.
4. **`e` constant (D11)** — `build_lookup()` no longer binds letter 'e', so tinyexpr's
   builtin Euler constant resolves; `->e`/`->E` store returns "E is reserved".
5. **HOME key** — global intercept in `main.cpp` (like F6): pops to the home screen
   from any screen via new `ScreenManager::pop_to_root()`; on the home screen it
   falls through to the input line's cursor-to-start.

README per-screen usage updated. On-device verification for all five queued in the
HW-PENDING table (the shift+arrow reporting is the one real HW unknown — D12 names
the fallback). KIV: F-key layout rethink (feedback item 7).

**Addendum (same day)**: developer corrected the F-key picture — F1-F5 are physical,
F6-F10 are Shift+F1-F5 *translated by the STM32 into distinct scan codes* (that's how
F6 opened diagnostics). Extended `Key` to kF10 and decode to 0x8A (0x87-0x8A assumed
from the 0x81-0x86 pattern; HW-PENDING). This translation behavior cuts both ways for
D12: the STM32 demonstrably remaps shifted non-printables, so Shift+UP/DOWN may well
arrive as something other than arrow-plus-shift-flag — verify early.

---

## 2026-07-11 — Session 2: Phase 2/3 specs imported + consistency pass

Imported the developer's drafts of `phase2-spec.md` and `phase3-spec.md` (from
`picogc_phase2_3.zip`) into `docs/phases/` and reconciled them against the Phase 1
code and the committed Phase 4 spec.

**Added**: built-in help planned into Phase 2 (new §10; tasks 2.26–2.28, ~9 hrs;
total now ~110 hrs). `HelpScreen` with Functions/Keys/Syntax tabs, entry Home F5
(unassigned in Phase 1). Function catalog driven by a `math::catalog` descriptor
table that `build_lookup()` also consumes — one source of truth so help can't drift
from the parser. Motivated by test-drive feedback item 6 (entry below).

**Consistency fixes applied to the drafts**:

- phase2 §5.3/§6.1: corrected engine claims — `eval_compiled` hardcodes X (task 2.4
  parameterizes the swept slot), and `theta` is its own variable slot (`kTheta`),
  which the polar sweep drives (draft claimed theta would be aliased to `t`).
- phase2 §8.1/§14: framebuffer clips strip-only (vertical); split-screen needs a real
  clip-rect — added to task 2.19 (draft assumed clipped rendering already sufficed).
- phase2 §12: removed unmeasured "~400K evals/sec benchmarked" figure (5.6 is still
  HW-PENDING); replaced with a pessimistic bound.
- phase2: `TableConfig` moved `apps::` → `graph::` to keep apps→graph layering;
  header/total hours fixed (~120 vs ~101 → both ~110 incl. help).
- phase3: header hours 160→170 (matches task tables); dropped "reuse Phase 1's
  numeric solver" (none exists — the solver is Phase 4 §3.4; 3C uses a local
  bisection); `extern` globals → singleton accessors per project convention; noted
  tinyexpr is fixed-arity (no default args) for parser registration (3C.7).
- phase4-spec: weeks renumbered 16–25 → 26–35 (it predates Phase 3's existence;
  Phase 3 occupies weeks 17–25).
- README Phase 2 line updated ("multi-function graphing" already shipped in Phase 1).

**Left as-is (deliberate)**: Phase 2 `GraphState` sizes slots at `kMaxExprLen` (256)
vs Phase 1's 96-char `YFunctions` — persisted struct grows to ~6.4 KB, fine; the
Matrix-vs-Array reconciliation stays deferred to Phase 4 start (phase3 §10 records it).

---

## 2026-07-11 — Test-drive feedback (on-device, Pico 1)

Developer used the calculator for a while on hardware. Observations, each diagnosed in
code this session. No fixes applied yet — several need a design decision first.

1. **Graph grid too bright.** Grid lines use `colors::kGrayLine` = rgb(200,200,200) —
   near-white on the panel (`graph_screen.cpp draw_axes`). Proposal: add a dedicated
   dark-gray `kGridLine` (~rgb 60,60,60) for the grid; axes stay white. Don't darken
   `kGrayLine` itself — it's also the history-expression text color and hint text.
   Palette on black bg: Y7 dark green rgb(0,120,0) is the dim one; consider yellow.
2. **Can't exit the F6 diagnostics screen.** F6 is a global toggle in `main.cpp` — a
   second F6 press *should* pop it. DiagScreen swallows every other key incl. ESC, so
   the natural exit key does nothing. Fix: ESC pops; draw an "F6/ESC exits" hint line.
   HW question: confirm whether the second F6 press really never arrives (STM32 quirk)
   or the tester only tried ESC. Related dead code found: `GraphScreen::on_key` has a
   kF6 case (unreachable — main intercepts F6 first) and the graph softkey bar labels
   F6 as "Y=" — mislabeled, should be DIAG.
3. **Input history navigation.** Today: UP on empty input recalls only the newest
   expression (5.5); any further UP scrolls the output view. Wanted: shell-style —
   repeated UP/DOWN walks back/forward through past inputs; a separate control scrolls
   the view. Keyboard has no PgUp/PgDn (coyote scan codes), but shift state is tracked.
   Proposal: UP/DOWN = input-history recall, Shift+UP/DOWN = scroll output.
4. **`e` is not Euler's number.** `build_lookup` binds all 26 letters as variables and
   tinyexpr checks the user lookup *before* its builtins (tinyexpr.c base()), so `e`
   resolves to variable E (0.0). `pi` works because it's two letters — never collides.
   Current convention: single letters = variables (case-folded by preprocess),
   multi-letter names = builtins + pi/theta/ans. Workaround today: `exp(1)`.
   Decision needed: reserve `e` as the constant and drop variable E (TI users rarely
   use E; recommended), vs. case-sensitivity (e=const, E=var — needs preprocess rework).
5. **Home key does nothing.** It decodes fine (0xD2 → `Key::kHome`) but only InputLine
   consumes it (cursor-to-start — invisible on an empty line; other screens ignore it).
   Proposal: global intercept in main.cpp like F6 — pop to the root (home) screen from
   anywhere; when already on Home with text in the input, keep cursor-to-start.
6. **Built-in help**: not planned in any written spec (phase 2/3 specs don't exist yet;
   phase 4 is CAS/matrix/MicroPython). Candidate for `phase2-spec.md`: a catalog/help
   screen — function list with signatures + key map. Cheap and high-value on a device
   with no manual.
7. **F-key mapping (KIV).** Corrected 2026-07-11: F1-F5 are direct physical keys;
   F6-F10 arrive via Shift+F1-F5 (the STM32 translates them to their own scan codes).
   That's exactly TI's five top-row keys on the direct layer — a TI-order remap
   (Y=/WINDOW/ZOOM/TRACE/GRAPH) with secondary functions (DIAG, HELP) on the shifted
   layer is the obvious candidate. Watch what feels natural during the next test
   drive before committing.

---

## 2026-07-10 — Session 2: first hardware bring-up (Pico 1)

First flash to a real PicoCalc. Initial symptom: screen of random colors + dead keyboard.
Diagnosed on-device by bisection (vendored-only `picocalc_diag` proved the panel works →
bug is ours) and USB-serial boot tracing. Found and fixed three bugs, all in **D10**:

1. **Boot hang** → the "random colors". `run_self_tests()` used the vendored *bulk* PSRAM
   transfer, which hangs on hardware (single-word PSRAM works). Froze after display init
   but before first draw, leaving power-on noise on the panel. Fix: word-based self-test;
   bulk `Psram::read/write` quarantined (added `read_word`/`write_word`).
2. **Dual-core display stall**. Pushing strips through a core-1 FIFO service stalled on
   frame 1. Fix: render synchronously on core 0 via the vendored blocking `spi_write_fast`
   path (the diagnostic's known-good mechanism). Core 1 idle; DMA/dual-core deferred.
3. **Dead keyboard**. I2C timeouts were 2 ms but a 2-byte read on the 10 kHz keyboard bus
   takes ~3.5 ms, so every read timed out. Fix: `kI2cTimeoutUs = 100 ms`.

Result: **boots to the home screen; display + keyboard confirmed working on hardware.**
Also made rendering event-driven (redraw only after a keypress) since a full-frame push is
~200 ms (5 fps) — removes idle redraws. Removed the debug instrumentation afterward.

Follow-ups: verify the rest of the calculator on-device (needs a FAT32 SD card for
persistence — boot showed sd=0), capture graph-profiling numbers, then the ~200 ms redraw
latency is the main perf item (dirty-rectangle partial updates — task 5.6). See the
HW-PENDING queue above.

---

## 2026-07-08 — Session 1: environment + skeleton + Phase 0 start

**Done:**

- Verified/repaired host toolchain. Quirk: Homebrew *formula* `arm-none-eabi-gcc` lacks
  newlib (`nosys.specs` link failure); the working compiler is ArmGNUToolchain 15.2.rel1
  from the `gcc-arm-embedded` cask. Documented in `docs/dev-environment.md`, AGENTS.md.
- Pico SDK 2.2.0 + pico-examples checked out in-repo (gitignored). picotool 2.3.0 via brew.
- Extracted the project skeleton package; adapted docs to this host; fixed build-dir naming
  (`build/pico`, `build/pico2`), pinned SDK 2.2.0 in CI, scoped C++-only compile flags.
- Initial commit `f76d10c`. Both boards build the blink stub to .uf2.

**Phase 0 status vs checklist:** 0.1.1–0.1.5 done (verified via hello_serial + skeleton
builds), 0.1.6 HW-PENDING, 0.2.* done, 0.4.* done, 0.5 done, 0.6.1/0.6.2/0.6.5 done.
Remaining: 0.3 (vendor drivers), 0.6.3 (optional CLAUDE.local.md — skipped, developer's
call), 0.7 (remote push — no remote configured yet).

**Next:** task 0.3 — clone Coyote OS, vendor `lcdspi/ i2ckbd/ rp2040-psram/ pwm_sound/`,
vendor FatFs R0.15a, record SHAs + licenses.

### Checkpoint: Phase 0 complete (2026-07-08)

- Vendored from Coyote OS `e86cf36d` (2026-02-05): `lcdspi/` (incl. font1/battery fonts),
  `i2ckbd/`, `rp2040-psram/` (MIT, upstream polpo; examples dropped), `pwm_sound/`, plus
  `coyote_reference/` (config.h SD pinout, keyboard_definition.h scan codes — reference only).
  GPL-2.0 text kept as `drivers/LICENSE.coyote-os`; noted GPL implication in dependencies.md.
- Vendored FatFs R0.15a from elm-chan.org (`ff.c`, `ffsystem.c`, `ffunicode.c`, stub
  `diskio.c`; SD SPI glue to be written in task 1.5).
- Phase 0 checklist all [x] except 0.1.6 (HW-PENDING) and 0.6.3/0.7 ([s], developer's call).
- Not yet in build: drivers are intentionally NOT in CMakeLists.txt (per 0.3.4) —
  integrated incrementally in tasks 1.3–1.6.
- Useful discovery: Coyote OS uses tinyexpr (C) and pico-vfs as submodules; we'll use
  tinyexpr++ per spec (task 2.1) and plain FatFs instead of pico-vfs.

### Checkpoint: Milestone 1 (bootstrap) code complete (2026-07-08)

Tasks 1.1–1.9 all [x] in phase1-plan.md; both boards build the diagnostics firmware.
Decisions D6–D9 recorded (RGB666 wire format, async keyboard, FatFs LFN, interim font).

Layer map as built:
- `src/platform/`: display (565→666 push, DMA), keyboard (async poll SM), sd_card
  (own SD SPI driver) + sd_diskio (FatFs glue) + storage (FatFs API), psram (bump
  allocator over PSRAM addresses), system (battery via STM32), platform::init().
- `src/gfx/`: framebuffer (strip ping-pong on Pico 1 / full FB on Pico 2, clipped
  primitives, core-1 display service over multicore FIFO), font (UTFT-format).
- `src/ui/`: Screen base + fixed-depth ScreenManager.
- `src/main.cpp`: core dispatch + DiagScreen (self-tests for SD/PSRAM, key echo).

Notes / known limitations:
- **Full-frame push is ~98 ms @ 25 MHz SPI** (3 B/px wire format) → ~10 fps if the
  whole screen redraws every frame. Fine for milestone-1 accept ("text visible"),
  but the spec's 30 fps target needs dirty-rect updates and/or SPI overclock —
  planned lever for task 5.6. Milestone 2+ UI should avoid full-screen redraws.
- Overclock constant (`config::kOverclockHz`) intentionally NOT applied yet.
- `set_backlight` uses STM32 reg 0x05 (standard PicoCalc fw) — not in the vendored
  driver, needs HW confirmation.
- lint.sh not run: clang-tidy unavailable (Homebrew `llvm` not installed — ~1.5 GB;
  developer's call). clang-format installed (v22) and applied; line width 100.
- Vendored driver C files emit warnings under our -Wall/-Wextra/-Wpedantic (they
  compile as part of our target via INTERFACE libs). Cosmetic; suppress later if
  it drowns signal.

### Checkpoint: Milestone 2 (calculator core) code complete (2026-07-08)

Tasks 2.1–2.8 all [x]; both boards build the calculator firmware; 53/53 host tests pass.
Decisions D1 (store op `->`) and D4 (plaintext history) recorded.

What's new:
- `src/math/`: `Engine` (tinyexpr wrapper + `->` store op + `!`→`fac()` preprocess),
  `functions` (angle-aware trig, ln/log10, nCr/nPr, fac, rand, round, min/max, deg/rad),
  `format_number` (int / 10-sig-fig / scientific), `types` (calc_t=double, AngleMode).
- `src/ui/input_line`: cursor editing (insert/backspace/del/home, horizontal scroll).
- `src/apps/home_screen`: input line + 50-entry ring-buffer history (right-aligned
  results, pretty via format_number), UP/DOWN scroll, F4 angle toggle, SD persistence
  of history (TSV) and variables (binary). main.cpp now boots to HomeScreen; F6 opens
  the milestone-1 diagnostics overlay.

Testing approach (NEW — matters for session continuity):
- `tests/host/test_math.cpp` + `scripts/host-tests.sh` compile the math layer with the
  **host** compiler and assert real values (2.1–2.4, 2.6 acceptance criteria). This is
  how we verify calculator correctness without a PicoCalc. Extend this suite as the
  math/renderer grows. Cross-compile + host-test + doc-update is now the per-milestone
  loop.

Decision worth remembering: used **C tinyexpr** (not tinyexpr++) — see dependencies.md
for rationale (C ABI fits -fno-exceptions/-fno-rtti; extended fns live in our C++).

Known limitations / deferred:
- Results in history are plain text (format_number), NOT yet 2D-typeset — that's
  milestone 3 (the renderer replaces the result string with a layout tree).
- HomeScreen redraws the full frame each key/frame (~10 fps ceiling, see M1 note).
  Acceptable for text entry; revisit with dirty-rects if it feels laggy on HW.
- Softkey bar is a static label placeholder; real softkey dispatch is task 5.2.
- rand() uses libc rand() unseeded — deterministic across boots. Seed from an ADC
  noise source or uptime at first use during polish.

### Checkpoint: Milestone 3 (natural math renderer) code complete (2026-07-08)

Tasks 3.1–3.7 all [x]; both boards build; 74/74 host tests pass (53 math + 21 layout).
Decision D2 (simple-operand fraction heuristic) recorded.

Design:
- `src/render/layout_node.hpp`: one fixed-size tagged-union `LayoutNode`
  (Text/HBox/Fraction/Superscript/Paren). **No virtual functions** — a plain
  `NodeType` tag + switch, so it's RTTI-free (-fno-rtti) and pool-friendly.
- `src/render/pool.{hpp,cpp}`: 8 KB bump allocator, `pool_new<T>()` placement-new,
  reset per build. On exhaustion, builders degrade to plain text (never crash).
- `src/render/layout_builder.cpp`: recursive-descent parser producing sized nodes.
  Grammar: expr(+/-) → term(*,/) → power(^ right-assoc) → unary(-) → atom
  (number | ident | ident(args) | (expr)). Fractions only for simple operands (D2).
- `src/render/layout_render.cpp`: `render_node()` walks the tree; strip-clipped;
  stroked auto-scaling parens for tall content.
- HomeScreen history now renders **expressions** as 2D math (`render_node`); results
  stay plain text (a formatted number is already display-ready, and the layout parser
  would choke on result annotations like "5>A" / error strings).

Testing: NEW `tests/host/test_layout.cpp` asserts node types + sizes for the Phase 1
constructs (fractions, superscripts, parens, function calls, nesting, right-assoc `^`).
This is how the renderer is verified without a screen; the visual/alignment quality
still needs the HW-PENDING check.

Known limitations / deferred:
- Single font size → superscripts are same-size-but-raised, not 75%-shrunk. Fine at
  8x12; revisit if a smaller font is added (would also help nested exponents).
- Fraction vertical centering is approximate (bar sits at the math baseline; adjacent
  inline text aligns to the bar, so it reads slightly "numerator-high"). Cosmetic;
  tune after seeing it on HW.
- SqrtNode deferred to Phase 2 per spec (sqrt renders as "sqrt(x)" text for now).
- Rebuilding history trees every strip (~20x/frame) is wasteful but cheap for short
  strings; optimize with dirty-rects / cached measurement in task 5.6 if needed.

### Checkpoint: Milestone 4 (graphing) code complete (2026-07-08)

Tasks 4.1-4.9 all [x]; both boards build; 81/81 host tests pass. Decisions D3 (trace
readout at bottom) and D5 (keep double, float deferred) recorded. **Phase 1 is now
feature-complete** — only milestone 5 (polish) and hardware verification remain.

New:
- `math::Engine` gained a compile-once/eval-many path (`compile` / `eval_compiled` /
  `free_compiled`) so graphing does 320 evals per function, not 320 re-parses. Shared
  `build_lookup()` helper. GraphScreen saves/restores the user's X around the sweep.
- `src/apps/graph_model`: Y1..Y7 + GraphWindow singletons, 7-color palette, SD
  persistence (yfuncs.txt TSV, window.dat binary), zoom presets/ops.
- `src/apps/y_editor`: Y= list editor (navigate, inline edit via InputLine, enable
  checkbox, clear, jump to graph).
- `src/apps/graph_screen`: column-cached plotting (recompute on dirty), axes+grid,
  discontinuity detection, trace cursor, zoom. Softkeys F1 trace / F2,F3 zoom /
  F5 Y=; keys S,T = standard/trig presets.
- `src/apps/window_screen`: 6-field editor; ESC replots and returns to graph.
- HomeScreen softkeys now F1=Y=, F2=WINDOW, F3=GRAPH, F4=MODE; main loads graph
  state at boot.

Host tests: added compile/eval_compiled coverage to test_math (60 checks now).
The plotting/axes/trace geometry is not host-tested (needs the framebuffer) — that's
the largest HW-PENDING surface this milestone.

Known limitations / deferred:
- **ZoomFit (F4 in spec) not implemented** — needs a y-range auto-scan over enabled
  functions. Left as a small follow-up; F2/F3/S/T cover the common cases.
- **Axis tick numeric labels not drawn** (grid lines are). Deferred to M5 polish;
  needs careful label placement to avoid clutter at 8x12.
- Graph coordinate coloring/labels and the "no functions" hint are basic; refine in M5.
- Trace steps by pixel column (reads the cache), not by evaluating at sub-pixel x —
  fine for a 320px viewport.

### Checkpoint: Milestone 5 (polish) code complete — PHASE 1 CODE COMPLETE (2026-07-08)

Tasks 5.1-5.5, 5.8, 5.9 [x]; 5.6 [~] (profiling hook in, numbers need HW); 5.7 [!]
(HW test, no PicoCalc attached). Both boards build; 90/90 host tests pass.

New:
- `src/ui/chrome`: shared `draw_status_bar` (title + RAD/DEG + FLT/FIX/SCI + 2nd/A
  indicators) and `draw_softkeys` (6 cells). HomeScreen/GraphScreen/ModeScreen use them.
- `src/apps/mode_screen`: angle mode, display format (FLOAT/FIX/SCI), fix digits, and
  "Reboot to bootloader" → `reset_usb_boot(0,0)` for flashing without the BOOTSEL button
  (task 5.8). Reached via Home F4.
- `math::format_number` gained FIX/SCI modes (global `DisplayMode` + fix digits),
  host-tested.
- Expression recall: UP on an empty Home input line restores the last expression (5.5).
- Graph `recompute()` times itself and prints "graph recompute: N us" to USB serial +
  stores `last_recompute_us_` (5.6 profiling hook).
- Error handling verified by host tests: 1/0→Inf, 0/0→NaN, syntax→"Syntax error"
  (shown in red), no crashes (5.4).
- README rewritten: feature list, host-tests section, per-screen usage, flash-from-
  firmware note.

**Phase 1 is code-complete.** The only remaining work is on real hardware (see the
HW-PENDING queue above): boot both boards, run the exit test, confirm SD persistence,
record graph-render timing, then write `docs/notes/phase1-retro.md` and begin
`docs/phases/phase2-spec.md`.

Deferred within M5:
- clang-tidy not run (Homebrew `llvm`/`clang-tidy` not installed — ~1.5 GB; developer's
  call). clang-format IS applied repo-wide each checkpoint.
- ZoomFit (4.7) and axis numeric tick labels (4.4) still deferred from M4.
- 2nd/Alpha status indicators are plumbed through `StatusFlags` but not yet driven by a
  real 2nd/Alpha key mode (the STM32 reports ASCII directly). Wire when those modes land.
