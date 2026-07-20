# Start here — next session

**Last session:** 2026-07-20 (Session 16). **Sub-phase 4A is
code-complete (D28)** — matrices + numeric solver, on top of the
Session 15 Phase 3 close. Suite **953 checks**, lint clean, both
boards build, **flashed to the Pico 2** (boots clean; psram-bulk OK,
battery heartbeat sane). What landed (user decisions taken upfront:
TI `[A]`-`[J]` syntax; eigenvalues error on complex pairs; solver =
form screen + inline `solve()`):

- **`matops` + `MatrixStore`** (`src/math/matrix.{hpp,cpp}`): free
  functions over 2-D Arrays (listops-style streaming) — arithmetic,
  transpose, LU det, Gauss-Jordan inverse, rref/ref/rank, powers,
  reshape, QR eigenvalues (n<=10, real; complex pair = error).
  [A]-[J] persist to `/picocalc/matrices.dat` (**PCM1**, late-init
  retry like lists).
- **`[A]` expressions** (`src/math/mat_expr.{hpp,cpp}`): recursive-
  descent evaluator — `[A]*[B]`, `2*[A]`, `[A]^-1`, `[A]^T`,
  `[A](2,3)`, det/rank inline, inverse/rref/augment/identity,
  `dim`/`eigenvals` (whole-form, list results), `-> [C]`/`lk`/`a`
  stores, MatAns buffer. Routed first in the home screen.
- **`matrix` editor** (alias `mat`): TAB cycles [A]-[J]+Ans(RO),
  F7 DIM, F8 clear, strip-safe cached render.
- **Numeric solver** (`src/math/numeric_solve.{hpp,cpp}`): bisection +
  Newton polish, Newton-from-guess fallback; `solve` form screen
  (root -> variable + Ans) and inline `solve(f,x,lo,hi)` /
  `solve(f,x,guess)` / `solve(lhs=rhs,...)` substitution. Reused by
  4B zero/intersect later.
- **ArrayStore pools grew** (28 slabs, 24 PSRAM regions; catalog cap
  72): Pico 1 now text ~337 KB, **bss ~188 KB of 264 KB** (~76 KB
  stack/heap headroom — watch it).

## The next job

1. **On-device evals** (worklog HW-PENDING; the Session 16 build is
   already flashed): Session 11 (3A lists), Session 12 (3B
   stats), Session 15 storage health + 3D (inference + stat plots —
   expect the PCG4 one-time reset on first boot), **and Session 16
   4A**: matrix editor feel, `[A]`/`]`/bracket typing on the physical
   keyboard, `[A]*[B]` -> `[C]` round-trip, det/inverse/eigenvals
   spot-checks, matrices.dat first save + power cycle, solver screen
   + inline `solve()`, big-matrix (>16x16, PSRAM tier) edit/op timing.
2. **3D.14 — the combined Pico 1 pass (D18)** closes Phase 3: swap the
   board, reflash `build/pico/…uf2` (BOOTSEL volume `RPI-RP2`), run the
   Phase 2 sweep (headline: split-pane clipping on the strip renderer),
   the Session 8+9 fix list, Phase 3 acceptance, and watch every §8
   screen for strip-render artifacts. **Re-check the map file** — 4A
   pushed Pico 1 bss to ~188 KB of 264 KB (D28); if the ~76 KB
   stack/heap headroom pinches, shrink `ArrayStore::kSlabCount`.
3. Then **Phase 4B** (`phase4-spec.md` §4): graph analysis / CALC menu
   (value, zero, min/max, intersect, dy/dx, fnInt) — reuses 4A's
   `numeric_solve` for zero/intersect. Open question P4-6 (intersect
   curve picking) is due at 4B.5.

Mind the §8 strip-safety rule (idempotent `render()` — Stats/Dist/
Infer/Solver screens cache result lines; matrix editor caches cells;
stat plots split recompute/draw).

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Firmware on the unit is the Session 16 build** (4A matrices +
  solver on top of everything prior; flashed 2026-07-20, warm-boot
  verified over serial). The PCG4 graph-state reset already happened
  on the Session 15 build — no reset this time. The Pico 1 is still
  on Session 7 firmware; its pass is 3D.14 (D18).
- **`lists.dat` / `matrices.dat` may not exist yet on the SD card** —
  first save creates them. If a load ever misbehaves, deleting the
  file resets that store (magics PCL1 / PCM1; bump on layout change).
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
Sessions 11+12 added static SRAM (ArrayStore slabs, list buffers, stats chunk
buffers + sinusoid scan accumulators ~10 KB) — Pico 1 bss is ~126 KB of 264 KB,
still comfortable, but re-check the map file then.

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
- Backlog: D14 rail settle ([next-bench-session.md](next-bench-session.md) —
  the last deferred HW item); 340-point curve cache cap; audio HAL; licensing (D17 —
  display/keyboard rewrites remain); dual-core display service (D10
  addendum).

## Feature wishlist

Desired-but-unplanned features live in **[wishlist.md](wishlist.md)**. Complex
numbers and TI-84 CALC-menu graph analysis have since graduated into Phase 4
(sub-phases 4C and 4B — [phase4-spec.md](../phases/phase4-spec.md)); still-open
item there is symbolic display (KIV).

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
