# Architecture & Design Decisions

A running log of decisions made during development. Each entry captures the question, the choice, the rationale, and what was traded off. New entries go at the top.

Format:

```
## DXX: <decision title>

**Date**: YYYY-MM-DD
**Status**: Accepted | Superseded by DYY | Deferred
**Context**: <what triggered the decision>
**Decision**: <what was chosen>
**Rationale**: <why this over alternatives>
**Tradeoffs**: <what we gave up>
**Revisit when**: <conditions that should trigger reconsidering>
```

---

## D0: Track decisions in this file

**Date**: TBD (Phase 0)
**Status**: Accepted
**Context**: Solo project, but decisions made early (e.g., expression engine choice, HAL layering) need to be traceable later when their consequences surface.
**Decision**: Maintain `docs/notes/decisions.md` as a chronological log. Append entries as decisions are made, not retrospectively.
**Rationale**: Future-me will not remember why the math engine uses `double` instead of `float`, or why the framebuffer lives in PSRAM on Pico 1. A log is cheaper than re-deriving.
**Tradeoffs**: Minor maintenance overhead. Mitigated by keeping entries short (~10 lines).
**Revisit when**: Project becomes multi-developer and an ADR (Architecture Decision Record) format with separate files might be preferable.

---

## D14: Deferred PSRAM/SD late-init for the RP2350 cold-boot rail settle

**Date**: 2026-07-11
**Status**: Accepted
**Context**: First Pico 2 (RP2350) bring-up: display, keyboard, and battery came up fine, but PSRAM and SD both failed on **cold power-on** and both worked on every warm reboot. Instrumented cold-boot traces (buffered `dbg_log`, dumped over USB serial) measured the failure directly: at 0.5 s the PSRAM reads back zeros; at 0.6-2.5 s it returns almost-correct data (single bit errors, or the whole word shifted one bit — analog-marginal behavior, independent of SPI clock: same at 75 MHz and 18.75 MHz); at 7.5 s it is perfect. The SD card answers CMD0/CMD8 immediately (comms fine, R7 voltage echo clean) but never completes ACMD41 — its power-sensitive init — until, at 7.5 s, it inits instantly. Conclusion: the peripheral rail needs **~5-8 s to settle after cold power-on with the Pico 2 module**; the Pico 1 module doesn't exhibit this (Phase 1 was fully verified on it, same mainboard, same card). Community reports match (RP2350 PSRAM cold-boot failures; fuzix SD failure on PicoCalc Pico 2).
**Decision**: Do not block boot. Boot-time init runs as always (instant home screen); if PSRAM or SD failed, the main loop retries every 2 s for the first 30 s of uptime: PSRAM via `Psram::reinit()` (re-sends the chip reset through the already-configured PIO — deliberately not `psram_spi_init()`, which re-adds the PIO program and claims 2 DMA channels per call), SD via a fresh `Storage::init()` (f_mount re-runs `disk_initialize`). When storage arrives late, the self-tests re-run and history/variables/graph state load then; the current screen is fully invalidated so the UI reflects it.
**Rationale**: A calculator that boots in 0.3 s shouldn't stall 8 s on one board variant. Warm reboots and Pico 1 hit the success path at boot and never enter the retry loop.
**Tradeoffs**: During the first ~10 s of a Pico 2 cold boot, persistence isn't available yet and a failing SD attempt (card inserted, rail still low) blocks the loop up to ~1 s per retry — brief input lag if the user types immediately. History appears a few seconds after boot rather than instantly.
**Revisit when**: The rail settle is understood at the hardware level (measure 3V3 with a scope; possibly a PicoCalc mainboard/Pico 2 SMPS interaction), or a keyboard-firmware/mainboard revision changes the power path. If Phase 3/4 needs PSRAM immediately at boot, reconsider a short blocking wait with a splash.

## D13: Opt-in dirty-band partial redraw (rows, not rectangles)

