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
