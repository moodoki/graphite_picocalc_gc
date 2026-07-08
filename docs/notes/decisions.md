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
