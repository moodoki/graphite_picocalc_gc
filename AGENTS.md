# AGENTS.md

Instructions for AI coding agents working on this project. Keep this file lean and concrete. If a linter/formatter can enforce a rule, put it there instead.

## Project

PicoCalc Graphing Calculator — a TI-83/84-inspired graphing calculator firmware for the ClockworkPi PicoCalc. Native C++17 on the Raspberry Pi Pico SDK. Targets both Pico 1 H (RP2040) and Pico 2 H (RP2350). Solo developer, personal-use project.

Phases: see `docs/phases/`. Phases 0–4 (sub-phases 4A–4D) and Phase 5 (CAS) are complete and hardware-verified on both boards — Phase 4's completion was the pre-release milestone, a feature-complete TI-83/84+-class graphing calculator; Phase 5 added symbolic math on top. Next up is Phase 5.1 (serial line injection), then Phase 5.2 (the unified evaluator), then Phase 6 (apps). See `docs/notes/next-session.md` for the live handoff.

**Sub-phase naming — two schemes, and they mean different things:**

- **Letters** (`4A`, `4B`, `6A`, …) — planned work that progresses toward the parent phase's completion. The phase is not done until its lettered sub-phases are.
- **Dots** (`5.1`, `5.2`, …) — work that *turned up* during or after the parent phase: a significant unit in its own right, but **not part of that phase's planned goals**, and not big enough to justify its own phase number. A dotted sub-phase does not gate the parent phase's completion; Phase 5 was closed before 5.1 and 5.2 existed.

Task IDs follow the same split: `4D.22` for lettered, `5.1.3` for dotted. Note `§5.1` inside a spec still means a *section* reference — always write the `Phase ` prefix in prose to disambiguate.

## Build commands

Builds use **Ninja**, not Make. `CMAKE_GENERATOR=Ninja` is set in the developer's shell, so plain `cmake` calls also work — but agents should pass `-G Ninja` explicitly to be safe.

```bash
# Required environment (this host — see docs/dev-environment.md):
export PICO_SDK_PATH="$PWD/pico-sdk"   # SDK 2.2.0, checked out in-repo
export PICO_TOOLCHAIN_PATH="/Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi"  # path carries the version

# Configure (once, or after CMakeLists.txt changes)
cmake -G Ninja -DPICO_BOARD=pico  -B build/pico -S .
cmake -G Ninja -DPICO_BOARD=pico2 -B build/pico2 -S .

# Build
cmake --build build/pico
cmake --build build/pico2

# Build a single target
cmake --build build/pico --target picocalc_graphcalc

# Clean
cmake --build build/pico --target clean

# Both boards in one shot (helper script)
./scripts/build-all.sh
```

Host toolchain quirk: `/opt/homebrew/bin/arm-none-eabi-gcc` (Homebrew formula) lacks newlib and fails at link with `nosys.specs: No such file or directory`. Always set `PICO_TOOLCHAIN_PATH` to the ArmGNUToolchain install above so the SDK picks the right compiler.

The CI matrix builds both boards. **Both must succeed before any code change is considered complete.** Pico 1 has stricter constraints (no FPU, less SRAM) so it surfaces errors Pico 2 hides.

## Lint, format, and static analysis

Run these before declaring work done. Don't put their rules in this file — they're authoritative on their own.

```bash
# Format (modifies files in place)
./scripts/format.sh             # clang-format on all src/**/*.{cpp,hpp,c,h}

# Lint (read-only checks)
./scripts/lint.sh               # clang-tidy + clang-format --dry-run --Werror

# Markdown validation (math mode, links, code blocks)
python3 scripts/validate_md.py docs/**/*.md
```

Configuration files are authoritative — agents do not need to memorize their content:

- `.clang-format` — formatting rules
- `.clang-tidy` — static analysis checks
- `.clangd` — language server configuration
- `.editorconfig` — universal editor settings

## Language server

`clangd` is the C++ language server. It needs `compile_commands.json` to function. CMake regenerates this on every configure step (we set `CMAKE_EXPORT_COMPILE_COMMANDS=ON` in `CMakeLists.txt`). After running `cmake -B build/pico ...`, symlink the database to the project root so clangd finds it:

```bash
ln -sf build/pico/compile_commands.json compile_commands.json
```

The `scripts/setup-clangd.sh` script does this automatically. Re-run it after switching the active board target.

If clangd shows phantom errors on Pico SDK includes, the `--query-driver` flag in `.clangd` needs to point at the correct `arm-none-eabi-gcc` for the host. See `docs/dev-environment.md`.

## Code conventions