**Date**: 2026-07-11
**Status**: Accepted; HW-verified same day (typing instant, no stale-row artifacts)
**Context**: With synchronous full-frame rendering (D10), every keypress cost ~200 ms — the SPI push dominates (recompute is only 15-17 ms), and push time is proportional to pixel count. Task 5.6 part 2.
**Decision**: Screens track a dirty **row band** (`[y0, y1)`, full width); `ScreenManager::render_frame()` consumes it and `Framebuffer::render_frame()` renders/pushes only the strips inside the band. Tracking is **opt-in** per screen (`track_dirty()` in the constructor + `invalidate()` on every state change in `on_key`); non-tracking screens keep full-frame redraws. Any screen surfacing to top of the stack is fully invalidated by the manager. Opted in: home screen (typing = input band, ~28 of 320 rows; Enter = everything above the softkeys, which also keeps the battery/mode status fresh) and the Y= editor (per-row bands). An empty band skips the render+push entirely, so unconsumed keys cost nothing.
**Rationale**: A y-band is enough — the hot regions (input line, editor rows) are full-width, so x-cropping would add a strided push path for no measurable win. Opt-in keeps the default safe: a screen that never calls `invalidate()` can't accidentally stop redrawing.
**Tradeoffs**: Tracking screens must invalidate every band their key handler touches — a missed call shows as stale rows on the panel (visible, not corrupting). The battery indicator refreshes only on Enter/screen changes, not per keystroke. Renderers still run their full draw code per band (clipping discards out-of-band work), so CPU cost is unchanged — fine while push time dominates.
**Revisit when**: Graph interactions need help (trace/zoom redraw the ~280-row plot area anyway, so bands don't win there — that wants a faster SPI clock, DMA, or plot-region caching); or a screen needs non-full-width updates.

## D12: Shell-style input recall on UP/DOWN; modifier+arrows scroll the view; HOME pops to root

**Date**: 2026-07-11 (revised same day after HW verification)
**Status**: Accepted; scroll modifier revised to Alt/Ctrl
**Context**: On hardware, UP recalled only the newest expression once, then further UP scrolled the output view — no way to walk back through older inputs. The HOME key did nothing visible.
**Decision**: Plain UP/DOWN walk backward/forward through past inputs (the in-progress line is stashed and restored); **Alt+UP/DOWN or Ctrl+UP/DOWN** scroll the history view. HOME pops to the home screen from any screen (global intercept in the main loop, like F6); on the home screen it falls through to the input line's cursor-to-start. *Revision:* Shift was the original scroll modifier, but HW verification (2026-07-11) showed the STM32 swallows Shift on arrow keys (it emits a shift-release then a plain arrow); Alt and Ctrl pass through with flags intact, so scroll moved to them. Shift is still accepted in case a future keyboard firmware reports it.
**Rationale**: Shell-style recall is the behavior every terminal user expects, and the keyboard has no PgUp/PgDn — shift is the only spare modifier and its state is already tracked in `KeyEvent`.
**Tradeoffs**: Editing a recalled entry then pressing UP discards the edit (bash-like, not zsh-like). View scrolling is now two-handed.
**Revisit when**: a keyboard firmware update reports arrows with shift held. Re-checked on fw v1.6 (2026-07-11): still swallowed — the kShift press arrives with no arrow event at all — so Alt/Ctrl scroll stands.

## D11: `e` is Euler's constant; variable E is reserved

**Date**: 2026-07-11
**Status**: Accepted (test-drive feedback)
**Context**: `e` evaluated to 0 on the device. `build_lookup()` bound all 26 letters as variables, and tinyexpr consults the user lookup before its builtin table — so the variable E shadowed the builtin Euler constant (`pi`, being two letters, never collided).
**Decision**: Do not bind the letter `e` as a variable; `e` reaches tinyexpr's builtin constant. Storing to E (`5->E`) returns "E is reserved (Euler's e)". Convention: single letters = variables, `pi`/`e`/`theta`/`ans` and function names = reserved words.
**Rationale**: A calculator where `e` isn't 2.718... fails the least-surprise test; TI users rarely store to E (on TI it's the exponent token anyway). Case sensitivity (e vs E) was rejected — the preprocessor lowercases everything and the win isn't worth reworking that.
**Tradeoffs**: 25 letter variables instead of 26. The E slot still exists in `Variables` storage (persisted file format unchanged).
**Revisit when**: Someone actually misses variable E.

