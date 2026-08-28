cmake_minimum_required(VERSION 3.20)

# The desktop target is its own project(), not a third target inside the
# root one (D92). The root CMakeLists is bound to the Pico SDK from its
# first lines -- pico_sdk_import, pico_sdk_init, the SDK toolchain file,
# pico_add_extra_outputs, the MicroPython cross-build -- and a target that
# shares no toolchain, no linker script and no output format would mean
# guarding nearly every line of it.
#
# What the fork got wrong, and what D92 fixes, is the SOURCE LIST: theirs
# is a copy, and it is 29 files stale after one of our phases. The list
# below is deliberately AD HOC and temporary -- 6.4.1 replaces it with
# cmake/graphite-sources.cmake, included by this file and by the root, so
# a new file lands in one place and both targets see it. Do not add to it
# here; if you are reading this after 6.4.1 landed, this comment is a bug.
#
# Build:  cmake -B build/host -S host && cmake --build build/host
project(graphite_host LANGUAGES C CXX)

set(GRAPHITE_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD        17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS      OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Mirrors the root project's flags. Not cosmetic: -fno-exceptions and
# -fno-rtti change what the shared tree compiles to, so a host build
# without them would be testing different code from the firmware.
add_compile_options(
    -Wall
    -Wextra
    -Wshadow
    -Wpedantic
    -fno-exceptions
    $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
)

# ---- Vendored C dependencies, same options as the root build ----
add_library(tinyexpr STATIC ${GRAPHITE_ROOT}/drivers/tinyexpr/tinyexpr.c)
target_include_directories(tinyexpr PUBLIC ${GRAPHITE_ROOT}/drivers/tinyexpr)
target_compile_definitions(tinyexpr PUBLIC TE_POW_FROM_RIGHT)

# The renames are load-bearing, not style: cephes exports gamma/erf/erfc,
# which collide with libm. Same list as the root cephes target.
add_library(cephes STATIC
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
target_compile_definitions(cephes PRIVATE gamma=cephes_gamma erf=cephes_erf erfc=cephes_erfc)
target_compile_options(cephes PRIVATE -w)
target_include_directories(cephes PRIVATE ${GRAPHITE_ROOT}/src)

# ---- The shared tree, minus the platform backends and MicroPython ----
# TEMPORARY. See the header comment: 6.4.1 replaces this.
set(GRAPHITE_HOST_SPIKE_SOURCES
    ${GRAPHITE_ROOT}/src/platform/app_registry.cpp
    ${GRAPHITE_ROOT}/src/platform/sd_apps.cpp
    ${GRAPHITE_ROOT}/src/platform/sd_app_scan.cpp
    ${GRAPHITE_ROOT}/src/platform/io_scratch.cpp
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
    ${GRAPHITE_ROOT}/src/scripting/calc_canvas.cpp
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

add_executable(graphite-shot
    ${GRAPHITE_HOST_SPIKE_SOURCES}
    main_host.cpp
    platform_host.cpp
    display_headless.cpp
    keyboard_host.cpp
    storage_stub.cpp
    psram_arena.cpp
    scripting_stub.cpp
)

target_include_directories(graphite-shot PRIVATE
    ${GRAPHITE_ROOT}/src
    ${GRAPHITE_ROOT}
    ${GRAPHITE_ROOT}/drivers
)

target_compile_definitions(graphite-shot PRIVATE PICOCALC_HOST=1)

# The font is compiled in, so a host build with a different one silently
# produces screenshots that do not match the device. Same cache variable,
# same default, same fatal error on a bad value as the root project.
set(PICOCALC_FONT "terminus" CACHE STRING "8x16 main font: spleen | juliamono | iosevka | unifont | terminus")
set_property(CACHE PICOCALC_FONT PROPERTY STRINGS spleen juliamono iosevka unifont terminus)
if(PICOCALC_FONT STREQUAL "juliamono")
    target_compile_definitions(graphite-shot PRIVATE PICOCALC_FONT_JULIAMONO=1)
elseif(PICOCALC_FONT STREQUAL "iosevka")
    target_compile_definitions(graphite-shot PRIVATE PICOCALC_FONT_IOSEVKA=1)
elseif(PICOCALC_FONT STREQUAL "unifont")
    target_compile_definitions(graphite-shot PRIVATE PICOCALC_FONT_UNIFONT=1)
elseif(PICOCALC_FONT STREQUAL "terminus")
    target_compile_definitions(graphite-shot PRIVATE PICOCALC_FONT_TERMINUS=1)
elseif(NOT PICOCALC_FONT STREQUAL "spleen")
    message(FATAL_ERROR "PICOCALC_FONT must be spleen, juliamono, iosevka, unifont, or terminus (got '${PICOCALC_FONT}')")
endif()

target_link_libraries(graphite-shot PRIVATE tinyexpr cephes)
