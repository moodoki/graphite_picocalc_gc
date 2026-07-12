# Start here — next session

**Last session:** 2026-07-12 (Session 7, very long). Lint baseline made clean
and gating; **Phase 2 essentially code-complete in one session**: 2.1–2.23 +
2.26–2.28 (graph/ subsystem, parametric + polar modes, tables, split-screen,
unified persistence, built-in help). Only 2.22-polish / 2.24 / 2.25 remain, and
2.24 **is** the test drive below. Tree = committed; **on-device firmware is a
full session behind — reflash both boards first.**

Read `docs/notes/worklog.md` (Session 7 + HW-PENDING queue) for the full story.
This file is the short "what's next".

## Current state

- **Phase 2 code-complete except 2.22-polish/2.24/2.25.** Spec:
  `docs/phases/phase2-spec.md` (§13 open questions all resolved). Decisions
  D15 (editor base), D16 (split-screen) in decisions.md.
- **Lint is a real gate**: `./scripts/lint.sh` = clang-format + clang-tidy,
  `WarningsAsErrors: '*'`, exits non-zero on any finding. Run before commits.
- **Careful with `clang-tidy --fix`**: it has produced invalid
  `const char const*` and const-ified written-through pointers. Always
  rebuild + re-lint after.
- **Dirty-band rendering (D13):** home screen + editors track dirty row
  bands; a missed `invalidate()` = stale rows — watch for it in `on_key`.
- **STM32 fw is v1.6**. **Never poll the STM32 aggressively** —
  back-to-back register reads wedge it (physical power cycle to recover).
- **Pico 2 debug notes:** BOOTSEL volume is `RP2350` (not `RPI-RP2`);
  1200-baud reset works; boot printfs race USB enumeration — buffer late.
- **Session protocol:** read this file first when starting fresh; update it
  before ending a session.

## THE test drive (= task 2.24) — full Phase 2 checklist

Flash fresh `build/pico/…uf2` + `build/pico2/…uf2` first. Capture USB serial
(`cat /dev/cu.usbmodem*`) — the `graph recompute: N us` lines during the graph
tests are the 2.25 perf baseline (function vs parametric vs polar vs split).

1. **Regression parity (refactors should be invisible):**
   - Graph function mode (2.1/2.3): plot 2–3 Y-funcs, trace, zoom, S/T
     presets — pixel-identical to Phase 1 expectations.
   - Y= editor (2.5 base extraction): row nav, inline edit, F2 toggle,
     F3 clear, dirty-band feel while typing (no stale rows).
   - Home screen basics still fine (eval, history, store op) — engine's
     build_lookup was refactored onto the catalog (2.26).
2. **Persistence + migration (2.23) — do this EARLY, order matters:**
   - First boot after flash: existing Y-funcs + window must reappear
     (migrated from yfuncs.txt/window.dat; `graphstate.dat` appears on SD;
     old files remain but are ignored).
   - After setting up parametric/polar/table state below: cold power
     cycle → curves, graph mode, T/TH ranges, table config all survive.
   - **Pico 2 cold boot (D14 interplay):** with the ~5-8 s rail settle,
     storage arrives late → graph state must still load when the SD
     late-init lands (watch serial for the late-mount + load).
3. **Parametric acceptance (weeks 11-12 gate):** MODE → Graph mode →
   PARAM. WINDOW now shows Tmin/Tmax/Tstep rows. Graph F5 → parametric
   editor: X1T=cos(t) commit **auto-focuses empty Y1T** (§5.1); pair
   checkbox sits on the X row; pair auto-enables when both halves filled.
   X1T=cos(t), Y1T=sin(t) → circle; cos(3t)/sin(2t) → Lissajous; trace
   readout `P1 t= x= y=`; UP/DOWN switches pairs.