- **Language**: C++17 in `src/`, C in vendored `drivers/` (left as-is from upstream).
- **Naming**: `snake_case` for files and functions, `PascalCase` for classes, `kCamelCase` for constants, `UPPER_SNAKE` for macros only.
- **Headers**: pair every `.cpp` with a `.hpp`. Use `#pragma once`. No header-only implementations except trivial inlines.
- **Memory**: no `new`/`delete` in application code. All dynamic allocation goes through pool allocators (`platform::Psram`, `math::ExprPool`, `render::layout_pool`). Heap allocation is reserved for one-time init in `main.cpp`.
- **Floating point**: use `math::calc_t` (typedef'd to `double`), never raw `double`/`float`. This makes the precision-vs-speed tradeoff visible at every site and eases future migration.
- **Error handling**: no exceptions. Functions that can fail return `bool`, `std::optional<T>`, or a result struct with an explicit `ok` field. Disabled in build flags (`-fno-exceptions`).
- **Logging**: `printf` over USB serial during development. There is no `std::cout`. Wrap in `LOG_DEBUG(...)` / `LOG_ERROR(...)` macros that compile out in release builds.
- **No RTTI**: `-fno-rtti` is set. Don't use `dynamic_cast` or `typeid`.

## Architecture

Layered, top to bottom: `apps` → `ui` → `math` / `render` → `platform` (HAL) → vendored C drivers + Pico SDK. Application code never touches Pico SDK functions or hardware registers directly — always through `platform::*`.

The dual-target story: same source tree, both boards. Differences are gated by `PICOCALC_PICO1` / `PICOCALC_PICO2` in `src/config.hpp`. Avoid scattering `#ifdef` through application code — confine them to `config.hpp` constants and platform layer implementations.

Dual-core: core 0 runs the application, core 1 handles display DMA. Multicore FIFO is the only sanctioned cross-core communication. No shared mutable state without explicit synchronization.

Read `docs/architecture.md` before structural changes.

**Python bindings (`src/scripting/`, Phase 6B)** carry two rules that are not
style preferences — see D74 and D76:

- **A binding is a C glue function plus a C++ leaf, in separate files.**
  `mp_calc_module.c` converts arguments and builds Python objects;
  `calc_api.cpp` calls `math::` and never calls back into MicroPython. Not
  only `mp_raise_*` longjmps — `mp_obj_new_*` can trigger a GC, a `__del__`
  finalizer can run arbitrary Python during it, and a `MemoryError` leaves
  from there. Allocation happens after the C++ leaf has returned.
- **A binding starts ~1.8 KB into a 4 KB stack, and nothing below it is
  checked.** `MICROPY_STACK_CHECK` guards MicroPython's own recursion only.
  Measure every new binding with `-DPICOCALC_STACK_PROBE=ON` and give it a
  headroom requirement; do not reason about frame sizes.

## Hardware

- **Display**: $320\times320$ RGB565 IPS via SPI, ST7365P controller (~ILI9488-compatible).
- **Keyboard**: 67-key matrix, scanned by an STM32 co-processor on the mainboard, exposed over I2C.
- **Memory**: 8 MB SPI PSRAM and 32 GB SD card on the mainboard, available regardless of which Pico module is installed.
- **Module differences**: see `docs/hardware.md` for the Pico 1 vs Pico 2 spec table.

## Drivers

The HAL wraps C drivers vendored from Coyote OS (PicoCalc calculator firmware). Vendored sources live in `drivers/` and should be treated as **read-only third-party code**. Do not edit them in place — if a driver bug needs fixing, add a wrapper in `src/platform/` and document the workaround.

## Workflow expectations

1. Read the relevant phase plan in `docs/phases/` before starting a task.
2. For non-trivial work, write a short plan and confirm with the developer before coding. Phase 1 has tasks numbered (1.1, 2.1, ...); refer to them by number.
3. After making changes: build both boards, run lint, run format, validate any modified markdown.
4. Commit messages: imperative mood, prefix with subsystem (`platform: add display init`, `math: fix tinyexpr++ wrapper`, `docs: update phase 1 plan`).
5. Don't update `docs/phases/phase1-spec.md` casually — it's the design contract. Implementation notes go in commit messages or `docs/notes/`.

## What not to do

- Don't introduce heap allocation in hot paths (rendering, input, math evaluation).
- Don't use STL containers that allocate (`std::vector`, `std::string`, `std::map`). Use fixed-size arrays or pool-backed alternatives. `std::array`, `std::span`, `std::string_view` are fine.
- Don't pull in dependencies outside the SDK and the explicitly-vendored libraries. Adding a new dependency requires updating `docs/dependencies.md` and developer approval.
- Don't run `cmake --build` without specifying `-B <dir>` — there's no in-source build.
- Don't commit `compile_commands.json` (it's a symlink), `build/`, or `.uf2` artifacts. They're in `.gitignore`.
- Don't bypass the platform layer to call Pico SDK functions from `src/apps/`, `src/ui/`, `src/math/`, or `src/render/`.

## Files to read first

- `docs/notes/next-session.md` — start-here handoff; read at the start of every session
- `README.md` — landing page. Detail lives in the root docs it links to: `FEATURES.md` (capabilities), `USAGE.md` (device UI), `ROADMAP.md` (phase status + specs), `CONTRIBUTING.md` (build/test/lint, repo layout, developer-doc index). Keep the README a landing page — when a section grows past a paragraph, it belongs in one of those four.
- `docs/phases/phase5.2-spec.md` — the most recently completed implementation contract (unified evaluator); `docs/phases/phase6-spec.md` is the forward contract (app framework + MicroPython)
- `docs/architecture.md` — layer rules and rationale
- `docs/dev-environment.md` — toolchain details (macOS Apple Silicon)
- `src/config.hpp` — compile-time configuration, board detection

Related handoff notes: `docs/notes/next-bench-session.md` (deferred hardware
bench work), `docs/notes/wishlist.md` (historical — the backlog it used to hold
now lives in GitHub Issues).

## The backlog lives in GitHub Issues

Open work is tracked as issues, not in files. At the start of a session, after
reading `next-session.md`, run:

```sh
./scripts/gh-issues.py     # writes docs/notes/issues.local.md (gitignored)
```

That mirrors every open issue with its full body, in the shape `wishlist.md`
used, so the backlog is readable without a network round-trip per query. It is
**one-way** — edit issues on GitHub and re-run; nothing writes back.

What belongs where is settled in `docs/notes/issue-tracking.md`, and the test is
**could someone close this?** If not, it is a record and stays in the repo:
`decisions.md`, the phase specs, `worklog.md`, retros and measurements all stay.
