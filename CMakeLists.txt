cmake_minimum_required(VERSION 3.20)

# Pull in the Pico SDK (must come before project()).
include(pico_sdk_import.cmake)

project(picocalc_graphcalc
    VERSION   0.3.1
    LANGUAGES C CXX ASM
)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD        17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS      OFF)

# Always export compile_commands.json so clangd works.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

# ---- Board target detection ----
# Pass -DPICO_BOARD=pico (default) or -DPICO_BOARD=pico2 to CMake.
if(PICO_BOARD STREQUAL "pico2")
    add_compile_definitions(PICOCALC_PICO2=1)
    message(STATUS "Building for PicoCalc with Pico 2 H (RP2350)")
else()
    add_compile_definitions(PICOCALC_PICO1=1)
    message(STATUS "Building for PicoCalc with Pico 1 H (RP2040)")
endif()

# ---- Common compile flags ----
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

# ---- Vendored drivers (read-only, see drivers/README.md) ----
add_subdirectory(drivers/i2ckbd)
add_subdirectory(drivers/lcdspi)
add_subdirectory(drivers/pwm_sound)
add_subdirectory(drivers/rp2040-psram)

# FatFs core. diskio (SD SPI glue) and get_fattime live in src/platform/.
add_library(fatfs STATIC
    drivers/fatfs/ff.c
    drivers/fatfs/ffsystem.c
    drivers/fatfs/ffunicode.c
)
target_include_directories(fatfs PUBLIC drivers/fatfs)
target_link_libraries(fatfs PUBLIC pico_stdlib)

# tinyexpr (C99). TE_POW_FROM_RIGHT: TI-style right-associative '^'.
add_library(tinyexpr STATIC drivers/tinyexpr/tinyexpr.c)
target_include_directories(tinyexpr PUBLIC drivers/tinyexpr)
target_compile_definitions(tinyexpr PUBLIC TE_POW_FROM_RIGHT)

# Cephes special functions (Phase 3C distributions — drivers/cephes/README.md).
# Symbols that exist in libm are renamed at compile time so the two can
# never collide; math::dist declares the cephes_* names extern "C".
add_library(cephes STATIC
    drivers/cephes/const.c
    drivers/cephes/polevl.c
    drivers/cephes/mtherr.c
    drivers/cephes/gamma.c
    drivers/cephes/ndtr.c
    drivers/cephes/expx2.c
    drivers/cephes/ndtri.c
    drivers/cephes/igam.c
    drivers/cephes/igami.c
    drivers/cephes/incbet.c
    drivers/cephes/incbi.c
    src/math/cephes_support.c
)
target_compile_definitions(cephes PRIVATE gamma=cephes_gamma erf=cephes_erf erfc=cephes_erfc)
target_compile_options(cephes PRIVATE -w)

