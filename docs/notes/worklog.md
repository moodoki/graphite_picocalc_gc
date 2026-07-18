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

- **Phase**: 1 code complete; **Pico 1 fully HW-verified** (2026-07-11) and **Pico 2
  brought up** (2026-07-11/12): full-framebuffer display path works, cold-boot
  PSRAM/SD failure root-caused to a ~5-8 s peripheral rail settle and fixed with
  deferred late-init (D14, verified on a cold power-on). STM32 keyboard fw v1.6;
  battery, dirty-band rendering (D13), SD persistence, store op, pretty math (D2
  revised), trace/presets, mode, bootloader reboot all verified on Pico 1.
- **Open HW items (small)**: none on the Pico 2 — charging decode verified
  2026-07-18 (Session 9); the Session 8 test drive covered the functional
  spot-check. Remaining HW work is the deferred Pico 1 pass (D18).
- **Phase 1 declared done 2026-07-12** (retro: `docs/notes/phase1-retro.md`).
- **Phase 2 started 2026-07-12** (Session 7): lint baseline is clean and gating
  (clang-tidy installed + config fixed, `WarningsAsErrors: '*'`), and **task 2.1 is
  done** — `src/graph/` now holds `Viewport` + `Plotter`/`PointSource`, GraphScreen
  routes through them, behavior-preserving (viewport formulas locked by host test).
- **Tasks 2.2–2.4 also done** (same session): `graph::Mode` + `GraphState`
  (apps::graph_model delegates into `graph::state()`); `FunctionSource` feeds
  GraphScreen's recompute; `Engine::eval_compiled` got a slot-indexed overload
  and `ParametricSource` sweeps t (host-tested with a unit circle).
- **Tasks 2.5–2.7 done** (same session): editors per D15 (`SlotEditorScreen`
  base + Y=/parametric subclasses); mode-aware WindowScreen (Tmin/Tmax/Tstep
  rows); parametric plotting with a 340-point/pair pixel cache + mode-aware
  `graph::TraceCursor` (the §14 trace extraction). Graph F5 routes to the
  right editor per mode. **The whole parametric path is code-complete but
  unreachable until the mode selector (2.22) and unpersisted until 2.23.**
- **Minimal 2.22 pull also done**: MODE screen gained a "Graph mode" row
  (FUNC<->PARAM) so the parametric path is reachable on-device; polar joins the
  cycle with 2.8–2.11.
- **Built-in help done (2.26–2.28, pulled from week 16)**: `math::catalog`
  drives both `build_lookup` and the FUNC tab; `HelpScreen` (FUNC/KEYS/SYNTAX)
  on Home F5. Addresses the 07-11 test-drive discoverability pain (§10).
- **Polar week done (2.8–2.11)**: `PolarSource` (theta-slot sweep, angle-mode
  aware conversion), `PolarEditorScreen` (D15 subclass), THmin/THmax/THstep
  window rows, GraphScreen polar branch sharing the parametric point cache,
  MODE cycles FUNC/PARAM/POLAR. **All of weeks 11–13 is code-complete.**
- **2.23 done (pulled from week 16)**: unified `graphstate.dat` (magic-tagged
  binary image) persists everything — mode, all three modes' slots, window,
  t/theta ranges, table config; one-time migration from yfuncs.txt/window.dat;
  parametric/polar editors + mode row now save. **No unpersisted state left.**
- **Table view done (2.12–2.18)**: host-testable `table_model`
  (mode-aware columns + `evaluate_table_row`), `TableSetupScreen`
  (Start/Step/AUTO-ASK, persists via 2.23), `TableScreen` (auto infinite
  scroll, ask accumulation, horizontal column scroll, cached visible
  window). Entry: Graph F4 "TBL".
- **Split-screen done (2.19–2.21, D16)**: framebuffer pane clip rect;
  horizontal split reusing the live graph/table singletons with runtime
  pane geometry; nearest-row trace sync (all modes); F4 = pane focus,
  F9 = split toggle. Spec §13 open questions P2-1..P2-6 all resolved.
- **2.24 test drive DONE on Pico 2 (Session 8, 2026-07-17/18)**: all
  checklist steps pass; found-bug fixes + quick features implemented,
  built, linted, flashed same session (see Session 8 entry). 2.25 perf
  baseline captured (`docs/notes/testdrive-phase2-observations.md`).
- **Session 9 (2026-07-18)**: all ten Session 8 fixes verified on the
  Pico 2 (offline spin passed 8/10; items 8+9 re-root-caused, re-fixed,
  re-verified same session — see Session 9 entry). Charging-bit decode
  confirmed. Six-item usage-feedback batch shipped and HW-verified
  (D19 case-sensitive input, D20 keymap + typed commands, graph chrome,
  square ZStandard, DEL/SPACE). The full Pico 1 pass is **deferred to
  post-Phase 3** (D18) — folds into task 3D.14 to save a board swap.
- **Phase 2 declared done 2026-07-18** (retro:
  `docs/notes/phase2-retro.md`). Close-out: 2.24 closed on the Pico 2
  passes (Pico 1 leg → 3D.14 per D18); 2.25 closed on the baseline —
  recompute ≪ frame push, the scroll symptom was event backlog (fixed),
  compile-once lever not needed; 2.22 closed — MODE row + D20 made mode
  fully integrated (F3 everywhere, mode-aware F1/WINDOW/labels).
- **Session 10 (2026-07-18)**: pre-Phase-3 deferred-item batch, all four
  items code-complete, lint-clean, flashed to the Pico 2 (HW eval
  pending): **D9 done** — Spleen 8x16 main + 5x8 small font (BSD-2,
  `drivers/spleen/`, `scripts/bdf_to_utft.py` converter), Coyote `font1`
  no longer compiled (D17 step 3, NOTICE updated); **rand() seeded**
  (xorshift64* in math::fn, `get_rand_64()` at boot, host-deterministic);
  **ZoomFit** (`F` on graph); **numeric axis tick labels** in the small
  font (`L` toggles — evaluation feature).
- **Session 10 later rounds (same day)**: round-1 eval passed (labels
  kept → persisted, PCG3); **D10 bulk PSRAM root-caused, fixed,
  HW-verified** (~6.8 MB/s); **D21 amended — Array cap 10000 with the
  PSRAM tier**; D14 parked as the NEXT BENCH SESSION (non-blocking,
  scope plan + schematic findings in next-session.md).
- **Session 11 (2026-07-19): Phase 3 sub-phase 3A code-complete and
  flashed** — `Array` (dtype-tagged, SRAM slab / PSRAM region tiers per
  D21) + `ArrayStore`, `ListStore` l1-l6 with `lists.dat` persistence
  (+ `Storage::read_file_range`), list ops incl. external merge sort
  for PSRAM-tier lists, the `math::listexpr` home-screen layer
  (literals, `->lk` store, reductions, vector lift — see **D22**), and
  the list editor screen (typed `lists` command). 106 new host checks;
  lint clean; **Pico 2 flashed, boot verified over serial (HW eval
  pending — see HW-PENDING)**.
