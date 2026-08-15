# Development Environment

This project targets development on **macOS Apple Silicon**. Adapt as needed for Intel Mac, Linux, or Windows.

## Required tools

| Tool | Version | Purpose |
|------|---------|---------|
| **arm-none-eabi-gcc** | $\geq$ 13.2 | ARM cross-compiler for RP2040/RP2350 |
| **CMake** | $\geq$ 3.20 | Build system generator |
| **Ninja** | latest | Fast build system (preferred over Make) |
| **clangd** | $\geq$ 18 | C++ language server |
| **clang-format** | matches clangd | Code formatter |
| **clang-tidy** | matches clangd | Static analysis |
| **picotool** | $\geq$ 2.0 | UF2 generation (required by Pico SDK 2.x) |
| **Python** | 3.10+ | Build helper scripts, markdown validation |
| **Git** | any modern | Version control, submodule management |

## Installation (macOS Apple Silicon)

### 1. Homebrew dependencies

```bash
brew install cmake ninja git python3 llvm picotool
brew install --cask docker  # Optional, only for CI work locally
```

`llvm` provides `clangd`, `clang-format`, and `clang-tidy`. Optionally:

```bash
brew link --force llvm  # Exposes clang-tidy in PATH
```

If you don't `brew link --force`, the tools are in `$(brew --prefix llvm)/bin`. Adjust your PATH accordingly:

```bash
echo 'export PATH="$(brew --prefix llvm)/bin:$PATH"' >> ~/.zshrc
```

### 2. ARM GNU Toolchain

**Installed on this host**: ARM GNU Toolchain **15.3.rel1** via the Homebrew cask `gcc-arm-embedded` (15.2.rel1 verified working 2026-07-08; upgraded in place by the cask since). The cask is a repackaging of ARM's official Apple Silicon installer and includes newlib.

```bash
brew install --cask gcc-arm-embedded
# → /Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi/
```

> **The install path carries the version, so it moves on every upgrade** — and a
> stale `PICO_TOOLCHAIN_PATH` does not fail cleanly. CMake reconfigures from a
> cached compiler path and reports "not a full path to an existing compiler
> tool"; `scripts/lint.sh` loses the toolchain's system include paths and buries
> the real output under hundreds of bogus `'cstddef' file not found` errors.
> `lint.sh` and `size-report.sh` now discover the newest installed toolchain when
> `PICO_TOOLCHAIN_PATH` is unset, but the exported value below and any existing
> `build/` cache still need updating by hand. To find the current one:
>
> ```bash
> ls -d /Applications/ArmGNUToolchain/*/arm-none-eabi | sort -V | tail -1
> ```
>
> After an upgrade, delete `build/pico` and `build/pico2` and reconfigure — the
> old compiler path is in `CMakeCache.txt`.

> **Warning — do not use the Homebrew *formula* `arm-none-eabi-gcc`.** It ships **without newlib** and every link fails with `cannot read spec file 'nosys.specs'` (confirmed on this host with formula version 16.1.0, 2026-07-08). If it's installed and linked at `/opt/homebrew/bin/arm-none-eabi-gcc`, either uninstall it or make sure `PICO_TOOLCHAIN_PATH` points at the ArmGNUToolchain install so the SDK never picks the formula's compiler off PATH.

Alternatively, ARM's official `.pkg` installer from https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads (**macOS Apple silicon** → **AArch32 bare-metal target, arm-none-eabi**) lands in the same location.

Verify:

```bash
/Applications/ArmGNUToolchain/*/arm-none-eabi/bin/arm-none-eabi-gcc --version
# arm-none-eabi-gcc (Arm GNU Toolchain 15.3.Rel1 ...) 15.3.1
```

### 3. Submodules

One submodule, MicroPython (Phase 6B), pinned to a release tag. CMake stops
with an explicit error if it is empty rather than failing later on a missing
header.

```bash
git submodule update --init --recursive
```