# ---- Main executable ----
add_executable(picocalc_graphcalc
    src/main.cpp
    src/platform/display.cpp
    src/platform/fault.cpp
    src/platform/keyboard.cpp
    src/platform/platform.cpp
    src/platform/psram.cpp
    src/platform/sd_card.cpp
    src/platform/sd_diskio.cpp
    src/platform/storage.cpp
    src/platform/system.cpp
    src/platform/power.cpp
    src/gfx/font.cpp
    src/gfx/framebuffer.cpp
    src/ui/screen_manager.cpp
    src/ui/input_line.cpp
    src/ui/chrome.cpp
    src/math/engine.cpp
    src/math/functions.cpp
    src/math/format.cpp
    src/math/frac.cpp
    src/math/catalog.cpp
    src/math/scratch.cpp
    src/math/array.cpp
    src/math/array_backend_pico.cpp
    src/math/lists.cpp
    src/math/lists_persist.cpp
    src/math/named_lists.cpp
    src/math/named_lists_persist.cpp
    src/math/list_ops.cpp
    src/math/list_expr.cpp
    src/math/stats.cpp
    src/math/dist.cpp
    src/math/infer.cpp
    src/math/matrix.cpp
    src/math/matrices_persist.cpp
    src/math/mat_expr.cpp
    src/math/numeric_solve.cpp
    src/math/solve_expr.cpp
    src/math/complex.cpp
    src/math/complex_expr.cpp
    src/math/unified_compile.cpp  # Phase 5.2, not yet reachable from any screen
    src/math/unified_vm.cpp
    src/math/seq_expr.cpp
    src/math/units.cpp
    src/math/cas/expr.cpp
    src/math/cas/parser.cpp
    src/math/cas/serialize.cpp
    src/math/cas/simplify.cpp
    src/math/cas/derivative.cpp
    src/math/cas/expand.cpp
    src/math/cas/poly.cpp
    src/math/cas/solve.cpp
    src/math/cas/factor.cpp
    src/math/cas/integrate.cpp
    src/math/cas/cas_eval.cpp
    src/math/cas/exact.cpp
    src/render/pool.cpp
    src/render/layout_builder.cpp
    src/render/layout_render.cpp
    src/graph/viewport.cpp
    src/graph/plotter.cpp
    src/graph/graph_mode.cpp
    src/graph/graph_state.cpp
    src/graph/graph_persist.cpp
    src/graph/function_source.cpp
    src/graph/parametric_source.cpp
    src/graph/polar_source.cpp
    src/graph/seq_points.cpp
    src/graph/stat_plot.cpp
    src/graph/trace.cpp
    src/graph/analysis.cpp
    src/graph/analysis_cursor.cpp
    src/apps/nav.cpp
    src/apps/home_screen.cpp
    src/apps/const_screen.cpp
    src/apps/settings_screen.cpp
    src/apps/files_screen.cpp
    src/apps/list_editor.cpp
    src/apps/calc_menu.cpp
    src/apps/cas_menu.cpp
    src/apps/matrix_editor.cpp
    src/apps/solver_screen.cpp
    src/apps/stats_screen.cpp
    src/apps/dist_screen.cpp
    src/apps/infer_screen.cpp
    src/apps/plot_screen.cpp
    src/apps/graph_model.cpp
    src/apps/slot_editor.cpp
    src/apps/y_editor.cpp
    src/apps/param_editor.cpp
    src/apps/polar_editor.cpp
    src/apps/seq_editor.cpp
    src/apps/graph_screen.cpp
    src/apps/window_screen.cpp
    src/apps/mode_screen.cpp
    src/apps/help_screen.cpp
    src/apps/table_model.cpp
    src/apps/table_setup.cpp
    src/apps/table_screen.cpp
    src/apps/split_screen.cpp
)

target_include_directories(picocalc_graphcalc PRIVATE
    src/
    drivers/
)

# Build identifier for the diag screen: git short hash, suffixed "-dev"
# when the working tree is dirty. CI builds a clean checkout so they show
# a traceable hash; local dev builds (usually dirty) show "<hash>-dev".
# Captured at configure time — reconfigure (or a clean CI build) to
# refresh the hash after committing.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git status --porcelain
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DIRTY
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_HASH)
    set(GIT_HASH "nogit")
endif()
if(GIT_DIRTY)
    set(PICOCALC_BUILD_ID "${GIT_HASH}-dev")
else()
    set(PICOCALC_BUILD_ID "${GIT_HASH}")
endif()
message(STATUS "Build id: ${PICOCALC_BUILD_ID}")

# Current code-complete phase, shown next to the build id on the diag
# screen. Not derivable from git — bump it here when a phase/sub-phase
# reaches code-complete (the session-wrapup agent's README status pass
# covers this).
set(PICOCALC_PHASE "5.1")

# Serial line injection for on-device test automation (Phase 5.1). Lets a
# host script submit whole lines to the home screen over USB serial and read
# back the result plus its kind. Default ON: this is a personal-use dev tool
# and the automation is worth far more than the surface it adds. Not a
# security control — it is a calculator — but compiling it out keeps a
# shipping build free of a path that lets the USB port drive the UI.
option(PICOCALC_SERIAL_INJECT "Accept expression lines over USB serial (dev tooling)" ON)

