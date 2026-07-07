#!/usr/bin/env bash
# Symlink compile_commands.json from a build directory to the project
# root, so clangd finds it automatically.
#
# Usage:
#   ./scripts/setup-clangd.sh                    # defaults to build/pico
#   ./scripts/setup-clangd.sh build/pico2        # use Pico 2 build

set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${1:-build/pico}"

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "ERROR: $BUILD_DIR does not exist." >&2
  echo "Run ./scripts/build-all.sh first to generate build directories." >&2
  exit 1
fi

if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
  echo "ERROR: $BUILD_DIR/compile_commands.json not found." >&2
  echo "Make sure CMakeLists.txt has CMAKE_EXPORT_COMPILE_COMMANDS ON." >&2
  exit 1
fi

ln -sf "$BUILD_DIR/compile_commands.json" compile_commands.json
echo "Linked compile_commands.json → $BUILD_DIR/compile_commands.json"
echo "Restart your editor's clangd server to pick up the change."
