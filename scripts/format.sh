#!/usr/bin/env bash
# Format all C/C++ source files in src/ using clang-format.
#
# Modifies files in place. Run lint.sh afterward to verify the
# changes don't introduce other issues.

set -euo pipefail

cd "$(dirname "$0")/.."

# Prefer the pinned clang-format from the dev .venv (requirements-dev.txt) so
# formatting matches CI byte-for-byte regardless of the system/Homebrew version.
if [[ -x .venv/bin/clang-format ]]; then
  CLANG_FORMAT=.venv/bin/clang-format
else
  CLANG_FORMAT=clang-format
fi

if ! command -v "$CLANG_FORMAT" &>/dev/null; then
  echo "ERROR: clang-format not found (tried .venv and PATH)." >&2
  echo "Install with: .venv/bin/pip install -r requirements-dev.txt" >&2
  exit 1
fi

# host/ included as of Phase 6.4 -- keep in step with scripts/lint.sh.
FILES=$(find src host -type f \( \
  -name '*.cpp' -o -name '*.hpp' -o \
  -name '*.cc'  -o -name '*.h'   -o \
  -name '*.c'                          \
\) 2>/dev/null || true)

if [[ -z "$FILES" ]]; then
  echo "No C/C++ source files found in src/."
  exit 0
fi

echo "Formatting $(echo "$FILES" | wc -l | tr -d ' ') files..."
echo "$FILES" | xargs "$CLANG_FORMAT" -i

echo "Done."