4. **Polar acceptance (week 13 gate):** POLAR mode; WINDOW shows
   THmin/THmax/THstep (name column widens). F5 → polar editor.
   r1=1+cos(theta) → cardioid; r2=2*sin(3*theta) → rose. **Both angle
   modes**: in DEGREE set THmax=360, THstep≈5.7 — same shapes. Trace
   readout `r1 th= x= y=` (th in current mode's units).
5. **Tables (2.12–2.18):** Graph F4 → TBL in each mode (x|Y.., T|X1T
   Y1T.., th|r..). Auto scrolls infinitely both directions (up past
   Start goes negative). F1 setup: Start/Step edit, AUTO/ASK toggle;
   F2 jumps to Step. ASK: ENTER adds values, F5 deletes, empty-state
   hint. LEFT/RIGHT column scroll with 4+ enabled functions (`<`/`>`
   markers). Detail line shows full precision. **Watch scroll latency**
   — per-row compile is the known 2.25 lever if it drags.
6. **Split-screen (2.19–2.21, D16):** F9 (=Shift+F4) from graph or table
   → split; F4 switches focused pane (white edge marks focus); ESC/F9
   exits. **Pane clipping on the Pico 1 strip renderer** — no bleed
   across the divider, no artifacts. Trace sync: tracing moves the
   nearest table row; table selection places the trace cursor — all
   three modes. Judge the nearest-row feel (option b "trace steps by
   table-step" is the KIV upgrade, D16). Frame time budget ~1.5x.
   Pushed screens from a pane (setup, editors) open full-screen and
   return to split.
7. **Help (2.26–2.28):** Home F5 → HELP (softkey now labeled). FUNC tab
   lists all 17 functions; KEYS covers new F4/F9/TABLE/SPLIT bindings;
   SYNTAX covers store/e-E/theta/graph modes; LEFT/RIGHT tabs, UP/DOWN
   scroll + position indicator.
8. **Keymap sanity (D16 scheme):** Graph F4=TBL, table F4=back to graph,
   F9 toggles split everywhere relevant; MODE row cycles
   FUNC/PARAM/POLAR and **persists across reboot**.
9. **KIV extras while the units are out:** Pico 2 functional sweep
   (eval/graph/dirty-band feel/persistence — display/kbd/PSRAM/SD already
   verified); charging color once battery <95%; form an opinion on the
   F-key layout rethink (feedback item 7) — help KEYS content must be
   revised if it changes.

## After the test drive

- Log results in worklog (clear the HW-PENDING table), fix fallout.
- Close 2.22 (decide what "mode selector integration" needs beyond the
  MODE row), 2.24 (this drive), 2.25 (perf: use the captured recompute
  numbers; levers = table compile-once-per-regenerate, parametric step
  tuning). **That completes Phase 2** → retro, then Phase 3 spec
  (`docs/phases/phase3-spec.md`).
- Decide on option b trace sync + pane sizes based on split feel (D16).

## Backlog (not blocking, but tracked)

- Root-cause the Pico 2 rail settle electrically (scope 3V3 on cold boot) — D14
  works around it; matters if Phase 3/4 needs PSRAM at boot.
- Bulk PSRAM transfer hangs on HW — root-cause before Phase 3/4 (needs it). (D10)
- Dual-core display stall never fully diagnosed (worked around with sync core-0
  render). (D10)
- Graph-screen latency: full-frame by design (D13) — if trace/zoom feel slow, the
  lever is SPI clock / DMA / plot-region caching, not bands.
- Parameter-mode curve cache caps at 340 points/curve — very small T/TH steps
  truncate the drawn curve (documented; revisit if it bites).
- No audio HAL (pwm_sound vendored/linked but unused).
- **Fully permissive (all-MIT) release** — future option, NOT on any roadmap
  (D17). Effort scoped in NOTICE.md: rewrite lcdspi from the datasheet
  (~2-3 sessions incl. HW), rewrite i2ckbd (~1), replace font1 with a
  permissive font (~1, overlaps D9), drop pwm_sound, verify Coyote's own
  GPL provenance upstream. Until then: own code MIT, firmware GPL-2.0.
- Deferred: DMA push, ZoomFit + axis tick labels, 8x16 font (D9), unbounded
  history file (D4), unused overclock, `float` graph-eval lever (D5), `rand()`
  unseeded (NOLINTed), key auto-repeat suppressed, 2nd/Alpha indicators not
  driven.

## Hardware debugging kit (reminder)

- Reset to BOOTSEL without touching the board: `stty -f /dev/cu.usbmodem* 1200`, then
  `cp build/pico/picocalc_graphcalc.uf2 /Volumes/RPI-RP2/` (Pico 1) or
  `cp build/pico2/picocalc_graphcalc.uf2 /Volumes/RP2350/` (Pico 2).
- USB serial: `cat /dev/cu.usbmodem*`.
- `picocalc_diag` target (`src/diag_main.cpp`) = vendored-only display test for bisecting.
- Build env: `PICO_SDK_PATH=$PWD/pico-sdk`,
  `PICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.
