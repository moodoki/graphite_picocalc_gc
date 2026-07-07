#!/usr/bin/env bash
# Format all C/C++ source files in src/ using clang-format.
#
# Modifies files in place. Run lint.sh afterward to verify the
# changes don't introduce other issues.

set -euo pipefail

cd "$(dirname "$0")/.."

if ! command -v clang-format &>/dev/null; then
  echo "ERROR: clang-format not found in PATH." >&2
  echo "Install with: brew install llvm && brew link --force llvm" >&2
  exit 1
fi

FILES=$(find src -type f \( \
  -name '*.cpp' -o -name '*.hpp' -o \
  -name '*.cc'  -o -name '*.h'   -o \
  -name '*.c'                          \
\) 2>/dev/null || true)

if [[ -z "$FILES" ]]; then
  echo "No C/C++ source files found in src/."
  exit 0
fi

echo "Formatting $(echo "$FILES" | wc -l | tr -d ' ') files..."
echo "$FILES" | xargs clang-format -i

echo "Done."