## D10: Synchronous core-0 rendering; PSRAM bulk path and dual-core display deferred

**Date**: 2026-07-10
**Status**: Accepted (from first hardware bring-up)
**Context**: First flash to real PicoCalc (Pico 1) showed a screen of random colors and a dead keyboard. Bisecting on hardware (vendored-only diagnostic + USB-serial boot tracing) found three distinct bugs:
1. **Boot hang** — `run_self_tests()` called the vendored *bulk* PSRAM transfer (`psram_read`/`psram_write`, 1 KB), which hangs on this hardware, even though single-word `psram_read32`/`write32` work. Boot froze after display init but before the first draw, so the panel showed power-on noise ("random colors").
2. **Dual-core display stall** — routing strip pushes through a core-1 service over the multicore FIFO stalled on the first frame.
3. **Dead keyboard** — the I2C read/write timeouts (2 ms) were shorter than a 2-byte transfer on the 10 kHz keyboard bus (~3.5 ms), so every read timed out.
**Decision**:
- Render **synchronously on core 0** using the vendored blocking `spi_write_fast` path (proven good by the diagnostic). Core 1 is left idle; `display_service_main` and `push_rect_dma` are retained but unused, as the basis for a future revisit.
- Quarantine the **bulk PSRAM** API (`Psram::read`/`write`) as known-hanging; expose and use only single-word `read_word`/`write_word`. Phase 1 needs no bulk PSRAM (framebuffer is line-buffered in SRAM).
- Set the keyboard I2C timeout to 100 ms (`kI2cTimeoutUs`), comfortably above the 10 kHz transfer time.
- Rendering is **event-driven**: a full-frame push is ~200 ms (5 fps), so redraw only after a key press, not every loop.
**Rationale**: Get a correct, working calculator on hardware first. The DMA push, dual-core split, and bulk PSRAM are all optimizations/future-phase needs, not Phase 1 requirements; each is a separate investigation.
**Tradeoffs**: ~200 ms full-screen redraw latency per keypress (single-threaded, full-frame). Acceptable for a calculator; the fix is dirty-rectangle / partial updates (and possibly a faster SPI clock or revisiting DMA), tracked for task 5.6.
**Revisit when**: task 5.6 performance work — profile, then add partial updates and re-evaluate DMA/dual-core and the bulk PSRAM transfer (needed for Phase 3 statistics / Phase 4 matrices).

## D3: Trace coordinate readout at the bottom of the viewport

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision — show the trace `(x, y)` at the top or bottom of the graph? Top risks overlapping the plotted curves near the peak; bottom risks the softkey bar.
**Decision**: Bottom of the viewport, in a dark strip just above the softkey bar, matching the TI-84.
**Rationale**: Curves cluster around the top/middle more often than the very bottom edge; TI users expect it there.
**Tradeoffs**: A curve that dips to the bottom edge is briefly obscured by the readout. Acceptable.
**Revisit when**: A cleaner overlay (semi-transparent, or auto-repositioning away from the cursor) is worth the code.

## D5: Keep `double` for graph evaluation (float deferred)

**Date**: 2026-07-08
**Status**: Deferred (revisit after hardware profiling)
**Context**: Open decision — use `float` instead of `double` for graph point evaluation on Pico 1 (no FPU) to roughly halve softfloat cost?
**Decision**: Keep `double` (`math::calc_t`) everywhere for now, including the graph sweep. The compile-once/eval-many path already removes the dominant cost (re-parsing per point), so evaluation is 320 `te_eval`s per function, not 320 compiles.
**Rationale**: Correctness first; can't profile without hardware. `calc_t` is a single typedef, so a `float` graph-eval variant is a localized change if profiling shows plotting is too slow.
**Tradeoffs**: Softfloat `double` is ~2x slower than `float` on RP2040; may matter with 7 functions. Measured lever, not a guess.
**Revisit when**: Task 5.6 profiling on real Pico 1 hardware shows graph render missing the <50 ms target.