# rp2040-psram is an INTERFACE library: its sources compile as part of
# this target, so its pin configuration is set here (PicoCalc mainboard).
target_compile_definitions(picocalc_graphcalc PRIVATE
    PICOCALC_BUILD_ID="${PICOCALC_BUILD_ID}"
    PICOCALC_PHASE="${PICOCALC_PHASE}"
    $<$<BOOL:${PICOCALC_SERIAL_INJECT}>:PICOCALC_SERIAL_INJECT=1>
    # Stack overrun protection (D47). Core 0's stack lives in the 4 KB
    # SCRATCH_Y bank, immediately above core 1's stack in SCRATCH_X. The
    # SDK default declares only 2 KB of it, so __StackBottom sat in the
    # middle of the bank and 2 KB of overrun went unnoticed before core 1
    # even started getting corrupted. 4096 (the maximum the bank allows)
    # puts __StackBottom exactly on the boundary, and the MPU guard traps
    # there. Caveat: on RP2040 that guard is a single 32-byte subregion,
    # so it reliably catches gradual/recursive growth but one oversized
    # frame can step over it — scripts/size-report.sh's frame listing is
    # the deterministic half of this check.
    PICO_STACK_SIZE=4096
    PICO_USE_STACK_GUARDS=1
    PSRAM_MUTEX=1
    PSRAM_ASYNC=0
    PSRAM_PIN_CS=20
    PSRAM_PIN_SCK=21
    PSRAM_PIN_MOSI=2
    PSRAM_PIN_MISO=3
)

# 8x16 main font (testdrive 2026-07-21): -DPICOCALC_FONT=<name> swaps the
# main font; the 5x8 small font stays Spleen either way. Every variant
# ships the same 32..134 slot map, so the math/complex glyphs work in all
# of them. Rasters are regenerated by scripts/gen-<name>.sh. Default is
# terminus (chosen on-device over spleen/juliamono/iosevka/unifont).
set(PICOCALC_FONT "terminus" CACHE STRING "8x16 main font: spleen | juliamono | iosevka | unifont | terminus")
set_property(CACHE PICOCALC_FONT PROPERTY STRINGS spleen juliamono iosevka unifont terminus)
if(PICOCALC_FONT STREQUAL "juliamono")
    target_compile_definitions(picocalc_graphcalc PRIVATE PICOCALC_FONT_JULIAMONO=1)
elseif(PICOCALC_FONT STREQUAL "iosevka")
    target_compile_definitions(picocalc_graphcalc PRIVATE PICOCALC_FONT_IOSEVKA=1)
elseif(PICOCALC_FONT STREQUAL "unifont")
    target_compile_definitions(picocalc_graphcalc PRIVATE PICOCALC_FONT_UNIFONT=1)
elseif(PICOCALC_FONT STREQUAL "terminus")
    target_compile_definitions(picocalc_graphcalc PRIVATE PICOCALC_FONT_TERMINUS=1)
elseif(NOT PICOCALC_FONT STREQUAL "spleen")
    message(FATAL_ERROR "PICOCALC_FONT must be spleen, juliamono, iosevka, unifont, or terminus (got '${PICOCALC_FONT}')")
endif()

target_link_libraries(picocalc_graphcalc
    pico_stdlib
    pico_multicore
    pico_rand
    hardware_watchdog
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pio
    hardware_pwm
    hardware_timer
    hardware_adc
    i2ckbd
    lcdspi
    pwm_sound
    rp2040-psram
    fatfs
    tinyexpr
    cephes
)

# Generate .uf2, .elf, .bin, .hex, .map alongside the binary.
pico_add_extra_outputs(picocalc_graphcalc)

# Enable USB stdio for printf debugging during development.
pico_enable_stdio_usb(picocalc_graphcalc  1)
pico_enable_stdio_uart(picocalc_graphcalc 0)

# ---- Hardware bring-up diagnostic (vendored blocking display path only) ----
# Minimal firmware that bypasses our gfx pipeline entirely, to isolate
# display bugs. Build with: cmake --build <dir> --target picocalc_diag
add_executable(picocalc_diag src/diag_main.cpp)
target_include_directories(picocalc_diag PRIVATE src/ drivers/)
target_link_libraries(picocalc_diag
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_i2c
    i2ckbd
    lcdspi
)
pico_add_extra_outputs(picocalc_diag)
pico_enable_stdio_usb(picocalc_diag  1)
pico_enable_stdio_uart(picocalc_diag 0)
