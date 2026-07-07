# Phase 0: Project Preparation

**Goal**: Get from "empty repository" to "ready to start Phase 1, week 1, task 1.1." Everything in this phase is one-time setup — environment, repository bootstrap, vendored drivers, baseline docs, and tooling.

**Estimated effort**: 1–2 days (8–12 hours).

**Exit criteria**: a developer can clone the repo, run `./scripts/build-all.sh`, and get two `.uf2` files. The "blink" smoke test runs on both Pico 1 and Pico 2 hardware. clangd works in the IDE without phantom errors. The agent workflow is operational.

---

## 0.1 Development environment (host machine)

Target host: macOS on Apple Silicon. Adapt as needed for other platforms.

### 0.1.1 Install host toolchain

```bash
# Homebrew dependencies
brew install cmake ninja git python3 clang-format clangd llvm

# Note: Homebrew's clang-format is pinned to current clang version.
# clang-tidy comes with the llvm formula.
brew link --force llvm  # Optional — exposes clang-tidy in PATH
```

### 0.1.2 Install ARM GNU Toolchain

Download the **official Apple Silicon native** package from ARM, not Homebrew's `gcc-arm-embedded` formula. The Homebrew version has historically lagged and occasionally ships without `newlib`, causing `nosys.specs` errors.

1. Visit https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
2. Select: **macOS (Apple silicon)** → **AArch32 bare-metal target (arm-none-eabi)**
3. Download the `.pkg` installer and run it.
4. Installs to `/Applications/ArmGNUToolchain/<version>/arm-none-eabi/`.

Verify:

```bash
/Applications/ArmGNUToolchain/*/arm-none-eabi/bin/arm-none-eabi-gcc --version
# Should report a version >= 13.2
```

### 0.1.3 Set environment variables

Add to `~/.zshrc`:

```bash
# Pico SDK
export PICO_SDK_PATH="$HOME/pico/pico-sdk"

# ARM toolchain (replace <version> with the actual version, e.g., 14.2.rel1)
export PICO_TOOLCHAIN_PATH="/Applications/ArmGNUToolchain/<version>/arm-none-eabi"

# Build system: prefer Ninja over Make
export CMAKE_GENERATOR="Ninja"
```

Reload the shell or run `source ~/.zshrc`.

### 0.1.4 Install Pico SDK

```bash
mkdir -p ~/pico
cd ~/pico
git clone -b master --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
```

The SDK should be on the `master` branch with submodules initialized (`tinyusb`, `cyw43-driver`, etc.).

### 0.1.5 Verify the toolchain with the official "blink" example

This is the smoke test for the host environment, separate from this project:

```bash
cd ~/pico
git clone -b master https://github.com/raspberrypi/pico-examples.git
cd pico-examples
mkdir -p build/pico1 build/pico2

cmake -G Ninja -DPICO_BOARD=pico  -B build/pico1 -S .
cmake --build build/pico1 --target blink

cmake -G Ninja -DPICO_BOARD=pico2 -B build/pico2 -S .
cmake --build build/pico2 --target blink

# Should produce build/pico1/blink/blink.uf2 and build/pico2/blink/blink.uf2
ls -la build/*/blink/*.uf2
```

If both `.uf2` files exist, the host toolchain is good.

### 0.1.6 Hardware smoke test

Flash the blink `.uf2` to a bare Pico (without the PicoCalc) to confirm the BOOTSEL/UF2 workflow:

1. Hold BOOTSEL on the Pico.
2. Plug in USB-C — Pico mounts as `RPI-RP2` volume.
3. `cp build/pico1/blink/blink.uf2 /Volumes/RPI-RP2/`
4. Pico reboots; the on-board LED blinks.

Repeat with the Pico 2 module.

---

## 0.2 Repository bootstrap

### 0.2.1 Initialize the project

```bash
git init picocalc-graphcalc
cd picocalc-graphcalc
git branch -M main
```

### 0.2.2 Create the directory structure

```bash
mkdir -p \
  src/{platform,gfx,ui,math/cas,render,apps,scripting} \
  drivers \
  assets \
  docs/{phases,notes} \
  scripts \
  .clangd .vscode .github/workflows
```

Note: `.agent/` and `.claude/` directories will appear via tooling; they're gitignored.

