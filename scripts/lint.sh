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

EXIT=0

# 1. clang-format check
if ! command -v clang-format &>/dev/null; then
  echo "ERROR: clang-format not found in PATH." >&2
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
  echo "=== clang-format check ==="
  if ! echo "$FILES" | xargs clang-format --dry-run --Werror; then
    echo "ERROR: files are not formatted. Run ./scripts/format.sh." >&2
    EXIT=1
  else
    echo "All files are properly formatted."
  fi
fi

# 2. clang-tidy
if ! command -v clang-tidy &>/dev/null; then
  echo "WARNING: clang-tidy not found in PATH. Skipping static analysis." >&2
  echo "  Install with: brew install llvm && brew link --force llvm" >&2
else
  if [[ ! -f compile_commands.json ]]; then
    echo "WARNING: compile_commands.json not found." >&2
    echo "  Run: ./scripts/setup-clangd.sh build/pico" >&2
    echo "  Skipping clang-tidy." >&2
  elif [[ -z "$FILES" ]]; then
    echo "Skipping clang-tidy (no source files)."
  else
    echo
    echo "=== clang-tidy ==="
    # Run clang-tidy on src/ files. Vendored drivers/ are excluded
    # by .clangd config and HeaderFilterRegex in .clang-tidy.
    if ! echo "$FILES" | xargs clang-tidy -p . --quiet; then
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
