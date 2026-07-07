#!/usr/bin/env bash
# Build both Pico 1 and Pico 2 targets.
#
# Usage: ./scripts/build-all.sh [--clean]

set -euo pipefail

cd "$(dirname "$0")/.."

CLEAN=0
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    -h|--help)
      cat <<EOF
Usage: $0 [--clean]

Build both Pico 1 H (RP2040) and Pico 2 H (RP2350) targets.

Options:
  --clean    Remove build/ directory before configuring.
  -h, --help Show this help.
EOF
      exit 0
      ;;
    *) echo "Unknown argument: $arg" >&2; exit 1 ;;
  esac
done

if [[ -z "${PICO_SDK_PATH:-}" ]]; then
  echo "ERROR: PICO_SDK_PATH not set. See docs/dev-environment.md." >&2
  exit 1
fi

if [[ -z "${PICO_TOOLCHAIN_PATH:-}" ]]; then
  echo "WARNING: PICO_TOOLCHAIN_PATH not set. Build may fail or use system toolchain." >&2
fi

if [[ "$CLEAN" -eq 1 ]]; then
  echo "Cleaning build/ ..."
  rm -rf build/
fi

build_target() {
  local board="$1"
  local dir="build/${board}"
  echo
  echo "=== Configuring $board → $dir ==="
  cmake -G Ninja -DPICO_BOARD="$board" -B "$dir" -S .
  echo
  echo "=== Building $board ==="
  cmake --build "$dir"
  echo
  echo "=== $board build complete ==="
  ls -la "$dir/picocalc_graphcalc.uf2" 2>/dev/null || \
    echo "WARNING: expected .uf2 not found in $dir/"
}

build_target pico
build_target pico2

echo
echo "All builds succeeded."
echo "  Pico 1: build/pico/picocalc_graphcalc.uf2"
echo "  Pico 2: build/pico2/picocalc_graphcalc.uf2"