### 0.2.3 Stub `src/main.cpp`

Minimum buildable program for early CMake validation. Just blinks the on-board LED — we'll replace this in Phase 1, task 1.7 with the real entry point.

```cpp
// src/main.cpp - Phase 0 stub
#include "pico/stdlib.h"

int main() {
    stdio_init_all();
    const uint LED_PIN = 25;  // Pico 1 onboard LED; differs on Pico 2
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
    }
}
```

### 0.2.4 Create the top-level `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)

# Pull in Pico SDK
include(pico_sdk_import.cmake)

project(picocalc_graphcalc C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Always export compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

# Board detection: PICO_BOARD passed as -DPICO_BOARD=pico or pico2
if(PICO_BOARD STREQUAL "pico2")
    add_compile_definitions(PICOCALC_PICO2=1)
    message(STATUS "Building for PicoCalc with Pico 2 H (RP2350)")
else()
    add_compile_definitions(PICOCALC_PICO1=1)
    message(STATUS "Building for PicoCalc with Pico 1 H (RP2040)")
endif()

# Common compile flags
add_compile_options(
    -Wall
    -Wextra
    -Wshadow
    -Wpedantic
    -fno-exceptions
    -fno-rtti
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
)

add_executable(picocalc_graphcalc
    src/main.cpp
)

target_include_directories(picocalc_graphcalc PRIVATE src/)

target_link_libraries(picocalc_graphcalc
    pico_stdlib
    pico_multicore
)

# Generate .uf2, .bin, .hex, .map alongside the .elf
pico_add_extra_outputs(picocalc_graphcalc)

# Enable USB stdio for development (printf over USB serial)
pico_enable_stdio_usb(picocalc_graphcalc 1)
pico_enable_stdio_uart(picocalc_graphcalc 0)
```

### 0.2.5 Add `pico_sdk_import.cmake`

Copy from the Pico SDK's `external/` directory:

```bash
cp $PICO_SDK_PATH/external/pico_sdk_import.cmake .
```

### 0.2.6 First build

```bash
cmake -G Ninja -DPICO_BOARD=pico  -B build/pico1 -S .
cmake -G Ninja -DPICO_BOARD=pico2 -B build/pico2 -S .

cmake --build build/pico1
cmake --build build/pico2

# Verify .uf2 files exist
ls -la build/pico1/picocalc_graphcalc.uf2
ls -la build/pico2/picocalc_graphcalc.uf2
```

Both must build successfully. Flash the Pico 1 build to the PicoCalc to confirm the on-board LED blinks. (The Pico 2 LED is on a different pin — fix in Phase 1.)

---

## 0.3 Vendor third-party drivers

The HAL is built on top of Coyote OS's C drivers. Vendor them as **read-only copies** under `drivers/` — not git submodules, because we want stability and don't expect to track upstream changes.

### 0.3.1 Get Coyote OS source

```bash
# Clone to a scratch location
cd /tmp
git clone https://github.com/laingcc/Picocalc-Coyote-OS.git coyote-os
```

### 0.3.2 Copy specific subdirectories

Identify the driver modules in Coyote OS (likely paths — verify against the actual repo layout):

- `lcdspi/` — ST7365P SPI display driver
- `i2ckbd/` — STM32 I2C keyboard driver
- `rp2040-psram/` — 8 MB PSRAM driver
- `pwm_sound/` — PWM buzzer driver

```bash
cd <project-root>
mkdir -p drivers
cp -r /tmp/coyote-os/lcdspi drivers/
cp -r /tmp/coyote-os/i2ckbd drivers/
cp -r /tmp/coyote-os/rp2040-psram drivers/
cp -r /tmp/coyote-os/pwm_sound drivers/
```

Also vendor FatFs (for SD card support):

```bash
# FatFs source from elm-chan.org or PicoCalc community fork
# Download the latest release ZIP
curl -L -o /tmp/fatfs.zip http://elm-chan.org/fsw/ff/arc/ff15a.zip
unzip /tmp/fatfs.zip -d /tmp/fatfs
mkdir -p drivers/fatfs
cp /tmp/fatfs/source/* drivers/fatfs/
```

### 0.3.3 Document the vendoring

Create `drivers/README.md`:

```markdown
# Vendored drivers

Read-only third-party C drivers. Do not edit in place — wrap in `src/platform/`.

| Directory | Source | Version | License |
|-----------|--------|---------|---------|
| `lcdspi/` | Coyote OS, commit `<sha>` | <date> | <license> |
| `i2ckbd/` | Coyote OS, commit `<sha>` | <date> | <license> |
| `rp2040-psram/` | Coyote OS, commit `<sha>` | <date> | <license> |
| `pwm_sound/` | Coyote OS, commit `<sha>` | <date> | <license> |
| `fatfs/` | http://elm-chan.org/fsw/ff/ | R0.15a | BSD-style |

Update commit SHAs and dates after vendoring.
```

### 0.3.4 Add CMake integration (deferred)

Don't add the drivers to `CMakeLists.txt` yet. They're integrated incrementally during Phase 1 tasks 1.3 (display), 1.4 (keyboard), 1.5 (storage), 1.6 (PSRAM). Keeping them out of the build initially avoids early build breakage from driver-side issues.

---

## 0.4 Tooling configuration

### 0.4.1 `.clang-format`

Use a derivative of LLVM/Google style with project preferences. See [.clang-format](../../.clang-format) — agents don't need to memorize the rules.

### 0.4.2 `.clang-tidy`

Enable a lean, focused set of checks. See [.clang-tidy](../../.clang-tidy).

### 0.4.3 `.clangd`

Configure the language server for cross-compilation context. See [.clangd/config.yaml](../../.clangd/config.yaml). The key setting is `CompileFlags.QueryDriver` pointing at `arm-none-eabi-gcc`, which authorizes clangd to ask the cross-compiler about its built-in include paths and macros.

### 0.4.4 `.editorconfig`

Universal editor settings (line endings, indent width, charset) so any editor produces consistent output.

### 0.4.5 Helper scripts in `scripts/`

- `build-all.sh` — build both Pico 1 and Pico 2 targets.
- `format.sh` — run clang-format in place on all `src/**/*.{cpp,hpp,c,h}`.
- `lint.sh` — run clang-format `--dry-run --Werror` plus clang-tidy.
- `setup-clangd.sh` — symlink `compile_commands.json` from a build directory to the project root.
- `flash.sh` — copy a `.uf2` to the BOOTSEL volume (macOS-aware).
- `validate_md.py` — validate markdown documents (math mode, links, code blocks).

All scripts use `bash` (not `sh`) and `set -euo pipefail` at the top.

### 0.4.6 `.gitignore`

```gitignore
# Build artifacts
build/
*.uf2
*.elf
*.bin
*.hex
*.map

# clangd / language server
compile_commands.json
.cache/
.clangd/.clangd-state/

# Editor / IDE
.vscode/*.local.json
.idea/
*.swp
*.swo
.DS_Store

# Agent state (gitignored — personal to each developer)
CLAUDE.local.md
.claude/memory/
.agent/state/

# Python (for scripts/)
__pycache__/
*.pyc
.venv/
```

### 0.4.7 GitHub Actions CI (optional but recommended)

`.github/workflows/build.yml` — build both targets on every push, run lint, fail on warnings. Even for a personal project this catches regressions cheaply.

---

## 0.5 Documentation skeleton

Create stubs for the documents that Phase 1 work will reference and gradually fill in:

- `docs/architecture.md` — layered architecture, layer rules, dual-core model
- `docs/hardware.md` — Pico 1 vs Pico 2 specs, mainboard peripherals, pinout reference
- `docs/dev-environment.md` — toolchain setup (this file plus more)
- `docs/dependencies.md` — list of all third-party code, sources, licenses
- `docs/phases/phase1-spec.md` — already exists (the design contract)
- `docs/phases/phase1-plan.md` — task list with status tracking (this is the active working doc)
- `docs/phases/phase4-spec.md` — already exists (CAS, matrix, MicroPython)
- `docs/notes/feasibility.md` — copy of the original feasibility report
- `docs/notes/decisions.md` — running log of architecture/library/UX decisions with rationale

---

## 0.6 Agent workflow setup

### 0.6.1 Confirm `AGENTS.md` is present and current

`AGENTS.md` lives at the project root. It's the canonical agent context file. Cross-tool — works with Claude Code, Cursor, Codex CLI, and others.

### 0.6.2 Confirm `CLAUDE.md` points at `AGENTS.md`

Symlink or 3-line stub — see the existing `CLAUDE.md`.

### 0.6.3 Create personal `CLAUDE.local.md` (gitignored)

Per-developer preferences that don't get committed:

```markdown
# CLAUDE.local.md

Personal preferences for this project, not committed to git.

## Communication style

Concise. Don't recap before answering. Skip the "Great question!" preamble.

## Workflow preferences

I run builds manually after changes; don't run them yourself unless I ask.
For long file edits, show a diff summary before committing changes.

## Current focus

Currently working on: <task number>
```

### 0.6.4 Skills directory (optional)

If using Claude Code or similar tools that support skills, create `.claude/skills/` with project-specific skills. Initial skill ideas (defer until they're actually needed):

- `picocalc-build` — encapsulates the build process, common errors, and recovery steps
- `pico-hal-wrapper` — pattern for wrapping a vendored C driver in a C++ HAL class
- `phase-task` — workflow for picking up a task from the phase plan, implementing, validating, and committing

Don't create these speculatively. Add a skill when the same workflow has been done manually 2–3 times and benefits from being automated.

### 0.6.5 Markdown validation tool

Copy `scripts/validate_md.py` (the one used to validate the phase specs) into the repo. Document its usage in `AGENTS.md` and `CONTRIBUTING.md` (if added later).

---

## 0.7 First commit and baseline

```bash
git add .
git commit -m "Phase 0: initial project skeleton"
```

The repo now has:

- A buildable `main.cpp` blink stub (both targets compile, blink runs on hardware)
- All vendored drivers in place (not yet integrated into build)
- Tooling configured (clang-format, clang-tidy, clangd, EditorConfig)
- Scripts for build/format/lint/flash/setup
- Documentation skeleton (specs, architecture stub, hardware stub, etc.)
- Agent workflow operational (`AGENTS.md`, `CLAUDE.md`)
- CI building both targets on every push (if Actions enabled)

**This is the Phase 0 exit point. Phase 1 work can begin from here.**

---

## 0.8 Phase 0 task checklist

Mark these off as you complete them:

- [ ] **0.1.1** Host toolchain installed (cmake, ninja, python3, clang-format, clangd, llvm)
- [ ] **0.1.2** ARM GNU Toolchain installed (Apple Silicon native)
- [ ] **0.1.3** Environment variables set in `~/.zshrc`
- [ ] **0.1.4** Pico SDK cloned with submodules
- [ ] **0.1.5** Pico examples blink builds for both Pico 1 and Pico 2
- [ ] **0.1.6** Hardware smoke test: blink runs on bare Pico 1 and Pico 2 modules
- [ ] **0.2.1** Repo initialized
- [ ] **0.2.2** Directory structure created
- [ ] **0.2.3** `src/main.cpp` blink stub committed
- [ ] **0.2.4** Top-level `CMakeLists.txt` committed
- [ ] **0.2.5** `pico_sdk_import.cmake` committed
- [ ] **0.2.6** Both `picocalc_graphcalc.uf2` files build successfully
- [ ] **0.3.1** Coyote OS source obtained
- [ ] **0.3.2** Drivers vendored to `drivers/`
- [ ] **0.3.3** `drivers/README.md` documents sources, versions, licenses
- [ ] **0.4.1** `.clang-format` committed
- [ ] **0.4.2** `.clang-tidy` committed
- [ ] **0.4.3** `.clangd/config.yaml` committed
- [ ] **0.4.4** `.editorconfig` committed
- [ ] **0.4.5** `scripts/` populated with build/format/lint/flash/setup-clangd helpers
- [ ] **0.4.6** `.gitignore` committed
- [ ] **0.4.7** CI workflow committed (if using GitHub Actions)
- [ ] **0.5** Doc skeleton committed (architecture, hardware, dev-environment, dependencies)
- [ ] **0.6.1** `AGENTS.md` reviewed and current
- [ ] **0.6.2** `CLAUDE.md` points at `AGENTS.md`
- [ ] **0.6.3** Personal `CLAUDE.local.md` created (gitignored)
- [ ] **0.6.5** `scripts/validate_md.py` committed
- [ ] **0.7** First commit pushed to remote (if using one)

Once everything is checked, proceed to [phase1-plan.md](phase1-plan.md).