## D2: Fractions stack only for "simple" operands

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision — should `a/b` always render as a stacked fraction, or only when the operands are simple? Always-stacked is less code; a heuristic reads better for messy expressions.
**Decision**: Stack `a/b` into a `FractionNode` only when both sides are "simple" — a number, a variable, a parenthesized group, an already-built fraction, **a function call, or a power** (the last two added 2026-07-11). Otherwise render inline with a text `/`. Also require the division to be the first operator in its term (no chaining an inline `*` into a stacked fraction), so `a*b/c` stays inline.
**Rationale**: Matches the spec's section 6.2 guidance and TI behavior; `(x+1)/(x-1)` stacks (operands are parens) while `1+2/3+4` keeps `2/3` inline-sized within the sum. Keeps trees shallow and predictable.
**Tradeoffs**: A few expressions a user might expect stacked stay inline; acceptable and consistent.
**Revisit when**: User feedback, or when an equation editor needs full 2D editing (Phase 2+).
**Revision (2026-07-11)**: HW test drive hit the tradeoff — `1/sqrt(2)` rendered inline because a function call parses to an HBox. Calls (recognized structurally: `HBox[alpha-name, paren]`, which excludes unary-minus HBoxes) and superscripts now count as simple, so `1/sqrt(2)` and `x^2/2` stack.

## D1: Variable store operator is `->` (arrow)

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision from the spec — how to store a value into A-Z/theta. Options: TI `→`, `=`, `:=`, or a dedicated STO key.
**Decision**: Use ASCII `->` typed as two chars (e.g. `2->A`, `x^2->B`). The engine splits on the last `->` whose right side is a bare variable name; `=` stays free for future comparison/equation use.
**Rationale**: No special key mapping or font glyph needed now; reads clearly; avoids the `=` ambiguity the spec flagged. A dedicated STO key can emit `->` later without changing the engine.
**Tradeoffs**: Two keystrokes vs. one; `->` can't appear elsewhere in an expression (fine — it has no other meaning).
**Revisit when**: A physical STO/→ key is added, or equation solving needs `=`.

## D4: History persisted as plaintext TSV

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision — history storage format: plaintext vs binary.
**Decision**: Append `expr\tresult\n` lines to `/picocalc/history.txt`. On boot, read the last 8 KB and parse backwards into the ring buffer. Variables persist separately as a binary blob (`variables.dat`, 28 doubles).
**Rationale**: Plaintext history is debuggable and hand-editable; parsing cost is trivial at 50 entries. Variables are fixed-size binary because they're not meant to be edited and round-trip exactly.
**Tradeoffs**: History file grows unbounded (append-only) — a compaction pass is a future cleanup; 8 KB tail read caps what's loaded regardless.
**Revisit when**: History file size becomes a concern, or results need structured metadata.

## D6: RGB565 framebuffers, RGB666 on the wire

