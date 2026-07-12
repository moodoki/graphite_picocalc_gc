#!/usr/bin/env bash
# Build and run host-side unit tests (math engine — no Pico hardware or
# cross-toolchain needed). Uses the system C/C++ compiler.
#
# Usage: ./scripts/host-tests.sh

set -euo pipefail

cd "$(dirname "$0")/.."

OUT=build/host-tests
mkdir -p "$OUT"

CC=${CC:-cc}
CXX=${CXX:-c++}

echo "== Compiling tinyexpr (C) =="
"$CC" -std=c11 -O1 -DTE_POW_FROM_RIGHT -c drivers/tinyexpr/tinyexpr.c \
    -o "$OUT/tinyexpr.o"

echo "== Compiling + linking test_math =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_math.cpp src/math/engine.cpp src/math/functions.cpp \
    src/math/format.cpp "$OUT/tinyexpr.o" \
    -o "$OUT/test_math"

echo "== Compiling + linking test_layout =="
"$CXX" -std=c++17 -O1 -Wall -Wextra \
    -Isrc \
    tests/host/test_layout.cpp src/render/layout_builder.cpp \
    src/render/pool.cpp \
    -o "$OUT/test_layout"

echo "== Compiling + linking test_graph =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_graph.cpp src/graph/viewport.cpp \
    src/graph/graph_mode.cpp src/graph/graph_state.cpp \
    src/graph/function_source.cpp src/graph/parametric_source.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    "$OUT/tinyexpr.o" \
    -o "$OUT/test_graph"

echo "== Running test_math =="
"$OUT/test_math"

echo "== Running test_layout =="
"$OUT/test_layout"

echo "== Running test_graph =="
"$OUT/test_graph"
