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

echo "== Compiling cephes (C) =="
# Same renames as the CMake cephes target (see drivers/cephes/README.md).
# Every test that links catalog.cpp needs these + dist.cpp (3C).
CEPHES_DEFS="-Dgamma=cephes_gamma -Derf=cephes_erf -Derfc=cephes_erfc"
CEPHES_OBJS=()
for f in const polevl mtherr gamma ndtr expx2 ndtri igam igami incbet incbi; do
    "$CC" -std=c11 -O1 -w $CEPHES_DEFS -c "drivers/cephes/$f.c" -o "$OUT/cephes_$f.o"
    CEPHES_OBJS+=("$OUT/cephes_$f.o")
done
# isfinite-function shim (neither newlib nor macOS libm exports the
# symbol; see the file).
"$CC" -std=c11 -O1 -c src/math/cephes_support.c -o "$OUT/cephes_shim.o"
CEPHES_OBJS+=("$OUT/cephes_shim.o")

echo "== Compiling + linking test_math =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_math.cpp src/math/engine.cpp src/math/functions.cpp \
    src/math/format.cpp src/math/catalog.cpp src/math/dist.cpp \
    src/math/complex.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
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
    src/graph/polar_source.cpp src/graph/trace.cpp \
    src/apps/table_model.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/complex.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_graph"

echo "== Compiling + linking test_lists =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_lists.cpp tests/host/host_psram_backend.cpp \
    src/math/array.cpp src/math/lists.cpp src/math/list_ops.cpp \
    src/math/list_expr.cpp src/math/stats.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/complex.cpp src/math/complex_expr.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_lists"

echo "== Compiling + linking test_dist =="
"$CXX" -std=c++17 -O1 -Wall -Wextra \
    -Isrc \
    tests/host/test_dist.cpp src/math/dist.cpp \
    "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_dist"

echo "== Compiling + linking test_infer =="
"$CXX" -std=c++17 -O1 -Wall -Wextra \
    -Isrc \
    tests/host/test_infer.cpp tests/host/host_psram_backend.cpp \
    src/math/array.cpp src/math/infer.cpp src/math/dist.cpp \
    "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_infer"

echo "== Compiling + linking test_matrix =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_matrix.cpp tests/host/host_psram_backend.cpp \
    src/math/array.cpp src/math/matrix.cpp src/math/mat_expr.cpp \
    src/math/lists.cpp src/math/list_ops.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/complex.cpp src/math/complex_expr.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_matrix"

echo "== Compiling + linking test_solve =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_solve.cpp \
    src/math/numeric_solve.cpp src/math/solve_expr.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/complex.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_solve"

echo "== Compiling + linking test_analysis =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_analysis.cpp \
    src/graph/analysis.cpp src/graph/analysis_cursor.cpp \
    src/graph/graph_mode.cpp \
    src/math/numeric_solve.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/complex.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_analysis"

echo "== Compiling + linking test_complex =="
"$CXX" -std=c++17 -O1 -Wall -Wextra \
    -Isrc \
    tests/host/test_complex.cpp src/math/complex.cpp \
    -o "$OUT/test_complex"

echo "== Compiling + linking test_complex_expr =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_complex_expr.cpp \
    src/math/complex.cpp src/math/complex_expr.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_complex_expr"

echo "== Compiling + linking test_stats =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_stats.cpp tests/host/host_psram_backend.cpp \
    src/math/array.cpp src/math/stats.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/complex.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_stats"

echo "== Running test_math =="
"$OUT/test_math"

echo "== Running test_layout =="
"$OUT/test_layout"

echo "== Running test_graph =="
"$OUT/test_graph"

echo "== Running test_lists =="
"$OUT/test_lists"

echo "== Running test_dist =="
"$OUT/test_dist"

echo "== Running test_infer =="
"$OUT/test_infer"

echo "== Running test_matrix =="
"$OUT/test_matrix"

echo "== Running test_solve =="
"$OUT/test_solve"

echo "== Running test_analysis =="
"$OUT/test_analysis"

echo "== Running test_stats =="
"$OUT/test_stats"

echo "== Running test_complex =="
"$OUT/test_complex"

echo "== Running test_complex_expr =="
"$OUT/test_complex_expr"
