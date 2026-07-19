# Start here — next session

**Last session:** 2026-07-19 (Session 13). Worked the **on-device
observation batch** (`phase3A-3B-observations-verbatim.md`) from the
first real use of the 3A/3B firmware — all dispositions recorded as
**D24**; suite now **508 checks**, lint clean, both boards build,
**flashed to the Pico 2** (boot verified over serial). What landed:

- **Bug fixes**: brace-literal broadcast (`{1,2,3}+2`,
  `{1,2,3}+{2,2,2}`) via the new **lift-operand** mechanism (literals
  and wrapper calls bind as extra lift vars, so `cumsum(range(1,4))+1`
  composes too); HOME-nav breakage (`switch_to` no longer replaces the
  stack root); list-editor negative numbers no longer placeholder-gray.
- **From the request list**: `range(lo,hi[,step])` (inclusive, default
  step +/-1 toward hi); `mean`/`median`/`stdev` (+`std`) reductions;
  reduction args generalized to any list expression
  (`sum(range(1,10000))` — D22 bare-arg limitation lifted); `?`/`list`/
  `stat` command aliases; stats **"Computing..." indicator** (D23
  revisit closed); **pi glyph** baked at 0x7F (8x16) + pretty-print
  substitution (`bdf_to_utft.py --map`).
- **Parked on the wishlist** (D24.9): greek/subscript stats display,
  JuliaMono, scientific constants, unit conversions, >6 lists / SD
  list files.

## The next job

1. **On-device eval of the 3A + 3B + 13 batches** (Session 11, 12,
   **and 13** rows in worklog HW-PENDING): list editor + home list
   syntax (now incl. literals/range/reductions and the bug-fix checks),
   then the stats screen sweep (form feel, results, store→graph
   overlay, error paths, the 10000-element 1-Var timing +
   "Computing..." visibility).
2. Then **continue sub-phase 3C** (`phase3-spec.md` §5, weeks 22-23).
   **3C.1 is DONE (Session 12)**: cephes `cprob` subset vendored to
   `drivers/cephes/` (see its `README.md`) — `ndtr`/`ndtri`,
   `incbet`/`incbi`, `igam`/`igamc`/`igami` + deps, compiled as the
   `cephes` CMake lib with `gamma`/`erf`/`erfc` renamed to `cephes_*`
   (call them `extern "C"` under those names). The integer-df
   convenience wrappers (`stdtr`/`chdtr`/`fdtr`/`bdtr`/`pdtr`) were
   deliberately NOT vendored — build `math::dist` on the real-df
   primitives: t/F via `incbet`/`incbi`, chi-square/Poisson via
   `igam`/`igamc`/`igami`, binomial cdf via the `incbet` identity,
   pdfs/pmfs via `std::lgamma` closed forms (no cephes needed).
   Remaining 3C tasks:
   - **3C.2-3C.6**: `src/math/dist.{hpp,cpp}` wrappers + host tests
     (link `drivers/cephes/*.c` into the test with the same
     `-Dgamma=cephes_gamma -Derf=cephes_erf -Derfc=cephes_erfc`
     defines). Decide **P3-4** at 3C.2 (spec + D23 lean: TI-style
     two-arg `cdf(lo, hi, ...)`).
   - **3C.7**: catalog registration — `build_lookup` already does
     `TE_FUNCTION0 + arity` so arity 3-4 works; catalog.cpp needs
     fp3/fp4 helper casts, `kMaxCatalogEntries` bumped past ~43, and
     the help FUNC tab needs its summary column to yield to long
     signatures (`normal_cdf(lo,hi,mu,sd)` = 23 chars > the fixed
     19-char `kSummaryCol` — draw summary at
     `max(kSummaryCol, sig_width + 1)`).
   - **3C.8**: `dist` typed command, guided-entry form following the
     `stats` screen pattern (distribution + function + numeric param
     fields → result; numeric fields need InputLine rows like
     WINDOW's, not the L/R-cycle rows).
   Record the naming/UI calls as **D25** when made (D24 was taken by
   the Session 13 observation batch).

Mind the §8 strip-safety rule (idempotent `render()` — StatsScreen
follows it: compute in on_key, cached result lines) and task 3D.14
(combined Pico 1 pass, D18).

## D14 rail settle — NEXT BENCH SESSION

Moved to **[next-bench-session.md](next-bench-session.md)** to keep this page
short. Non-blocking (last deferred HW item); schematic findings, probe points,
and the full bench plan live there. Pull it back here only when a bench session
is actually scheduled.

## Key things to note — Pico 2 specific

- **Firmware on the unit is the Session 13 build** (observation-batch
  fixes on top of 3A+3B; flashed 2026-07-19, `psram-bulk: OK` + battery
  heartbeat seen on serial — the BOOTSEL volume mounted in ~5 s and cp
  exited 0 again). The Pico 1 is still on Session 7 firmware; its pass
  is deferred to post-Phase 3 (D18).
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
