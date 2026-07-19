# Start here — next session

**Last session:** 2026-07-19 (Session 14). The Session 13 batch was
**developer-verified on device** (fixes + features work; large-array
stats feel OK; "Computing..." shows), then **sub-phase 3C shipped in
full (3C.2-3C.8)**: suite now **625 checks**, lint clean, both boards
build, **flashed to the Pico 2** (boot verified over serial).
Conventions recorded as **D25** (resolves P3-4). What landed:

- **`math::dist`**: normal/t/chisq/F pdf+cdf+inv, binomial/poisson/
  geometric pmf+cdf on the cephes primitives + lgamma closed forms.
  **Two-sided CDFs** `cdf(lo, hi, ...)` (open tails +/-1e99),
  lower-tail `inv`, real-valued df, NaN on domain errors, TI integer
  rule for discrete args (pmf strict, cdf floors k).
- **Catalog/help**: 18 new rows (47 total, `kMaxCatalogEntries` -> 56),
  fp3/fp4 casts, FUNC-tab summary column yields to long signatures.
- **`dist` typed command** -> guided form (Distribution/Function
  cycles, InputLine param fields with shared named slots, Calculate
  shows the equivalent call + result, **updates Ans**).
- **Link fix worth knowing**: cephes had never actually linked before —
  `lgam` needs an `isfinite()` *function* newlib/macOS don't export;
  shim at `src/math/cephes_support.c` (in the cephes target + host
  tests; see `drivers/cephes/README.md`).
- **Python dev-deps rule (this session)**: use the gitignored `.venv`,
  track packages in `requirements-dev.txt` (mpmath — reference-vector
  generator `tests/host/gen_dist_vectors.py`).

**Session 14 eval (2026-07-19): 3C spot-check OK** (features, functions,
docs/catalogue). It produced a new **logged-not-fixed observation
batch** — [session14-observations-verbatim.md](session14-observations-verbatim.md):
Y=-editor long-expression overlap (truncate + `...`), SD "no card"
after extended power-off (**root cause found: late-init retries stop
at 30 s** — lean: retry forever on a slow heartbeat), red `SD`/`PSRAM`
top-bar health indicators, SD hot-plug unhandled (DET pin never polled
after boot). Work it as one storage-health batch + one small UI fix;
record decisions when made.

## The next job

1. **On-device eval** of the outstanding batches (worklog HW-PENDING):
   Session 11 (3A lists sweep) and Session 12 (3B stats sweep — timing
   question already resolved).
2. **The Session 14 observation batch** (above) — storage health
   (retry-forever + indicators + hot-plug) and the editor truncation.
3. Then **sub-phase 3D** (`phase3-spec.md` §6, weeks 24-25): inference
   (hypothesis tests + confidence intervals over `math::stats` +
   `math::dist`), inference UI, and the StatPlot layer. Open questions
   to decide there: **P3-5** (stat plots vs Y-slots enable UI, task
   3D.13) and **P3-6** (always-compute paired CIs, task 3D.8). The
   sub-phase ends with **3D.14 — the combined Pico 1 pass (D18)**:
   reflash `build/pico/…uf2` first; note Sessions 11-14 added static
   SRAM and ~30 KB of text (cephes now really links) — re-check the
   map file there (bss ~135 KB of 264 KB as of Session 14).

Mind the §8 strip-safety rule (idempotent `render()` — StatsScreen and
DistScreen follow it: compute in on_key, cached result lines).

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Firmware on the unit is the Session 14 build** (3C distributions on
  top of everything prior; flashed 2026-07-19, `psram-bulk: OK` +
  battery heartbeat seen on serial — the BOOTSEL volume mounted in ~5 s
  and cp exited 0 again). The Pico 1 is still on Session 7 firmware;
  its pass is deferred to post-Phase 3 (D18/3D.14).
- **`lists.dat` may not exist yet on the SD card** — first save creates
  it. If a load ever misbehaves, deleting the file resets all lists
  (magic PCL1; bump to PCL2 on layout change).
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