**Date**: 2026-07-08
**Status**: Accepted
**Context**: The Coyote OS panel init programs COLMOD 0x66 (18-bit color, 3 bytes/pixel over SPI) — the ILI9488-family serial interface does not accept RGB565. The spec assumed RGB565 end-to-end.
**Decision**: Keep all render buffers RGB565 (as spec'd); convert to 3-byte RGB666 in `platform::Display` during push, using 5-to-8/6-to-8 bit LUTs, chunked through two 4-scanline staging buffers so conversion overlaps DMA.
**Rationale**: Preserves the spec's memory budget (2 B/px buffers) and the proven panel init. Conversion is a few cycles/pixel on core 1, which is otherwise idle waiting on SPI.
**Tradeoffs**: 50% more SPI traffic than true 565 (~98 ms/full frame @ 25 MHz — partial updates and/or a higher SPI clock are the perf levers; see worklog).
**Revisit when**: Profiling (task 5.6) shows the panel accepts COLMOD 0x55 at speed, or SPI overclocking changes the math.

## D7: Non-blocking keyboard poll state machine

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Vendored `read_i2c_kbd()` sleeps 16 ms between the FIFO register select and the data read — unacceptable in a per-frame poll loop.
**Decision**: `platform::Keyboard::poll()` reimplements the same I2C protocol (reg 0x09, addr 0x1F) as a two-phase non-blocking state machine (select, then read at least 10 ms later). The vendored driver still provides bus init and scan-code reference.
**Rationale**: Keeps the main loop responsive; drivers stay unmodified.
**Tradeoffs**: Two places know the STM32 protocol (vendored driver + wrapper).
**Revisit when**: STM32 firmware changes its register map, or an interrupt-driven design is needed.

## D8: FatFs local config — LFN enabled, CP437

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Spec filenames (`variables.dat`) exceed 8.3; the FatFs default config has LFN off and Shift-JIS codepage tables (large flash cost).
**Decision**: `ffconf.h`: `FF_USE_LFN=1` (static buffer), `FF_CODE_PAGE=437`. Documented as a local modification (ffconf.h is FatFs's designated user-config file).
**Rationale**: Smallest change that supports the spec's file layout.
**Tradeoffs**: LFN=1 is not thread-safe — fine, all file I/O happens on core 0.
**Revisit when**: File I/O moves off core 0.

## D9: Interim 8x12 font (Coyote font1) instead of 8x16

**Date**: 2026-07-08
**Status**: Accepted (interim)
**Context**: Spec calls for an 8x16 font generated from a public-domain BDF; that conversion needs font tooling not yet in the repo.
**Decision**: Ship milestone 1 with the vendored Coyote OS `font1` (8x12, UTFT layout) behind `gfx::Font`, which reads any UTFT-format header. Generate proper 8x16 + 6x8 fonts before milestone 5.
**Rationale**: Unblocks all text rendering now; the Font abstraction makes the swap a data change.
**Tradeoffs**: Slightly smaller glyphs than designed; layout metrics tuned later.
**Revisit when**: Task 5.x polish, or when the math renderer needs multiple sizes (milestone 3).

---

<!-- New decisions go above this line. Below: pre-Phase-0 decisions captured retrospectively from the spec & feasibility report. -->

## D-prelude-3: Use C++17 with the Pico SDK

**Status**: Accepted (pre-Phase-0)
**Decision**: C++17 in `src/`, plain C in vendored `drivers/`.
**Rationale**: C++17 gives `std::optional`, `std::variant`, `constexpr if`, structured bindings, and inline variables — all useful for the architecture. The Pico SDK is C with C++ wrappers; both languages mix cleanly. C++20 modules are not yet practical with the SDK.
**Tradeoffs**: Slightly larger binaries than C-only, but well within our 2 MB Pico 1 flash budget. Some templates and STL features are off-limits because they allocate; this is documented in `AGENTS.md`.

## D-prelude-2: Layered architecture with strict HAL discipline

**Status**: Accepted (pre-Phase-0)
**Decision**: `apps → ui → math/render → platform → drivers + Pico SDK`. Application code never calls Pico SDK functions directly.
**Rationale**: Dual-target support (Pico 1 + Pico 2) is the project's tightest constraint. Without HAL discipline, target-specific code metastasizes and the dual build becomes unmaintainable.
**Tradeoffs**: Some duplication in trivial wrappers. Worth it for testability and target portability.
**Revisit when**: Adding a third target (e.g., desktop simulator).

## D-prelude-1: Coyote OS as driver foundation

**Status**: Accepted (pre-Phase-0)
**Decision**: Vendor Coyote OS's C drivers (`lcdspi`, `i2ckbd`, `rp2040-psram`, `pwm_sound`) as read-only third-party code under `drivers/`.
**Rationale**: Coyote OS is the only known PicoCalc-native firmware with working drivers for our target hardware. Reimplementing them from scratch costs weeks and gains nothing.
**Tradeoffs**: We inherit any bugs in those drivers. Mitigated by wrapping them in `platform/` so fixes/workarounds happen at one layer.
**Revisit when**: A driver bug is unfixable from the wrapper layer.

## D-prelude-0: Pico SDK + CMake + Ninja

**Status**: Accepted (pre-Phase-0)
**Decision**: Use the official Raspberry Pi Pico SDK with CMake (Ninja generator). `CMAKE_GENERATOR=Ninja` is set in shell environment.
**Rationale**: Standard, well-supported, dual-target ready (`-DPICO_BOARD=pico` / `pico2`). Ninja is faster than Make for incremental builds and integrates better with clangd's `compile_commands.json`.
**Tradeoffs**: Requires Ninja installation. Negligible.