Its embed package is **generated at configure time** from that checkout, which
is why the build needs `make` and a host C compiler in addition to the cross
toolchain. See [drivers/README.md](../drivers/README.md) for what
`drivers/micropython_port/` is and why the submodule itself is never edited.

### 4. Pico SDK and picotool

The SDK is checked out **inside this repo** at `./pico-sdk` (gitignored), currently at tag **2.2.0** with submodules initialized. `./pico-examples` sits alongside it for reference and toolchain smoke tests. To recreate:

```bash
cd <repo-root>
git clone -b 2.2.0 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
git clone https://github.com/raspberrypi/pico-examples.git   # optional
```

The SDK supports both RP2040 and RP2350 from a single tree. Submodule init pulls in TinyUSB and other dependencies — required for many SDK features.

SDK 2.x also requires **picotool** to generate `.uf2` files. Install it via Homebrew (`brew install picotool` — v2.3.0 on this host); otherwise the first build tries to fetch and compile it from GitHub.

### 5. Environment variables

Add to `~/.zshrc`:

```bash
# Pico SDK (vendored in-repo)
export PICO_SDK_PATH="/Volumes/code/picocalc_gc/pico-sdk"

# ARM toolchain (ArmGNUToolchain cask — the path carries the version, so
# re-check it after every upgrade; see the warning above)
export PICO_TOOLCHAIN_PATH="/Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi"

# Default to Ninja for CMake generators
export CMAKE_GENERATOR="Ninja"
```

Reload: `source ~/.zshrc`.

### 6. Verify the toolchain

Run a smoke-test build from the in-repo `pico-examples` checkout:

```bash
cd pico-examples
cmake -G Ninja -DPICO_BOARD=pico  -B build/p1 -S .
cmake -G Ninja -DPICO_BOARD=pico2 -B build/p2 -S .
cmake --build build/p1 --target blink
cmake --build build/p2 --target blink
ls build/*/blink/*.uf2
```

If both files are produced, the host toolchain is good. (Last verified 2026-07-08 with `hello_serial` on both boards.)

## Editor / IDE setup

### VS Code (recommended)

Install:

- **C/C++** by Microsoft (or **clangd** by LLVM — pick one, they conflict)
- **CMake Tools** by Microsoft

Recommended: use **clangd** rather than the Microsoft C/C++ extension's IntelliSense. The Microsoft one struggles with cross-compilation and the Pico SDK's macro-heavy headers; clangd handles them cleanly with `--query-driver`.

If using clangd, disable the Microsoft IntelliSense:

```jsonc
// .vscode/settings.json
{
  "C_Cpp.intelliSenseEngine": "disabled",
  "clangd.path": "/opt/homebrew/opt/llvm/bin/clangd",
  "clangd.arguments": [
    "--background-index",
    "--clang-tidy",
    "--header-insertion=never",
    "--query-driver=/Applications/ArmGNUToolchain/**/bin/arm-none-eabi-*"
  ]
}
```

The `--query-driver` flag is critical — it tells clangd to ask the cross-compiler about its built-in include paths and predefined macros, so clangd's parser sees the same world the actual compiler does.

### Other editors

- **Neovim**: use built-in LSP support or `nvim-lspconfig` with clangd. Same `--query-driver` flag.
- **Emacs**: use `eglot` or `lsp-mode` with clangd.
- **CLion**: native CMake support; configure the toolchain to point at `arm-none-eabi-gcc`.
- **Sublime Text**: use the LSP package with `LSP-clangd`.

## clangd configuration

clangd reads `compile_commands.json` to know how each source file is compiled. CMake generates this when configured with `CMAKE_EXPORT_COMPILE_COMMANDS=ON` (set in our top-level `CMakeLists.txt`).

After running `cmake -B build/pico ...`, symlink the database to the project root:

```bash
ln -sf build/pico/compile_commands.json compile_commands.json
```

Or run `./scripts/setup-clangd.sh`. The symlinked file is gitignored.

When you switch which board you're "actively developing" against, re-run `setup-clangd.sh` with the other build directory. The symlink target controls which compile flags clangd uses for each translation unit.

