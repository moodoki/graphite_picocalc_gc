#!/usr/bin/env bash
# Lint and static analysis. Read-only — does not modify files.
#
# Runs:
#   1. clang-format --dry-run --Werror (catches unformatted files)
#   2. clang-tidy on changed files
#   3. check-text-fits.py (literal text drawn past the panel edge)
#
# Exits non-zero if any check fails.

set -euo pipefail

cd "$(dirname "$0")/.."

# Homebrew's llvm is keg-only; pick up its clang-tidy if none is on PATH.
if ! command -v clang-tidy &>/dev/null && [[ -x /opt/homebrew/opt/llvm/bin/clang-tidy ]]; then
  PATH="/opt/homebrew/opt/llvm/bin:$PATH"
fi

# Prefer the pinned clang-format from the dev .venv (requirements-dev.txt) so
# formatting matches CI byte-for-byte regardless of the system/Homebrew version.
if [[ -x .venv/bin/clang-format ]]; then
  CLANG_FORMAT=.venv/bin/clang-format
else
  CLANG_FORMAT=clang-format
fi

EXIT=0

# 1. clang-format check
if ! command -v "$CLANG_FORMAT" &>/dev/null; then
  echo "ERROR: clang-format not found (tried .venv and PATH)." >&2
  echo "  Install with: .venv/bin/pip install -r requirements-dev.txt" >&2
  exit 1
fi

FILES=$(find src -type f \( \
  -name '*.cpp' -o -name '*.hpp' -o \
  -name '*.cc'  -o -name '*.h'   -o \
  -name '*.c'                          \
\) 2>/dev/null || true)

if [[ -z "$FILES" ]]; then
  echo "No C/C++ source files found in src/. Skipping format check."
else
  echo "=== clang-format check ($("$CLANG_FORMAT" --version)) ==="
  if ! echo "$FILES" | xargs "$CLANG_FORMAT" --dry-run --Werror; then
    echo "ERROR: files are not formatted. Run ./scripts/format.sh." >&2
    EXIT=1
  else
    echo "All files are properly formatted."
  fi
fi

# 2. clang-tidy
if ! command -v clang-tidy &>/dev/null; then
  echo "WARNING: clang-tidy not found in PATH. Skipping static analysis." >&2
  echo "  Install with: brew install llvm" >&2
else
  # Only .cpp/.c go to clang-tidy: headers have no compile_commands.json
  # entry (clang-tidy would guess a bogus command) and are analyzed via
  # HeaderFilterRegex when their including TU is processed.
  SRC_FILES=$(echo "$FILES" | grep -E '\.(cpp|cc|c)$' || true)

  # clang-tidy replays arm-none-eabi-g++ commands but doesn't know that
  # toolchain's built-in include paths (newlib, libstdc++). Ask g++ for
  # its search list and pass it along.
  TIDY_ARGS=()
  # The ArmGNUToolchain install path is version-stamped, so it moves on every
  # upgrade. Discover the newest installed one rather than hardcoding a version:
  # a stale path here does not fail loudly, it silently drops the toolchain's
  # system include paths and buries the real output under hundreds of bogus
  # "'cstddef' file not found" / const-correctness errors.
  TOOLCHAIN="${PICO_TOOLCHAIN_PATH:-$(
    ls -d /Applications/ArmGNUToolchain/*/arm-none-eabi 2>/dev/null | sort -V | tail -1
  )}"
  ARM_GXX="$TOOLCHAIN/bin/arm-none-eabi-g++"
  if [[ ! -x "$ARM_GXX" ]]; then
    # Fatal rather than a warning, for the reason above — the run would
    # "fail" with errors that have nothing to do with the code.
    echo "ERROR: arm-none-eabi-g++ not found${TOOLCHAIN:+ at $ARM_GXX}." >&2
    echo "  clang-tidy needs the cross toolchain's system headers; without them" >&2
    echo "  every TU reports missing standard headers. Set PICO_TOOLCHAIN_PATH" >&2
    echo "  to the install (see docs/dev-environment.md) and re-run." >&2
    exit 1
  fi
  while IFS= read -r dir; do
    TIDY_ARGS+=("--extra-arg=-isystem$dir")
  done < <("$ARM_GXX" -mcpu=cortex-m0plus -mthumb -E -x c++ - -v </dev/null 2>&1 \
           | sed -n '/#include <...> search starts here:/,/End of search list./p' \
           | sed '1d;$d;s/^ //')

  if [[ ! -f compile_commands.json ]]; then
    echo "WARNING: compile_commands.json not found." >&2
    echo "  Run: ./scripts/setup-clangd.sh build/pico" >&2
    echo "  Skipping clang-tidy." >&2
  elif [[ -z "$SRC_FILES" ]]; then
    echo "Skipping clang-tidy (no source files)."
  else
    echo
    echo "=== clang-tidy ==="
    # Vendored drivers/ are excluded by HeaderFilterRegex in .clang-tidy.
    if ! echo "$SRC_FILES" | xargs clang-tidy -p . --quiet "${TIDY_ARGS[@]}"; then
      EXIT=1
    fi
  fi
fi

# 3. Panel-width check for literal text. Needs no toolchain and no board —
# it reads the strings and the panel width straight out of the source,
# which is the only place a hint row's length is ever decided. It does NOT
# see softkey labels: draw_softkeys truncates by design rather than
# overflowing, so nothing lands out of bounds (issue #52).
echo
echo "=== text-fits check ==="
if ! python3 scripts/check-text-fits.py; then
  echo "ERROR: text is drawn past the edge of the panel." >&2
  EXIT=1
fi

if [[ "$EXIT" -ne 0 ]]; then
  echo
  echo "Lint failed."
  exit "$EXIT"
fi

echo
echo "Lint passed."