- **Next up**: on-device eval of the 3A batch, then sub-phase 3B
  (descriptive stats + regression, spec §4) — 1-var/2-var stats are the
  first consumers of `Array`. Note the §8 strip-safety rule and the
  3D.14 combined Pico 1 pass.
- KIV: F-key layout rethink (feedback item 7) — Session 8 shipped the
  uncontroversial part (home F1 mode-dependent); F3/F4 consistency and
  WINDOW-from-graph still open, help KEYS must move with them.
- **Both boards build**: yes (`./scripts/build-all.sh`). Diagnostic target: `picocalc_diag`.
- **Host tests**: `./scripts/host-tests.sh` → 136 math + 37 layout + 72 graph + 106 lists = 351 checks, 0 failures

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

**Verified on Pico 1 hardware 2026-07-11 (round 1, Session 5):** evaluation + history,
input-history walk (UP/DOWN) and Alt/Ctrl+UP/DOWN view scroll (D12 revised), `e`
constant + E-reserved error (D11), HOME-pops-to-root, ESC exits diagnostics, graph
grid/palette colors, scientific-literal display (`1e10`), shifted F-keys F6-F9,
graphing with zoom, **graph recompute 15.3-17.1 ms for 2 functions (<50 ms target —
task 5.6 profiling done)**. Battery indicator: this unit's STM32 fw lacks the battery
register — shows "--" by design (see Session 5 entry).

**Verified on Pico 1 hardware 2026-07-11 (rounds 2-3, Session 6):** dirty-band
rendering (D13), battery % + cold-boot grace fix, SD card r/w self-test +
persistence, store op `2->A`, pretty math incl. the D2 revision (`1/sqrt(2)`,
`x^2/2` stack), trace + S/T presets, mode toggles, reboot-to-bootloader, full
exit test. **Pico 1 verification is complete.**

**Verified on Pico 2 hardware 2026-07-17/18 (Session 8 test drive):** everything in
the 2.24 checklist — function-mode parity post-refactors, Y= editor feel, home
basics, parametric acceptance (circle/Lissajous, editor auto-focus + pair
behavior), polar acceptance in both angle modes (cardioid/rose), tables (auto
infinite scroll, ASK add/delete/hint, setup + F2-to-Step, column scroll +
markers, detail precision), split-screen + trace sync, help tabs, keymap, cold
power cycle persistence. Bugs found were fixed the same session (commit 079a8b2).

**Verified on Pico 2 hardware 2026-07-18 (Session 9 — offline spin + re-fix
verification):** all ten Session 8 fixes, in two rounds, **plus the Session 9
six-item improvement batch (D19/D20 keymap, typed commands, graph chrome,
square ZStandard, DEL/SPACE) — all verified on-device same day** (minor
polish rounds: diag text, hint color/wording, diag line wrap). The offline
spin passed 8/10 on 079a8b2 (including the split-F1 watch-item and
MODE/graphstate persistence across reboots — which also closes the PCG2
one-time-reset check).
The two failures (held-key scroll overrun; battery staleness) were
re-root-caused, re-fixed, flashed, and **verified on-device same session**.
Item 9's verification (charger plug → status bar follows within ~5-6 s, battery
at 84%) also **confirms the charging-bit decode** (`raw & 0x8000`, value-byte
bit 7) — the last open battery question from Session 6.

Still to verify on hardware:

| Item | What to check on hardware |
|------|---------------------------|
| Pico 1 full pass — **deferred to post-Phase 3 (D18)** | Runs as part of Phase 3's both-boards pass (3D.14). Still on Session 7 firmware — reflash `build/pico/…uf2` first. Covers the whole Phase 2 sweep (headline: split-pane clipping on the strip renderer — no bleed across the divider), the Session 8+9 fixes + Session 9 remap, and Phase 3 acceptance |
| Session 10 round 2 (flashed 2026-07-18; round 1 eval passed — screens good, labels kept) | `L` toggle survives a reboot (PCG3 — expect a **one-time state reset** on first boot: re-set window/mode); `rand()` shows correctly in history; ZTrig tick labels short (`1.571`-style); quick regression: F ZoomFit still fine |
| Session 10 round 3 (bulk PSRAM verified on Pico 2 2026-07-18) | Nothing further on the Pico 2 (`psram-bulk: OK`, 150/156 us). **Pico 1 leg folds into the D18/3D.14 pass**: check the `psram-bulk:` heartbeat and diag `PSRAM: word OK, bulk OK` there — the chunked path is board-independent but only Pico-2-verified |
| Session 11 — Phase 3A lists (flashed 2026-07-19, boot + psram-bulk heartbeat verified over serial) | Home: `{1,2,3}->l1`, `l1+l2`, `l1*2`, `sum(l1)`, `sort_asc(l1)`, `seq(x^2,x,1,10,1)->l2`, error cases (`l1+l6` length mismatch, `5->l1`); results render in history (short lists + `,...` truncation). Editor (`lists` cmd): navigation, type-to-edit, append advance, DEL row shift, F6/F7 sort, F8 clear, horizontal scroll to l4-l6. Persistence: lists survive a reboot; big-list path: `seq(x,x,1,1000,1)->l1` (PSRAM tier) then sort + reboot. Cold power-on: lists appear after late-init (D14 wait, `late-init: lists loaded` if late). Regression: normal scalar eval, history recall, help tabs (new LISTS sections, wider FUNC summary column) |

---

## 2026-07-19 — Session 11: Phase 3 sub-phase 3A — Array, lists, list editor (D22)

Started Phase 3 per next-session.md. All five 3A tasks (3A.1-3A.5)
code-complete in one session; both boards build, lint clean, 106 new host
checks (suite now 351), Pico 2 flashed and boot-verified over serial.

