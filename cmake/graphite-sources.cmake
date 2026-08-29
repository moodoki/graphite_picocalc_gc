# The source list, shared by every target that compiles this tree (D92).
#
# There is one copy of this because there was very nearly not. The
# downstream Luckfox fork made host/ its own project with its own copy of
# the list, and one phase later that copy was 29 files stale -- it predates
# every app, scripting and text-editor file Phase 6 added. A host target
# that no longer builds is worse than none, because it costs CI time and
# lies about its own coverage.
#
# So: a new file goes in ONE of the two lists below, and both the firmware
# and the desktop build see it. The split is by target, not by directory --
# src/platform/ contains both kinds, because app_registry, sd_apps,
# sd_app_scan and io_scratch are pure logic over the platform:: interfaces
# rather than implementations of them.
#
# Paths are absolute, anchored here rather than at the includer, because
# host/ is a separate project() one directory down (D92) and a relative
# path would resolve differently in each.

set(GRAPHITE_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# Talks to the board: the platform:: backends and the firmware entry point.
# host/ supplies its own implementations of these interfaces, so this list
# is exactly what the two targets do NOT share.
set(GRAPHITE_PICO_SOURCES
    ${GRAPHITE_ROOT}/src/main.cpp
    ${GRAPHITE_ROOT}/src/platform/display.cpp
    ${GRAPHITE_ROOT}/src/platform/fault.cpp
    ${GRAPHITE_ROOT}/src/platform/keyboard.cpp
    ${GRAPHITE_ROOT}/src/platform/platform.cpp
    ${GRAPHITE_ROOT}/src/platform/psram.cpp
    ${GRAPHITE_ROOT}/src/platform/sd_card.cpp
    ${GRAPHITE_ROOT}/src/platform/sd_diskio.cpp
    ${GRAPHITE_ROOT}/src/platform/storage.cpp
    ${GRAPHITE_ROOT}/src/platform/system.cpp
    ${GRAPHITE_ROOT}/src/platform/power.cpp
    ${GRAPHITE_ROOT}/src/math/array_backend_pico.cpp)

# Compiles anywhere. 98 of these were verified host-clean by compilation
# before Phase 6.4 was written, and the whole of Phase 6's 29 new files
# turned out to be in here without anyone having checked as they landed --
# which is the measurement that decided the phase was affordable.
#
# If a file here stops compiling on a desktop, the fix is almost never a
# guard in this file's consumers: it is that Pico coupling has leaked past
# the platform:: seam. D94 keeps that visible by counting guards.
set(GRAPHITE_PORTABLE_SOURCES
    ${GRAPHITE_ROOT}/src/platform/app_registry.cpp
    ${GRAPHITE_ROOT}/src/platform/sd_apps.cpp
    ${GRAPHITE_ROOT}/src/platform/sd_app_scan.cpp
    ${GRAPHITE_ROOT}/src/platform/io_scratch.cpp
    ${GRAPHITE_ROOT}/src/platform/key_names.cpp
    ${GRAPHITE_ROOT}/src/gfx/font.cpp
    ${GRAPHITE_ROOT}/src/gfx/framebuffer.cpp
    ${GRAPHITE_ROOT}/src/ui/screen_manager.cpp
    ${GRAPHITE_ROOT}/src/ui/input_line.cpp
    ${GRAPHITE_ROOT}/src/ui/prompt_line.cpp
    ${GRAPHITE_ROOT}/src/ui/output_log.cpp
    ${GRAPHITE_ROOT}/src/ui/text_buffer.cpp
    ${GRAPHITE_ROOT}/src/ui/text_editor_widget.cpp
    ${GRAPHITE_ROOT}/src/ui/chrome.cpp
    ${GRAPHITE_ROOT}/src/math/engine.cpp
    ${GRAPHITE_ROOT}/src/math/functions.cpp
    ${GRAPHITE_ROOT}/src/math/format.cpp
    ${GRAPHITE_ROOT}/src/math/frac.cpp
    ${GRAPHITE_ROOT}/src/math/autoclose.cpp
    ${GRAPHITE_ROOT}/src/math/catalog.cpp
    ${GRAPHITE_ROOT}/src/math/scratch.cpp
    ${GRAPHITE_ROOT}/src/math/array.cpp
    ${GRAPHITE_ROOT}/src/math/lists.cpp
    ${GRAPHITE_ROOT}/src/math/lists_persist.cpp
    ${GRAPHITE_ROOT}/src/math/named_lists.cpp
    ${GRAPHITE_ROOT}/src/math/named_lists_persist.cpp
    ${GRAPHITE_ROOT}/src/math/list_ops.cpp
    ${GRAPHITE_ROOT}/src/math/stats.cpp
    ${GRAPHITE_ROOT}/src/math/dist.cpp
    ${GRAPHITE_ROOT}/src/math/infer.cpp
    ${GRAPHITE_ROOT}/src/math/matrix.cpp
    ${GRAPHITE_ROOT}/src/math/matrices_persist.cpp
    ${GRAPHITE_ROOT}/src/math/numeric_solve.cpp
    ${GRAPHITE_ROOT}/src/math/solve_expr.cpp
    ${GRAPHITE_ROOT}/src/math/complex.cpp
    ${GRAPHITE_ROOT}/src/math/unified_compile.cpp
    ${GRAPHITE_ROOT}/src/math/unified_vm.cpp
    ${GRAPHITE_ROOT}/src/math/unified_home.cpp
    ${GRAPHITE_ROOT}/src/math/array_format.cpp
    ${GRAPHITE_ROOT}/src/math/seq_expr.cpp
    ${GRAPHITE_ROOT}/src/math/units.cpp
    ${GRAPHITE_ROOT}/src/math/cas/expr.cpp
    ${GRAPHITE_ROOT}/src/math/cas/parser.cpp
    ${GRAPHITE_ROOT}/src/math/cas/serialize.cpp
    ${GRAPHITE_ROOT}/src/math/cas/simplify.cpp
    ${GRAPHITE_ROOT}/src/math/cas/derivative.cpp
    ${GRAPHITE_ROOT}/src/math/cas/expand.cpp
    ${GRAPHITE_ROOT}/src/math/cas/poly.cpp
    ${GRAPHITE_ROOT}/src/math/cas/solve.cpp
    ${GRAPHITE_ROOT}/src/math/cas/factor.cpp
    ${GRAPHITE_ROOT}/src/math/cas/integrate.cpp
    ${GRAPHITE_ROOT}/src/math/cas/cas_eval.cpp
    ${GRAPHITE_ROOT}/src/math/cas/exact.cpp
    ${GRAPHITE_ROOT}/src/math/var_store.cpp
    ${GRAPHITE_ROOT}/src/scripting/mp_port.c
    ${GRAPHITE_ROOT}/src/scripting/micropython_embed.cpp
    ${GRAPHITE_ROOT}/src/scripting/calc_api.cpp
    ${GRAPHITE_ROOT}/src/scripting/calc_canvas.cpp
    ${GRAPHITE_ROOT}/src/scripting/mp_calc_module.c
    ${GRAPHITE_ROOT}/src/render/pool.cpp
    ${GRAPHITE_ROOT}/src/render/layout_builder.cpp
    ${GRAPHITE_ROOT}/src/render/layout_render.cpp
    ${GRAPHITE_ROOT}/src/graph/viewport.cpp
    ${GRAPHITE_ROOT}/src/graph/plotter.cpp
    ${GRAPHITE_ROOT}/src/graph/graph_mode.cpp
    ${GRAPHITE_ROOT}/src/graph/graph_state.cpp
    ${GRAPHITE_ROOT}/src/graph/graph_persist.cpp
    ${GRAPHITE_ROOT}/src/graph/function_source.cpp
    ${GRAPHITE_ROOT}/src/graph/parametric_source.cpp
    ${GRAPHITE_ROOT}/src/graph/polar_source.cpp
    ${GRAPHITE_ROOT}/src/graph/seq_points.cpp
    ${GRAPHITE_ROOT}/src/graph/stat_plot.cpp
    ${GRAPHITE_ROOT}/src/graph/trace.cpp
    ${GRAPHITE_ROOT}/src/graph/analysis.cpp
    ${GRAPHITE_ROOT}/src/graph/analysis_cursor.cpp
    ${GRAPHITE_ROOT}/src/apps/nav.cpp
    ${GRAPHITE_ROOT}/src/apps/launcher_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/notepad_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/program_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/home_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/const_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/settings_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/file_list.cpp
    ${GRAPHITE_ROOT}/src/apps/files_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/list_editor.cpp
    ${GRAPHITE_ROOT}/src/apps/calc_menu.cpp
    ${GRAPHITE_ROOT}/src/apps/cas_menu.cpp
    ${GRAPHITE_ROOT}/src/apps/matrix_editor.cpp
    ${GRAPHITE_ROOT}/src/apps/solver_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/stats_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/dist_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/infer_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/plot_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/graph_model.cpp
    ${GRAPHITE_ROOT}/src/apps/slot_editor.cpp
    ${GRAPHITE_ROOT}/src/apps/y_editor.cpp
    ${GRAPHITE_ROOT}/src/apps/param_editor.cpp
    ${GRAPHITE_ROOT}/src/apps/polar_editor.cpp
    ${GRAPHITE_ROOT}/src/apps/seq_editor.cpp
    ${GRAPHITE_ROOT}/src/apps/graph_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/window_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/mode_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/help_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/table_model.cpp
    ${GRAPHITE_ROOT}/src/apps/table_setup.cpp
    ${GRAPHITE_ROOT}/src/apps/table_screen.cpp
    ${GRAPHITE_ROOT}/src/apps/split_screen.cpp
)

# ---- Vendored C dependencies ----
# Here for the same reason as the lists above: both builds compile these,
# and two copies of a file list is the exact thing D92 exists to stop. The
# *target* setup stays in each project -- the renames and -w on cephes are
# repeated in both, because they are compile options rather than a list,
# and a wrong one fails loudly at link time rather than rotting quietly.
set(GRAPHITE_TINYEXPR_SOURCES
    ${GRAPHITE_ROOT}/drivers/tinyexpr/tinyexpr.c
)

# cephes_support.c is ours, not vendored: neither newlib nor macOS libm
# exports isfinite as a function, and cephes calls it as one.
set(GRAPHITE_CEPHES_SOURCES
    ${GRAPHITE_ROOT}/drivers/cephes/const.c
    ${GRAPHITE_ROOT}/drivers/cephes/polevl.c
    ${GRAPHITE_ROOT}/drivers/cephes/mtherr.c
    ${GRAPHITE_ROOT}/drivers/cephes/gamma.c
    ${GRAPHITE_ROOT}/drivers/cephes/ndtr.c
    ${GRAPHITE_ROOT}/drivers/cephes/expx2.c
    ${GRAPHITE_ROOT}/drivers/cephes/ndtri.c
    ${GRAPHITE_ROOT}/drivers/cephes/igam.c
    ${GRAPHITE_ROOT}/drivers/cephes/igami.c
    ${GRAPHITE_ROOT}/drivers/cephes/incbet.c
    ${GRAPHITE_ROOT}/drivers/cephes/incbi.c
    ${GRAPHITE_ROOT}/src/math/cephes_support.c
)