The repo-level `.clangd/config.yaml` adds project-wide settings (e.g., header preferences, suppressed diagnostics). Don't put per-developer settings there; use VS Code workspace settings or `~/.config/clangd/config.yaml` for those.

## Troubleshooting

### `nosys.specs not found` during link

Means `newlib` or `libnosys` isn't installed — the build picked up a stripped-down `arm-none-eabi-gcc`. On this host that's the Homebrew *formula* `arm-none-eabi-gcc` at `/opt/homebrew/bin/arm-none-eabi-gcc` (confirmed broken, 2026-07-08). Fix: set `PICO_TOOLCHAIN_PATH` to the ArmGNUToolchain install (see steps 2 and 4 above), and reconfigure from a clean build directory so CMake re-detects the compiler.

### `arm-none-eabi-g++: no such file` during CMake configure

Toolchain not on PATH or `PICO_TOOLCHAIN_PATH` not set correctly. Verify:

```bash
echo $PICO_TOOLCHAIN_PATH
ls $PICO_TOOLCHAIN_PATH/bin/arm-none-eabi-gcc
```

### `CMAKE_C_COMPILER ... is not a full path to an existing compiler tool`

The toolchain was upgraded and its version-stamped path moved, but `PICO_TOOLCHAIN_PATH` — or an existing `build/*/CMakeCache.txt`, which caches the absolute compiler path — still names the old one. Exporting the new path is **not** enough on its own; the cache wins. Fix:

```bash
ls -d /Applications/ArmGNUToolchain/*/arm-none-eabi | sort -V | tail -1   # current install
export PICO_TOOLCHAIN_PATH="<that path>"
rm -rf build/pico build/pico2 && ./scripts/build-all.sh                   # cache holds the old path
```

### `scripts/lint.sh` reports hundreds of `'cstddef' file not found`

Same root cause, different symptom: clang-tidy replays the cross-compile commands but needs the toolchain's own system include paths, and a stale `PICO_TOOLCHAIN_PATH` silently drops them. Every TU then "fails" for reasons unrelated to the code. `lint.sh` discovers the newest installed toolchain when the variable is unset and now exits with an error rather than a warning when it finds none — but an *explicitly set, wrong* value still overrides it. Fix as above.

### clangd shows red squiggles on Pico SDK includes

The `--query-driver` flag isn't pointing at the right compiler. Check the path glob in your editor's clangd settings:

```
--query-driver=/Applications/ArmGNUToolchain/**/bin/arm-none-eabi-*
```

The `**` glob must literally appear in the argument; clangd interprets it. After editing, restart the language server.

### Build fails with unrelated case-sensitivity errors

macOS APFS is case-insensitive by default. The Pico SDK and some community libraries occasionally have files differing only by case (rare). Two fixes:

1. Create a case-sensitive APFS volume in Disk Utility (Format: APFS Case-Sensitive) and clone the project onto it.
2. Clean the build directory and re-configure: `rm -rf build && cmake -B build/pico1 ...`

### `screen` doesn't show USB serial output

After flashing, the PicoCalc takes a few seconds to enumerate the USB CDC device. Connect the screen *after* the device appears:

```bash
ls /dev/tty.usbmodem*
# Wait until one shows up, then:
screen /dev/tty.usbmodem* 115200
```

Exit `screen` with `Ctrl-A` then `K`.

### Build hangs at `Generating compile_commands.json`

CMake regeneration is slow on first configure (~30s). Subsequent configures with no changes are near-instant. If it hangs > 2 minutes, something is wrong — interrupt and check `build/CMakeError.log`.

## Optional: Docker dev container

For fully reproducible builds (or to develop on Linux/Windows hosts), a Dockerfile is provided in `scripts/docker/Dockerfile`. It installs the ARM toolchain, Pico SDK, CMake, Ninja, clangd, and Python in an Ubuntu base image. See `scripts/docker/README.md` for usage. (Stub — fill in if/when needed.)