**Design departure recorded as D22** (the load-bearing discovery of the
session): the spec's §2.1 `Array` sketch assumed pointer access, but the
PSRAM is SPI-attached and **not memory-mapped** — `platform::Psram` deals
in addresses via `read()`/`write()`, and its allocator is bump-only. The
as-built API is therefore `get`/`set` + `read_range`/`write_range`
(which is also exactly the shape D21's dtype-tag rule wants), and
`ArrayStore` recycles fixed-size storage: 12 x 2 KB SRAM slabs, up to
12 x 80 KB PSRAM regions on a free-list (bounded, fragmentation-free).
The spec §2 got an "as built" note.

What shipped:

- **`math/array.{hpp,cpp}`** — dtype-tagged (D21) 1-D/2-D `Array`,
  tier migration at 256 doubles (slab→region and back), zero-filled
  growth, cap 10000; `math::psram_backend` seam so host tests run the
  identical code against a malloc shim (`tests/host/host_psram_backend`).
- **`math/lists.{hpp,cpp}` + `lists_persist.cpp`** — `ListStore` l1-l6;
  `/picocalc/lists.dat` (magic `PCL1`, per-list dtype+count header,
  elements streamed in 2 KB chunks both ways — a full file can be 480 KB,
  far beyond any SRAM buffer, hence new **`Storage::read_file_range`**
  (f_lseek) in the platform layer). Load is all-or-nothing and returns
  false until PSRAM is up when large lists exist (D14 cold boot); main's
  late-init loop retries it (`late-init: lists loaded`). Saves are
  on-mutation (editor commits, home-screen list stores/sorts).
- **`math/list_ops.{hpp,cpp}`** — sum/prod, in-place sorts (NaN-safe
  total order; PSRAM tier uses an **external merge sort**: 256-element
  sorted runs into a temp region, streaming merge passes ping-ponging
  region<->region, ~6 passes at 10000), cumsum, delta_list, seq (engine
  compile-once, var slot saved/restored), copy — all chunked/streaming.
- **`math/list_expr.{hpp,cpp}`** — the home-screen syntax layer (D22):
  literals, l-refs, `->lk` store, bare-arg reductions substituted as
  numeric literals, wrapper functions, and the **vector lift** — any
  engine expression over l1..l6 compiles once via the new
  `Engine::compile_with(extras)` (l1..l6 bound as per-element variables)
  and evaluates in 256-element chunks. `sort_asc(l1)` bare-arg form
  sorts in place per spec; compound args are by-value.
- **`apps/list_editor.{hpp,cpp}`** — grid editor (3 of 6 lists visible,
  horizontal scroll, append row, type-to-edit via `eval_field`, DEL
  row-delete, F6/F7 sort, F8 clear, global F1-F5 intact), reached by the
  typed **`lists`** command. Strip-safe: visible cells cached as text on
  change; render only draws. Dirty-band tracked.
- **Home screen**: list expressions get first crack in evaluate_input
  (kNone falls through to the scalar engine untouched); `Entry.result`
  widened 24→48 for `{...}>l1` results; scalar-result formatting
  factored. **Catalog** gained help-only rows (fn == nullptr, skipped by
  build_lookup) for the eight list functions; help FUNC column widened
  (kSummaryCol 13→19), KEYS/SYNTAX got LIST EDITOR / LISTS sections.

Testing: `tests/host/test_lists.cpp` (106 checks) covers Array basics +
tier boundaries + recycling, all ops (incl. 5000-element external sort,
NaN ordering, seq edge cases), and ~40 list_expr cases (grammar, stores,
errors, empty lists, formatting truncation, PSRAM-tier lift). test_math's
catalog check was taught about help-only rows (arity check now
registration-only).

Flash: BOOTSEL cp path; note **`cp` exits 1 on macOS with "could not
copy extended attributes"** — harmless, the UF2 lands and the board
reboots (verified: volume unmounted, app re-enumerated, `psram-bulk: OK`
heartbeat + battery line on serial). On-device functional eval is queued
in HW-PENDING (Session 11 row).

Deferred/notes:

- Reductions take bare list names only; list literals can't sit inside
  element-wise arithmetic (both documented in D22 — revisit if 3B wants
  a real tagged-value evaluator).
- List results don't set Ans (scalar Ans preserved — D22).
- The editor's F8 clear is immediate (no confirm) — watch in eval.
- ArrayStore worst case: 24 KB static SRAM (slabs) + 960 KB PSRAM.

## 2026-07-18 — Session 10: pre-Phase-3 deferred-item batch (D9 fonts, rand seed, ZoomFit, axis labels)

Reviewed the deferred-items backlog before Phase 3; the user picked four to
clear first (D10/D14 stay next in line): D9 was easy, ZoomFit is a good
feature, axis tick labels need on-device evaluation (and needed D9's small
font), and rand() needed seeding before statistics work begins.

**D9 font upgrade — done (also D17 permissive-path step 3):**

- Vendored **Spleen 2.2.0** (BSD-2-Clause) under `drivers/spleen/` (8x16 +
  5x8 BDFs + LICENSE + README with regen commands).
- New `scripts/bdf_to_utft.py` converts BDF → the UTFT header layout
  `gfx::Font` already reads (packed row-major bitstream; glyphs composed via
  BBX/ascent). Verified by decoding glyphs back to ASCII art.
- Generated `src/gfx/fonts/spleen8x16.h` / `spleen5x8.h` (ASCII 32–126,
  ~1.5 KB + ~0.5 KB); `gfx::main_font()` is now the 8x16, new
  `gfx::small_font()` is the 5x8. Coyote `font1` is **no longer compiled
  in** — NOTICE.md updated (GPL surface now lcdspi/i2ckbd/pwm_sound only).
- Layout: Spleen's cell has built-in leading (caps ink rows 2–11), so
  16px rows pack tight. `kRowH 14→16` (table, files), `kLineH 14→16` +
  `kVisibleLines 19→16` (help), help scroll indicator moved into the title
  bar. Everything else already scaled off `font.height()` or had roomy rows;
  bar heights and text y-offsets unchanged (caps ink fits).

**rand() seeded** (closed the lint-era backlog item): `math::fn::rand01()`
is now a **xorshift64\*** PRNG (top-53-bits → [0,1) double) with
`seed_rand(uint64)`; firmware `main()` seeds from the SDK entropy source
(`get_rand_64()`, new `pico_rand` link dep). Host tests stay deterministic
under the default seed; 6 new checks (range, determinism, divergence) —
math suite 105→111.

**ZoomFit** (`F` on the graph screen, task 4.7 finally): function mode
refits y over the current x-range (TI behavior); parametric/polar refit
both axes to the curve extent. Sweeps world coordinates through the same
`PointSource`s recompute plots from (kMaxCurvePoints cap; engine vars
saved/restored like recompute). 5% margin, half-unit span floor for flat
curves; no active/plottable curve → no-op. Help KEYS updated.

**Numeric axis tick labels** (task 4.4, deferred since Phase 1 M4): drawn
in the small font at scl grid lines, thinned to >= ~48px (x) / ~24px (y)
apart, origin skipped, placed beside the axes (flipping/clamping at
edges; screen-edge fallback when an axis is off-screen). **`L` toggles
live** — this shipped explicitly as an *evaluation* feature: judge on
device whether it's too distracting; not persisted. Strip-safe (pure
draws in `draw_axes`' const path).

Lint clean (two float-loop-counter findings fixed by switching to the
integer-index grid idiom), both boards build, 216 host checks green.
**Pico 2 flashed same session** (BOOTSEL cp path, ~15 s mount wait
confirmed again); on-device eval queued in HW-PENDING.

**Round 3 (same day) — D10 root-caused, fixed, HW-verified; D14 scoped.**
The deferred-queue review moved to D10/D14:

- **D10 bulk-PSRAM hang: solved.** Reading the vendored driver against
  its PIO program found the mechanism — the PIO takes **8-bit transfer
  counts** (`out x, 8`/`out y, 8`; max 255 bits = 31 bytes/transaction),
  and `psram_write()`/`psram_read()` let the count byte wrap above 27/31
  data bytes, desyncing PIO from the DMA stream (count 0 underflows
  `jmp x--` into a ~2^32-bit shift loop) and wedging the blocking DMA
  wait. Fix: `Psram::read/write` chunk internally (27 B writes / 31 B
  reads, single DMA call + one mutex hold per chunk; also respects the
  chip's ~8 us tCEM). **Un-quarantined** — any length/alignment works.
- **Watchdog-guarded bulk self-test** in `run_self_tests()` (permanent):
  cap-straddling sizes, unaligned start, cross-chunk addressing, 1 KB
  timing. The historical failure is an infinite DMA wait, so the test
  arms a 2 s watchdog with a scratch-register marker — a regression
  reboots once and the next boot skips the test (no boot-loop). Verdict
  repeats on a 30 s serial heartbeat (`psram-bulk:`); diag screen shows
  `PSRAM: word OK, bulk OK`.
- **HW-verified on the Pico 2 same session**: `psram-bulk: OK (1KB
  write 150 us, read 156 us)` — ~6.8 MB/s, roughly 40x the word path.
  Unblocks the Array PSRAM tier (D21 keeps Phase 3 SRAM-only by choice,
  not necessity) and Phase 4 matrices.
- **Serial-capture gotcha found**: pico stdio_usb transmits only with
  DTR asserted — `screen` does that, plain `cat` does not (why captures
  looked dead). New `scripts/serial-capture.py` asserts DTR/RTS for
  non-interactive use: `serial-capture.py [seconds] [match-substring]`.
- **D14 (rail settle)**: software instrumentation is already sufficient
  (late-init timestamps + heartbeats); root cause needs bench time —
  scope plan written into next-session. Unchanged risk profile.

**Round 2 (same day) — eval verdict + fixes.** On-device: screens look
good; **axis labels are keepers**. Three fixes from the eval, flashed:

1. **`L` toggle now persists**: `axis_labels` moved into `GraphState`
   (default on), saved on toggle. Persistence magic bumped **PCG2→PCG3**
   — one-time state reset on first boot with this build (the loader
   falls back to defaults + Phase 1 legacy-file migration, so ancient
   yfuncs.txt content may resurface once; re-enter window/mode and
   resave).
2. **`rand()` rendered as `rand())`** in history: the layout parser's
   empty-arg-list path let `parse_expr` consume the `)` as a stray
   one-char text atom, and `make_paren` then drew its own. Fixed in
   `layout_builder.cpp` (empty args → empty text node); host layout
   test added (suite 33→37).
3. **Tick labels capped at 4 significant digits** (`%.4g`): ZTrig's
   pi/2 steps printed full double precision. **KIV (recorded in
   next-session)**: symbolic ticks (pi, pi/2) for irrational steps, and
   more broadly surd-form / fraction / pi-fraction *answer* display.

---

## 2026-07-18 — Session 9: offline-spin verdict, items 8+9 re-fixed & verified, D18 Pico 1 deferral

**Offline spin on 079a8b2: 8/10 Session 8 fixes verified on-device** — including
the split-F1 trace toggle (the one code inspection couldn't reproduce) and
MODE/graphstate persistence. The two failures were re-root-caused, fixed,
flashed, and **verified on-device the same session**:

- **Item 8 (held-key scroll overrun): the Session 8 drain was a no-op.**
  `Keyboard::poll()` is a two-phase machine — the first call only selects the
  FIFO register and returns kNone; the read lands ≥10 ms later. The drain loop
  broke on that first kNone, so it still consumed one event per frame. New
  `Keyboard::fifo_empty()` reports whether the last *completed* read found the
  FIFO empty; the main loop now drains through mid-phase kNones until a real
  empty read (event cap 16 + 250 ms budget as wedge guards). Verified: scroll
  stops at release.
- **Item 9 (battery staleness): the ~1 s target was never achievable** — the
  1 Hz main-loop check read a cache whose internal I2C refresh was still 30 s
  (worst case ~31 s; the observed 2-3 s was a lucky phase). Battery API split:
  `battery_status()` is now cache-only (render-safe, no I2C ever);
  new `battery_poll()` — sole call site, the main loop's 1 Hz check — owns the
  refresh. Cadence set to **5 s by developer call** (stability over a snappy
  charging indicator; an STM32 wedge needs a physical power cycle). Serial
  `battery:` prints on change + 30 s heartbeat instead of every read.
  Verified: status bar follows a charger plug within ~5-6 s — which also
  **confirms the charging-bit decode** (battery was at 84%).

**D17→D18: Pico 1 Phase 2 pass deferred to post-Phase 3** (board swap is
tedious; the board-conditional surface is 4 files; clip logic is shared and
Pico-2-exercised; RP2040 static RAM is 62.5 KB of 264 KB). Folds into Phase 3
task 3D.14. Guardrail added to phase3-spec §8: new `render()`s must be
strip-safe (idempotent, ~20×/frame on Pico 1; no host coverage exists).

**Flash-path revision:** the RP2350 BOOTSEL volume mounts again and
cp-to-volume is preferred — `picotool load` hung for minutes at a black
screen. Gotcha recorded: BOOTSEL reboot needs `picotool reboot -f -u`
(plain `-f` reboots into the application).

**Usage-observation round + six improvements implemented (same session).**
Observations logged (`testdrive-phase2-observations.md` §"round 2"),
designs settled in a quiz with the developer, then implemented:

1. **Graph status bar + top-bleed clip**: GraphScreen now draws the
   status bar (title shows GRAPH FUNC/PARAM/POLAR) and confines plot
   drawing to the plot rows via a tightened pane clip (restored for
   chrome; split panes unaffected — Framebuffer gained pane rect
   getters).
2. **Square ZStandard**: default window y = ±8.75 (= 10·280/320), so
   the standard window is square as displayed.
3. **Typed commands** on home: `cls` (session-level scrollback clear
   via display watermark — recall walk and history.txt untouched),
   `clrhist` (full wipe incl. history.txt), `help`, `diag`, `files`;
   grey right-aligned "type help" hint on the empty input line.
4. **Case-sensitive input** (D19): lowercase folds removed from
   preprocess + store op; `2->A` errors pointedly; `1E10` literals
   still fine (strtod). Stored-var echo now prints lowercase.
5. **DEL/SPACE semantics**: DEL clears (editor rows; WINDOW/table-setup
   fields edit-from-empty; ASK-table row delete), SPACE toggles slot
   enable in editors.
6. **F-key remap** (D20): global F1 editor / F2 window (table: setup) /
   F3 mode / F4 trace / F5 graph↔table, Alt+F5 split (HW-verified via
   diag key echo before implementing), `-`/`=` zoom, F6-F9 freed,
   global F6 diag toggle removed, FILES left the diag screen. New
   shared `apps/nav.{hpp,cpp}` (push_mode_editor, goto_graph_trace).
   Help KEYS/SYNTAX tabs rewritten for the new map.

Both boards build; lint clean; host tests 105+33+72 = 210 checks green
(new case-sensitivity coverage). Flashed to the Pico 2 (cp to BOOTSEL
volume). Next: on-device sweep of the remap + fixes, then close out
Phase 2 (2.24 done, judge 2.25, scope 2.22) → retro → Phase 3.

## 2026-07-17/18 — Session 8: THE Phase 2 test drive (2.24) + same-session fixes

**The full 2.24 checklist passed on the Pico 2** (Pico 1 deferred). Full record:
`docs/notes/testdrive-phase2-observations.md`; raw serial + 2.25 perf baseline in
`docs/notes/testdrive-serial-2026-07-18.txt`. Perf headline: recompute is nowhere
near the bottleneck — parametric pair ~1.0-1.4 ms, function set ~5 ms, polar
~2-5.3 ms, split ~4.1 ms vs the ~200 ms frame push.

**Bugs found on-device, all root-caused and fixed same session (079a8b2):**

- Stuck PSRAM/SD `FAIL` on the diag screen after a cold boot: the D14 late-init
  loop never re-ran self-tests that failed while rails were marginal. Now retries
  inside the 30 s window and prints late-init events over serial.
- Screen-stack leak: editor F4 *pushed* graph even when the editor sat on top of
  it; at kMaxDepth every push silently no-oped ("F4 stops working, ESC fixes
  it"). New `ScreenManager::switch_to` pops/replaces instead.
- Cardioid missing its final arc in degree mode: sweeps dropped the partial last
  step (up to one full step short of THmax). Sources now emit a final sample
  clamped to the range end; radian defaults had the same ~1.9° gap unnoticed.
- Held-key table scroll overran after release (event backlog): main loop now
  drains the key queue before each render.
- Home status bar: tall pretty-printed history entries drew over it (layout
  height checked only after drawing), and battery %/charging went stale under
  event-driven rendering. History now clips at the bar; battery cache polled
  ~1/s with a status-band repaint on change.
- Charging flag read bit 7 of the echoed register ID (always 0) — the Session 6
  "low byte assumption" was wrong. Now `raw & 0x8000` (value-byte bit 7) + a raw
  serial print for confirmation below 95%.
- DEG/RAD (and display mode / FIX digits) reset every boot: MODE-row settings now
  persist in graphstate.dat (**magic bumped to PCG2** — one-time state reset).
- Label lies: ESC/F4 pop to the *previous view* (behavior endorsed, wording
  fixed); table softkeys now standard divided cells; window footer fixed.

**Features from test-drive requests (same commit):** expression eval in all
numeric entry fields via `math::eval_field` (`2*pi`, `pi/180`; parse errors keep
the old value — strtod used to silently commit `2*pi` as 2.0); editor rows that
fail to compile render red; FILES screen (diag F6 → F5) lists /picocalc via the
existing `Storage::list_dir`; home F1 opens the mode-appropriate editor.

**Split-trace note:** "can't start trace inside split" was reported, but code
inspection says F1 is forwarded to the graph pane correctly and the split always
full-redraws. Softkey bar + help now advertise F1; verify the toggle on-device.

**Flash-path note (Pico 2):** macOS stopped mounting the `RP2350` BOOTSEL volume
this session; `picotool info` still saw the device, and `picotool load <uf2>` +
`picotool reboot` flashed fine. That is now the preferred path.

Both boards build; lint clean; 079a8b2 flashed to the Pico 2. Next: developer's
longer offline spin, then fix verification + the Pico 1 pass (see HW-PENDING).

## 2026-07-12 — Session 7: lint baseline clean + Phase 2 start (task 2.1 graph/ extraction)

Two commits: `lint: clang-tidy baseline clean`, `graph: extract Viewport + Plotter`.

**Lint cleanup (closed the "clang-tidy not installed" backlog item):**

- clang-tidy was installed via Homebrew `llvm` (keg-only → `lint.sh` now prepends
  `/opt/homebrew/opt/llvm/bin` when needed).
- Two silent problems found: (1) every TU failed to find newlib/libstdc++ headers
  (clang-tidy replays `arm-none-eabi-g++` commands but doesn't know GCC's built-in
  include paths — `lint.sh` now queries g++ for its search list and passes
  `--extra-arg=-isystem` per dir); (2) clang-tidy exits 0 on plain warnings, so lint
  "passed" while reporting them — `.clang-tidy` now sets `WarningsAsErrors: '*'`.
- Config was also backwards for this codebase (wanted `kmax_depth`-style constants,
  UPPER_CASE enum constants, no `#pragma once`); fixed to kCamelCase + prefix-k and
  dropped checks wrong for embedded (`.at()` needs exceptions; MMIO = fixed-address
  derefs; unchecked printf returns; cognitive-complexity on key dispatchers).
- ~50 mechanical fixes auto-applied (const-correctness, std::min/max, bool literals)
  — **caution**: `--fix` corrupted 4 `const char* arr[]` declarations into invalid
  `const char const*` and const-ified a written-through pointer in
  `Framebuffer::fill_rect`; all caught by rebuild + rerun. `misc-const-correctness`
  pointer analysis is now off (that FP class).
- Real code fixes: `atof/atoi` → `strtod/strtol`; factorial rewrite + `strip_zeros`
  use bounded copies (strcpy/strcat gone); grid-line loops use integer counters
  (float accumulation); `quiet_NaN()` replaces `0.0/0.0`; `Storage::read/write_string`
  const; `rand()` NOLINTed (seeding stays backlogged).

**Phase 2 task 2.1 — `graph/` extraction (per phase2-spec §2/§4):**

- New `src/graph/`: `viewport.{hpp,cpp}` (data↔pixel transform, Phase 1 formulas
  verbatim incl. the width-1 x-spacing), `plotter.{hpp,cpp}` (`PlotStyle`,
  `PointSource` interface, `Plotter` with the 140px discontinuity heuristic and
  the +/-1000 py clamp).
- Design call: GraphScreen **keeps its int16 column cache** (it's what makes trace
  redraws cheap) and replays it through `Plotter`'s pixel-space `begin()/point()`
  path; `Plotter::plot(PointSource&)` feeds the same path, so cached function mode
  and the future evaluate-on-plot modes (parametric/polar) share one segment logic.
- `value_to_py` and all duplicated transform math in recompute/draw_axes/draw_trace
  deleted in favor of `Viewport`.
- New host test `tests/host/test_graph.cpp` (18 checks) locks the transforms to the
  Phase 1 formulas — the refactor is provably behavior-preserving on the math side;
  a visual HW spot-check is queued above.
- **Deviation from spec §14**: trace was NOT generalized into `graph/trace.hpp` in
  2.1 — today's trace is a pixel-column walk over the function cache, and there's no
  second mode yet to generalize over. Folded into task 2.7 (parametric trace).

**Phase 2 task 2.2 — `graph::Mode` + mode-aware `GraphState`:**

- `graph/graph_mode.hpp`: `Mode` (kFunction/kParametric/kPolar — kCamelCase per
  codebase+lint convention, not the spec's UPPER_CASE) + `ModeDescriptor`/
  `descriptor_for()`. Slot counts 7/6/6 per §1/§9 (§3's "(function/polar)" comment
  saying 7 read as a typo). Polar descriptor's `independent_var = 0` = engine theta
  slot.
- `graph/graph_state.hpp`: `GraphState` = mode + Y/parametric/polar slots + shared
  x/y window + t/theta ranges (defaults §5.2/§6.2) + `TableConfig` (§7.1, lives in
  graph/ for layering). **Nested structs, not the spec's flat list**, so Phase 1
  screens keep their `GraphWindow&`/`YFunctions&`; save/load deferred to 2.23 with
  the migration (no dead persistence code).
- `apps::graph_model`: structs moved to graph/, apps keeps `using` aliases —
  zero churn in screens; globals are now references into `graph::state()`.
- Parametric/polar slots use `config::kMaxExprLen` (256) per §9; Y-slots stay 96
  until 2.23 (yfuncs.txt buffers sized to it).
- test_graph.cpp grew descriptor + defaults checks (host total now 144).

**Phase 2 tasks 2.3 + 2.4 — FunctionSource, engine sweep slot, ParametricSource:**

- `graph/function_source.{hpp,cpp}`: PointSource iterating viewport pixel columns
  through `eval_compiled` — Phase 1's recompute inner loop wrapped; GraphScreen now
  fills its cache from it (behavior identical).
- `Engine::eval_compiled(handle, var_slot, value)` overload — Phase 1 hardcoded the
  X write (§5.3/§14); 2-arg form delegates with the X slot. Parametric sweeps
  `'t'-'a'`; polar (2.8) will sweep `Variables::kTheta`.
- `graph/parametric_source.{hpp,cpp}`: t sweep with integer step counter +
  endpoint slack (1e-9) — `[0,2pi]` at `2pi/63` emits exactly 64 points; a point is
  defined only when both x(t) and y(t) are finite.
- Host tests: swept-slot checks (t/theta/X-compat), unit-circle sweep, degenerate
  zero-step case. Host total now **161 checks** (79 math + 33 layout + 49 graph).

**Phase 2 task 2.5 — SlotEditorScreen base + parametric editor (D15):**

- Editor architecture decided (D15): one `apps::SlotEditorScreen` base owning
  selection, InputLine editing, dirty-band row invalidation, key dispatch, and
  the render loop; per-mode subclasses provide labels/text/toggle/clear/checkbox
  + an `after_commit` hook. Chosen over both "one mode-aware Y= editor with
  if-branches" and "three duplicated screens" — the D13 invalidate footgun now
  lives in one file, and polar (2.9) should be ~50 lines.
- Commit 1 was a pure extraction (Y= behavior unchanged — same constants, same
  draw calls); commit 2 added `ParamEditorScreen`: 6 pairs as 12 rows (22px),
  pair checkbox on the X row, auto-enable when both halves non-empty, X-commit
  auto-focuses an empty partner (§5.1), F3 clears one field and drops the
  enable when the pair goes incomplete.
- **Not yet wired**: no navigation reaches the parametric editor until the mode
  selector (2.22), and its slots don't persist until the GraphState migration
  (2.23) — both deliberate, per spec task order.
- HW-PENDING: Y= editor visual/behavior spot-check after the extraction (queued
  with the 2.1 graph check).

**Phase 2 tasks 2.6 + 2.7 — mode-aware window screen; parametric plot + trace:**

- WindowScreen: fixed 6-field mapping → mode-driven field table; parametric
  prepends Tmin/Tmax/Tstep (§5.2 order) pointing into `graph::state()`; rows
  tighten to 28px when 9 fields; selection re-clamped on activate. Polar theta rows
  = 3 more table lines at 2.10.
- GraphScreen mode-branched: parametric recompute sweeps `ParametricSource` per
  complete enabled pair into a clamped (px,py)-per-step cache
  (`kMaxCurvePoints` = 340/pair ≈ 8 KB; default Tstep uses 64; tiny Tstep
  truncates — documented limitation). Undefined steps keep their index so the
  trace t-readout stays aligned.
- Trace generalized to `graph::TraceCursor` (index = pixel column in function
  mode, parameter step in parametric; clamped stepping host-tested). UP/DOWN
  cycles the active slots of the current mode; readout "P<n> t= x= y=".
- Graph F5 now pushes the parametric editor in parametric mode ("PAR" softkey).
- Host total **165 checks**. Week 11–12 subtotal (2.1–2.7) is code-complete;
  acceptance ("circle + Lissajous plot with trace") needs mode switching on
  device → argues for pulling a minimal 2.22 forward.

**Minimal 2.22 pull + built-in help (2.26–2.28, pulled from week 16):**

- MODE screen "Graph mode" row cycles FUNC<->PARAM (`graph::state().mode`);
  polar joins the cycle with 2.8–2.11. Mode unpersisted until 2.23.
- `math/catalog.{hpp,cpp}`: `FnDescriptor` table for all 17 parser functions;
  `build_lookup` registers functions by iterating it (variables stay inline).
  Host test asserts every catalog signature parses via the real engine — the
  drift guard §10 asks for. `kLookupCount` sized by `kMaxCatalogEntries` (32).
- `apps/help_screen`: FUNC tab (catalog-driven: signature + summary), KEYS tab
  (per-screen key map incl. PARAM-era keys), SYNTAX tab (store op, e/E rules,
  ans, factorial, angle mode, graph modes, history). Home F5 = "HELP"
  (previously unassigned). All content in flash — no SD dependency.
- **Content caveat**: KEYS reflects the current keymap; the F-key layout
  rethink (feedback 7) is still open — revise help strings if it lands.
- Host total **183 checks** (97 math + 33 layout + 53 graph).

**Polar week (2.8–2.11):**

- `graph/polar_source.{hpp,cpp}`: theta sweep (integer step counter + endpoint
  slack, same as parametric) writing `Variables::kTheta`; Cartesian conversion
  honors angle mode (§6.3) — in degree mode the range is degrees and
  `r*cos/sin` converts accordingly. Host tests: cardioid (radians, 64 points,
  starts (2,0)), r=1 circle in degree mode hitting the four axis points.
- `apps/polar_editor`: SlotEditorScreen subclass — r1..r6, palette label
  colors, auto-enable on non-empty, ~50 lines (D15 paying off as predicted).
- WindowScreen: polar prepends THmin/THmax/THstep; the name column now sizes
  to the longest field name (function/param layout unchanged at 5 chars).
- GraphScreen: `recompute_polar` shares the parametric (px,py)-per-step cache
  (only one parameter mode is active at a time — no extra 8 KB); helpers are
  now mode-switched (`param_style()` = parametric|polar); F5 routes to the
  polar editor ("POL" softkey); trace readout "r<n> th= x= y=" with theta in
  current angle-mode units. Theta var saved/restored around the sweep.
- MODE screen graph-mode row cycles FUNC/PARAM/POLAR; help KEYS/SYNTAX updated.
- Host total **188 checks** (97 math + 33 layout + 58 graph).
- Acceptance queued for HW: cardioid `1+cos(theta)`, rose `2*sin(3*theta)`,
  both angle modes (spec week-13 acceptance).

**Task 2.23 (pulled from week 16) — unified GraphState persistence:**

- `GraphState::save/load` → `/picocalc/graphstate.dat`: "PCG1" magic +
  size-guarded raw image (GraphState is static_assert'd trivially copyable).
  Layout change = bump magic → old image rejected, defaults/migration apply.
  Impl in `graph/graph_persist.cpp` so host-test links stay platform-free;
  6.5 KB image buffer is static (Pico stack is too small).
- `apps::load_graph_state`: unified image first; on miss, one-time migration
  from window.dat + yfuncs.txt, then writes unified. Old files ignored, not
  deleted. `save_functions`/`save_window` = thin wrappers over the full save;
  param/polar editors + MODE graph-mode row now persist their changes.
- Y-slots grew 96 → `config::kMaxExprLen` (256) per §9 — safe now that the
  yfuncs.txt writer is gone (its buffers were the 96 constraint).
- HW-PENDING: first boot after flash must migrate existing Y-funcs/window;
  parametric + polar curves and mode must survive a cold power cycle.

**Tasks 2.12–2.18 — table view:**

- `apps/table_model.{hpp,cpp}` (split out of table_screen for host-testability
  — not in the spec's file list): column mapping per mode (enabled slots in
  order, gaps skipped; parametric = two columns per pair, §7.3) and
  `evaluate_table_row` (per-slot compile/eval/free; syntax errors → NaN
  column). Per-row compile is the simple/spec-shaped path — if scroll feels
  slow on HW, the lever is compiling once per regenerate (note for 2.25).
- `TableSetupScreen`: Start/Step/Independent(AUTO/ASK); immediate-apply +
  unified save (deviation from the §7.1 mock's F1:SAVE/F2:CANCEL — matches
  the WINDOW screen convention; revisit if it feels wrong on-device).
- `TableScreen`: 17 visible rows evaluated into a cache once per change
  (dirty_ flag — same strip-render pattern as GraphScreen); auto mode
  scrolls infinitely both directions (row n can be negative); ask mode holds
  up to 32 entries (oldest dropped when full; ENTER adds via the detail-line
  InputLine, F5 deletes); LEFT/RIGHT shifts the 3 visible dependent columns
  with </> overflow markers; detail line = full-precision selected row.
- Entry point: **Graph F4 "TBL"** (was the free softkey slot); F3/ESC pops
  back to the graph. G-T split (F4 in the mock) comes with 2.19.
- Host total **202 checks** (97 math + 33 layout + 72 graph).

**Tasks 2.19–2.21 — split-screen graph|table (D16):**

- `gfx::Framebuffer::set_pane_clip/clear_pane_clip` (§8.1): a rect composing
  with the strip window, enforced in set_pixel + fill_rect (all primitives
  funnel through them — verified before touching this HW-verified code).
- **Horizontal split** (P2-1 → D16): graph pane rows 0–138 at full 320px
  width — column caches, trace x-mapping, and plot code untouched; only the
  viewport height shrinks. Table pane rows 142–298 (7 rows), divider between,
  white edge marks the focused pane.
- GraphScreen/TableScreen: geometry constants became members with
  `set_pane`/`reset_pane` (set_pane marks dirty — cached py depends on
  height). SplitScreen **reuses the singletons** — no duplicated caches, no
  forked trace/window state; on_deactivate resets panes so pushed screens
  (setup, editors) render full-screen and return cleanly.
- Trace sync (2.20, option c nearest-row): `GraphScreen::trace_value/
  sync_trace_to_value` + `TableScreen::selected_value/highlight_value` —
  works in all three modes (pixel column in function, nearest parameter step
  in parametric/polar). Option b (trace steps by table-step) KIV after the
  test drive.
- Keys (D16): F4 = switch graph↔table (full-screen) / pane focus (split);
  F9 (Shift+F4) toggles split from graph or table; ESC exits; table-focused
  F3 exits (its "GRAPH" meaning) while graph-focused F3 still zooms out.
- HW-PENDING: split rendering on the Pico 1 strip renderer (clip-rect
  compose), sync feel, pane sizes.

---

## 2026-07-11 — Session 6: STM32 fw v1.6 (battery works) + dirty-band partial rendering

**Developer hardware work**: updated the STM32 keyboard firmware to **v1.6** (not the
v1.2 the notes suggested; `PicoCalc_BIOS_v1.6.bin` is in the repo root). **The battery
indicator now works on-device** — the missing register 0x0B was indeed just old
firmware; no code change was needed, as predicted. Still to check under v1.6: charging
color/bit, phantom keys after a battery refresh, Shift-on-arrows, F10 (see the queue).

**Dirty-band partial rendering** (task 5.6 part 2, decision **D13**) — the last big
Phase 1 code item. The ~200 ms per-keypress latency is SPI push time, which scales
with pixel count, so:

- `ui::Screen` grows opt-in dirty-band tracking: `track_dirty()` + `invalidate(y0, y1)`
  (row band, full width); `take_dirty()` consumed per frame. Non-tracking screens
  keep full-frame redraws; `ScreenManager` fully invalidates any screen that surfaces
  to top of stack.
- `Framebuffer::render_frame()` takes the band and renders/pushes only those strips
  (both strip mode and the Pico 2 full-buffer path, which now treats its buffer as
  scratch). Empty band → render skipped entirely.
- Home screen: typing/recall/ESC = input band (~28 of 320 rows → ~20 ms expected);
  Enter = everything above the softkeys (also refreshes battery/mode status); history
  scroll = history band. Y= editor: per-row bands (~26 rows) for edit/select/toggle.
- Graph screen intentionally left full-frame (trace/zoom touch ~280 rows anyway — D13
  "revisit when").

Both boards build; 106 host checks green (host tests don't cover `ui/`).

**HW verification round 2 (same session, live with the developer over USB serial):**

- **Dirty-band rendering verified on Pico 1**: typing/recall/ESC instant, Enter,
  history scroll, Y= editor select/edit/toggle, all screen switches — no stale rows
  or seams anywhere. 5.6 closed.
- **Battery**: % displays correctly (100%). Found + fixed: on cold power-on the STM32
  is still booting when the first frame renders, so the first read failed and the
  10 s backoff pinned "--" in the status bar. `battery_status()` now has a 10 s boot
  grace (2 s retries that don't count toward the give-up cap). Verified after a
  power cycle: % appears at the first keypress ~2 s in.
- **STM32 v1.6 keyboard behavior unchanged from v1.2**: Shift+arrows still swallowed
  (diag serial shows the kShift press arrive with *no* arrow event), F10 still never
  emitted (Shift+F5 → F5). D12's Alt/Ctrl view-scroll stands. No phantom keys after
  a 40 s idle spanning a battery refresh.
- **Charging color inconclusive**: at 100% the charger is idle, so the charging bit
  being 0 proves nothing. Retest when the battery is below ~95% (queue row).

**HW verification round 3 (same session): rest of the queue cleared on Pico 1.**

- **Pretty math**: mostly right on first look, but `1/sqrt(2)` rendered inline —
  a function call parses to an HBox, which D2's is-simple check didn't accept.
  Fixed: calls (recognized structurally, `HBox[alpha-name, paren]`) and
  superscripts now count as simple operands, so `1/sqrt(2)` and `x^2/2` stack
  (D2 revision; 6 new layout tests, 33 total). Re-verified on-device.
- **Verified**: store op `2->A`/`A+1`→3 (`-` and `>` type fine), trace + S/T
  presets, mode toggles (status bar follows), reboot-to-bootloader (mounted
  RPI-RP2 — used it for the final reflash), SD card r/w self-test OK on the
  diag screen + history persistence. That completes the 5.7 exit test on Pico 1.

**Pico 2 (RP2350) bring-up (same session, continued): display path works;
cold-boot PSRAM/SD root-caused (D14).**

- **First flash ever on Pico 2.** The untested full-framebuffer display path
  **works**: home screen renders, diag shows "Pico 2 (RP2350)", color bars
  correct, keyboard + battery fine. (BOOTSEL volume is `RP2350`, not `RPI-RP2`.)
- **PSRAM + SD both failed on cold power-on, both fine on warm reboots.**
  Debugged over three instrumented flash cycles (buffered init trace dumped
  over USB serial after boot; timestamps). Measured: PSRAM reads zeros at
  0.5 s, near-correct data (bit-shifted — analog marginal, same at 75 MHz and
  18.75 MHz SCK) at 0.6-2.5 s, perfect at 7.5 s. SD answers CMD0/CMD8 cold but
  never completes ACMD41 until ~7.5 s, then inits instantly (R7 voltage echo
  clean). **The peripheral rail needs ~5-8 s to settle after cold power-on
  with the Pico 2 module**; Pico 1 doesn't show this. Matches community
  reports (RP2350 PSRAM cold-boot failures; fuzix SD failure on PicoCalc
  Pico 2, clockworkpi/PicoCalc#12).
- **Fix (D14): deferred late-init.** Boot stays instant; the main loop
  retries PSRAM (`Psram::reinit()` — chip reset via the existing PIO, no
  PIO/DMA re-allocation) and SD (`Storage::init()`) every 2 s for the first
  30 s. Late storage arrival re-runs self-tests + loads history/variables/
  graph state and refreshes the screen.
- Debug harness worth remembering: `stty -f /dev/cu.usbmodem* 1200` works on
  RP2350 for no-touch reflash; boot prints race USB enumeration, so buffer
  init logs and dump them once `stdio_usb_connected()` (or just late).
- **Verified on Pico 2 (cold power-on, 2026-07-12):** with the D14 late-init,
  a cold boot ends with the diag screen showing PSRAM OK + SD OK (came up
  before the developer even opened diag). Instant boot preserved.

Full flash-test-fix loop on Pico 1 with live USB-serial capture. Three flash cycles.

**Verified working** (removed from the HW-PENDING queue): evaluation/history, input
recall + view scroll, `e`/E-reserved, HOME key, ESC-exits-diag, graph colors, shifted
F-keys, graphing with zoom, and the `1e10` display fixes from this session.

**Graph profiling (5.6 part 1) DONE**: `graph recompute:` 15.3-17.1 ms for two enabled
functions (sin(x), x^2-3) on Pico 1 — well under the 50 ms target. Evaluation is not
the bottleneck; the ~200 ms full-frame display push is (dirty-rects = 5.6 part 2).

**New bugs found on-device and fixed** (HW-found, host tests added where possible):

1. **`1e10` displayed as "1" / result "10e9".** Two independent bugs: (a) the layout
   builder had no scientific-literal support and silently rendered a *truncated* tree —
   fixed by consuming `[eE][+-]?digits` after a number AND falling back to whole-string
   plain text whenever input is left unconsumed (also un-breaks `2->A` and multi-arg
   calls in history); (b) the Pico SDK's printf emits unnormalized `%e` mantissas at
   exact powers of ten ("10.000000000e+09") — host libcs don't, so host tests couldn't
   catch it; `normalize_mantissa()` fixes the output in both sci paths. +10 host checks.
2. **STM32 keyboard-controller forensics** (diag key echo over serial):
   - Shift on arrows is swallowed (STM32 emits shift-release, then a plain arrow) →
     view-scroll rebound to **Alt/Ctrl+UP/DOWN** (both pass flags through; D12 revised).
   - Shift+F1..F4 → scan codes 0x86-0x89 (F6-F9) confirmed. **Shift+F5 emits plain
     F5** — F10 does not exist in this STM32 firmware (decode keeps 0x8A for future fw).
   - **The battery register (0x0B) is unsupported by this unit's STM32 firmware**:
     reg 0x01 answers, 0x0B times out (100 ms) at select or read. Indicator shows "--";
     `battery_status()` stops after 5 consecutive failures until reboot.
   - **Hammering the STM32 wedges it** (the Session-4 build's broken backoff retried
     every strip): keyboard dead until a *physical power cycle* — the STM32 is not
     reset by USB reflash. Rule: pace all STM32 traffic, never poll aggressively.

Both boards build; 106 host checks (79 math + 27 layout). Battery debug instrumentation
removed after verification. Remaining for phase 1 close-out: SD/persistence (FAT32 card),
store-op typing, trace/S+T presets, mode/reboot 5.3, full exit test 5.7, Pico 2 bring-up.

---

## 2026-07-11 — Session 4: battery level indicator; first HW-test attempt stalled

HW verification of the polish fixes started: new firmware flashed to Pico 1 over the
1200-baud reset, serial capture attached. The diag-screen key tests were run **but the
serial log came back empty** — then the developer suspected a flat battery and paused.
Diagnose the capture path when the device returns (the key echo should print every
press; an empty log means the capture, power, or USB path failed — not the firmware).

That pause surfaced a gap: nothing displays battery level. `platform::read_battery_info()`
existed since M1 (STM32 register: percent + charging bit) but had no UI. Added:

- **`platform::battery_status()`** — cached accessor: refreshes at most every 30 s and
  only while the keyboard poll state machine is idle (new `Keyboard::bus_idle()`).
  The raw read blocks ~16 ms and shares the 10 kHz I2C bus with the keyboard's
  two-phase FIFO poll — an interleaved read would be decoded as FIFO data and produce
  phantom key events, hence the guard.
- **Status bar** (chrome, all screens): battery icon + percent at the far right; green
  above 50%, amber 21-50%, red at 20% and below, cyan while charging; "--" when
  unavailable.
- **Diag screen**: "Battery: N% (charging)" line.

Both boards build; 96 host tests still pass (feature isn't host-testable — HW rows
queued). **The on-device firmware is now one build behind** — reflash before resuming
the HW checklist.

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
