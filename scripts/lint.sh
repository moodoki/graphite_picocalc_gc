#!/usr/bin/env bash
# Lint and static analysis. Read-only — does not modify files.
#
# Runs:
#   1. clang-format --dry-run --Werror (catches unformatted files)
#   2. clang-tidy on changed files
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
  TOOLCHAIN="${PICO_TOOLCHAIN_PATH:-/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi}"
  ARM_GXX="$TOOLCHAIN/bin/arm-none-eabi-g++"
  if [[ -x "$ARM_GXX" ]]; then
    while IFS= read -r dir; do
      TIDY_ARGS+=("--extra-arg=-isystem$dir")
    done < <("$ARM_GXX" -mcpu=cortex-m0plus -mthumb -E -x c++ - -v </dev/null 2>&1 \
             | sed -n '/#include <...> search starts here:/,/End of search list./p' \
             | sed '1d;$d;s/^ //')
  else
    echo "WARNING: $ARM_GXX not found; clang-tidy may miss system headers." >&2
  fi

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

if [[ "$EXIT" -ne 0 ]]; then
  echo
  echo "Lint failed."
  exit "$EXIT"
fi

echo
echo "Lint passed."
