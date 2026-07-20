# Start here — next session

**Last session:** 2026-07-20 (Session 18). **Sub-phase 4C is
code-complete (D30)**, plus P4-7 (complex matrix eigenvalues, user pick)
— complex numbers, on top of the Session 17 Phase 4B close. Suite
**1206 checks**, lint clean, both boards build clean. **No hardware was
connected this session — nothing was flashed.** The Pico 2 is still on
the **Session 16 (4A) build**; Pico 1 is still on Session 7 firmware
awaiting the deferred 3D.14 pass. What landed:

- **`Complex` type + math** (`src/math/complex.{hpp,cpp}`): arithmetic,
  modulus/argument/from_polar, and the spec's elementary set (sqrt, exp,
  ln, pow, sin/cos/tan, asin/acos/atan, abs/arg/conj/real/imag).
- **`NumberMode`** (REAL/RECTANGULAR/POLAR): mirrors `AngleMode`'s
  storage/persistence/MODE-row pattern exactly; new "Number" row on the
  MODE screen; `GraphState` persistence bumped to **PCG5**.
- **`math::complexexpr`** (`src/math/complex_expr.{hpp,cpp}`): the
  home-screen complex evaluator, recursive-descent over `Complex`
  (mirrors `matexpr`'s shape since `Complex` can't flow through
  tinyexpr). Special-cases only `i`/`2i` and the complex-aware function
  set; everything else (pi/e/theta/ans, bare vars, the rest of the real
  catalog) reuses `eval_field` as an opaque real span. Side-effect-free
  by design — the home screen uses it as a probe in REAL mode to turn a
  bare NaN (`sqrt(-4)`) into "Non-real result" without double-committing
  Ans/a store. `i` is now globally reserved (blocked as a store target
  everywhere `e` already was).
- **`format_complex`**: rectangular ("3 + 2i") and polar ("2<60" — ASCII
  `<` stand-in for ∠; the vendored font has no true angle glyph).
- **P4-7: matrix eigenvalues, full spectrum** (`src/math/matrix.cpp`):
  `eigenvalues(Array&)` keeps its old "errors on complex" contract
  unchanged; new `eigenvalues_complex(Complex*, int*)` returns the full
  spectrum. `eigenvals([A])` shows an all-real spectrum as before
  (storable list) or a conjugate pair as new unstorable `Kind::kText`
  (e.g. `{i,-i}`).
- **D30** records all the as-built calls, including the two taken
  upfront: **P4-9 default number mode = REAL**, **P4-7 = add complex
  eigenvalues now** (spec's own note said "likely defer"). Full details
  in `decisions.md` (D30).
- Pico 1 grew modestly (text +7968 B, bss +68 B — ~188.7 KB of 264 KB,
  same watch item as D28/D29, not worse).

## The next job

1. **On-device evals** (worklog HW-PENDING; the flashed build is still
   Session 16/4A — flash the Session 18/4C build first, expect a
   **one-time PCG5 graph-state reset** on first boot): Session 11 (3A
   lists), Session 12 (3B stats), Session 15 storage health + 3D
   (inference + stat plots), Session 16 4A (matrix editor, bracket
   typing, solver), Session 17 4B (CALC menu, min/max Guess-step
   judgment), and **Session 18 4C**: MODE Number row cycle, REAL-mode
   "Non-real result" wording, a+bi/polar display + store rules, the `<`
   polar stand-in (judge if it needs a real glyph), `eigenvals()`
   complex text. This is the largest single-session diff yet (7 new/
   changed math source files) — worth a broader regression sweep, not
   just the new surface.
2. **3D.14 — the combined Pico 1 pass (D18)** closes Phase 3: swap the
   board, reflash `build/pico/…uf2` (BOOTSEL volume `RPI-RP2`), run the
   Phase 2 sweep (headline: split-pane clipping on the strip renderer),
   the Session 8+9 fix list, Phase 3 acceptance, and watch every §8
   screen for strip-render artifacts. **Re-check the map file** — Pico 1
   bss is ~188.7 KB of 264 KB (D28/D29/D30, essentially flat across all
   three); if the ~76 KB stack/heap headroom pinches, shrink
   `ArrayStore::kSlabCount`.
3. After the 4C on-device eval: **Phase 4D (CAS engine)** is next per
   `phase4-spec.md` §6 (weeks 32-36) — symbolic simplify/expand/solve,
   using this session's `Complex` type as the numeric backing for
   complex roots (§5.5 hook; quadratic/polynomial solves with negative
   discriminant emit `i`-valued symbolic roots).

Mind the §8 strip-safety rule (idempotent `render()`) for any new
screens touched during the on-device passes.

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Firmware on the unit is still the Session 16 build** (4A matrices +
  solver; flashed 2026-07-20, warm-boot verified over serial). Sessions
  17 (4B) and 18 (4C) are code-complete but **not yet flashed** — no
  hardware was connected either session. The Pico 1 is still on Session
  7 firmware; its pass is 3D.14 (D18).
- **`lists.dat` / `matrices.dat` may not exist yet on the SD card** —
  first save creates them. If a load ever misbehaves, deleting the
  file resets that store (magics PCL1 / PCM1; bump on layout change).
- **Flashing the 4C build will one-time-reset graph state (PCG5)** —
  same pattern as PCG3/PCG4 before it (window/mode/plots/number-mode
  back to defaults, then persistence resumes normally).
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

## Pico 1 pass: DEFERRED to post-Phase 3 (D18)

Decided 2026-07-18: the board swap is tedious, the board-conditional surface is
tiny (clip logic is shared and Pico-2-exercised; RP2040 RAM headroom is ~195 KB),
and the residual risks (strip-render idempotency, perf feel) are localized, not
architectural. One combined pass after Phase 3 (task 3D.14) covers the Phase 2
sweep — headline: **split-pane clipping on the strip renderer** — plus the
Session 8+9 fix list and Phase 3 acceptance. Until then the Pico 1 stays on
Session 7 firmware; reflash before that pass (`build/pico/…uf2`, BOOTSEL volume
`RPI-RP2`). Guardrail: Phase 3 render code must be strip-safe (idempotent, may
run ~20x/frame) — rule recorded in `phase3-spec.md` §8. Note for that pass:
Pico 1 bss is ~188.7 KB of 264 KB as of Session 18 (D28/D29/D30 combined) —
re-check the map file then; the knob is `ArrayStore::kSlabCount`.

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
- **4C watch-items (Session 18, judge on device)**: ASCII `<` as the
  polar angle separator (`"2<60"`) instead of a baked ∠ glyph — decide if
  it reads clearly or is worth extending the font by one slot; whether
  "Non-real result" is clear phrasing for REAL-mode domain errors; no
  complex-valued variable storage (`2i->a` errors) — decide if that's
  ever actually wanted (D30).
- Backlog: D14 rail settle ([next-bench-session.md](next-bench-session.md) —
  the last deferred HW item); 340-point curve cache cap; audio HAL; licensing (D17 —
  display/keyboard rewrites remain); dual-core display service (D10
  addendum).

## Feature wishlist

Desired-but-unplanned features live in **[wishlist.md](wishlist.md)**. Complex
numbers and TI-84 CALC-menu graph analysis have graduated into Phase 4
(sub-phases 4C and 4B — both now code-complete — see
[phase4-spec.md](../phases/phase4-spec.md)); still-open item there is symbolic
display (KIV).

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
