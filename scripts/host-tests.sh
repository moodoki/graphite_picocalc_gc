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
    src/math/format.cpp src/math/frac.cpp src/math/catalog.cpp src/math/dist.cpp \
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
    src/graph/seq_points.cpp \
    src/apps/table_model.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/complex.cpp src/math/seq_expr.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_graph"

echo "== Compiling + linking test_lists =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr -Itests/host \
    tests/host/test_lists.cpp tests/host/host_psram_backend.cpp \
    src/math/unified_compile.cpp src/math/unified_vm.cpp src/math/unified_home.cpp \
    src/math/array.cpp src/math/scratch.cpp src/math/lists.cpp src/math/list_ops.cpp \
    src/math/named_lists.cpp src/math/stats.cpp src/math/matrix.cpp \
    src/math/array_format.cpp src/math/frac.cpp src/math/complex.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
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
    src/math/array.cpp src/math/scratch.cpp src/math/infer.cpp src/math/dist.cpp \
    "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_infer"

echo "== Compiling + linking test_matrix =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr -Itests/host \
    tests/host/test_matrix.cpp tests/host/host_psram_backend.cpp \
    src/math/unified_compile.cpp src/math/unified_vm.cpp src/math/unified_home.cpp \
    src/math/array.cpp src/math/scratch.cpp src/math/lists.cpp src/math/list_ops.cpp \
    src/math/named_lists.cpp src/math/stats.cpp src/math/matrix.cpp \
    src/math/array_format.cpp src/math/frac.cpp src/math/complex.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
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

echo "== Compiling + linking test_unified =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_unified.cpp tests/host/host_psram_backend.cpp \
    src/math/unified_compile.cpp src/math/unified_vm.cpp \
    src/math/array.cpp src/math/scratch.cpp src/math/lists.cpp \
    src/math/list_ops.cpp src/math/named_lists.cpp src/math/stats.cpp \
    src/math/matrix.cpp src/math/array_format.cpp src/math/frac.cpp \
    src/math/complex.cpp \
    src/math/engine.cpp src/math/functions.cpp \
    src/math/format.cpp src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_unified"

echo "== Compiling + linking test_complex =="
"$CXX" -std=c++17 -O1 -Wall -Wextra \
    -Isrc \
    tests/host/test_complex.cpp src/math/complex.cpp \
    -o "$OUT/test_complex"

echo "== Compiling + linking test_complex_expr =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr -Itests/host \
    tests/host/test_complex_expr.cpp tests/host/host_psram_backend.cpp \
    src/math/unified_compile.cpp src/math/unified_vm.cpp src/math/unified_home.cpp \
    src/math/array.cpp src/math/scratch.cpp src/math/lists.cpp src/math/list_ops.cpp \
    src/math/named_lists.cpp src/math/stats.cpp src/math/matrix.cpp \
    src/math/array_format.cpp src/math/frac.cpp src/math/complex.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_complex_expr"

echo "== Compiling + linking test_units =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_units.cpp \
    src/math/units.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/complex.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_units"

# Phase 6A app framework. No math, no tinyexpr, no cephes — the registry
# is deliberately free of platform and Screen dependencies.
echo "== Compiling + linking test_apps =="
"$CXX" -std=c++17 -O1 -Wall -Wextra \
    -Isrc \
    tests/host/test_apps.cpp \
    src/platform/app_registry.cpp \
    -o "$OUT/test_apps"

# The editor's multi-line buffer, split out from TextEditorWidget so it
# needs no framebuffer.
echo "== Compiling + linking test_text_buffer =="
"$CXX" -std=c++17 -O1 -Wall -Wextra \
    -Isrc \
    tests/host/test_text_buffer.cpp \
    src/ui/text_buffer.cpp \
    -o "$OUT/test_text_buffer"

# Script output pane buffer (6B.12). Pure logic — no interpreter, and
# the drop-oldest/index-overflow edges are exactly what a device check
# cannot show.
echo "== Compiling + linking test_output_log =="
"$CXX" -std=c++17 -O1 -Wall -Wextra \
    -Isrc \
    tests/host/test_output_log.cpp \
    src/ui/output_log.cpp \
    -o "$OUT/test_output_log"

# Entry-time paren auto-close (issue #35). Pure string logic — no math
# engine, no tinyexpr, no cephes.
echo "== Compiling + linking test_autoclose =="
"$CXX" -std=c++17 -O1 -Wall -Wextra \
    -Isrc \
    tests/host/test_autoclose.cpp \
    src/math/autoclose.cpp \
    -o "$OUT/test_autoclose"

echo "== Compiling + linking test_seq =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_seq.cpp \
    src/math/seq_expr.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/complex.cpp \
    src/math/catalog.cpp src/math/dist.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_seq"

echo "== Compiling + linking test_cas =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_cas.cpp \
    src/math/cas/expr.cpp src/math/cas/parser.cpp src/math/cas/serialize.cpp \
    src/math/cas/simplify.cpp src/math/cas/derivative.cpp src/math/cas/expand.cpp \
    src/math/cas/poly.cpp src/math/cas/solve.cpp src/math/cas/factor.cpp \
    src/math/cas/integrate.cpp src/math/cas/cas_eval.cpp src/math/cas/exact.cpp \
    src/math/numeric_solve.cpp \
    src/math/engine.cpp src/math/functions.cpp src/math/format.cpp \
    src/math/frac.cpp src/math/catalog.cpp src/math/dist.cpp src/math/complex.cpp \
    src/math/scratch.cpp \
    "$OUT/tinyexpr.o" "${CEPHES_OBJS[@]}" \
    -o "$OUT/test_cas"

echo "== Compiling + linking test_stats =="
"$CXX" -std=c++17 -O1 -Wall -Wextra -DTE_POW_FROM_RIGHT \
    -Isrc -Idrivers/tinyexpr \
    tests/host/test_stats.cpp tests/host/host_psram_backend.cpp \
    src/math/array.cpp src/math/scratch.cpp src/math/stats.cpp \
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

echo "== Running test_units =="
"$OUT/test_units"

echo "== Running test_apps =="
"$OUT/test_apps"

echo "== Running test_text_buffer =="
"$OUT/test_text_buffer"

echo "== Running test_output_log =="
"$OUT/test_output_log"

echo "== Running test_autoclose =="
"$OUT/test_autoclose"

echo "== Running test_seq =="
"$OUT/test_seq"

echo "== Running test_cas =="
"$OUT/test_cas"

echo "== Running test_stats =="
"$OUT/test_stats"

echo "== Running test_unified =="
"$OUT/test_unified"

echo "== Running test_complex =="
"$OUT/test_complex"

echo "== Running test_complex_expr =="
"$OUT/test_complex_expr"
