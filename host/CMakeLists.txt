cmake_minimum_required(VERSION 3.20)

# The desktop target is its own project(), not a third target inside the
# root one (D92). The root CMakeLists is bound to the Pico SDK from its
# first lines -- pico_sdk_import, pico_sdk_init, the SDK toolchain file,
# pico_add_extra_outputs, the MicroPython cross-build -- and a target that
# shares no toolchain, no linker script and no output format would mean
# guarding nearly every line of it.
#
# What the fork got wrong, and what D92 fixes, is the SOURCE LIST: theirs
# is a copy, and it is 29 files stale after one of our phases. This file
# lists no sources of its own -- it includes cmake/graphite-sources.cmake,
# the same lists the firmware builds from, so a new file lands in one place
# and both targets see it (6.4.1). As of 6.4.3 it names no src/ path at
# all.
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

# ---- Vendored C dependencies ----
# Sources come from the shared list; the options are repeated because they
# are options, not a list. The cephes renames are load-bearing rather than
# style: it exports gamma/erf/erfc, which collide with libm.
include(${GRAPHITE_ROOT}/cmake/graphite-sources.cmake)

add_library(tinyexpr STATIC ${GRAPHITE_TINYEXPR_SOURCES})
target_include_directories(tinyexpr PUBLIC ${GRAPHITE_ROOT}/drivers/tinyexpr)
target_compile_definitions(tinyexpr PUBLIC TE_POW_FROM_RIGHT)

add_library(cephes STATIC ${GRAPHITE_CEPHES_SOURCES})
target_compile_definitions(cephes PRIVATE gamma=cephes_gamma erf=cephes_erf erfc=cephes_erfc)
target_compile_options(cephes PRIVATE -w)
target_include_directories(cephes PRIVATE ${GRAPHITE_ROOT}/src)

# ---- MicroPython (6.4.3) ----
# D96 is the whole of it: MICROPY_GCREGS_SETJMP swaps the aarch64 register
# scan, which uses a GCC-only asm-register declaration clang rejects, for
# MicroPython's own portable setjmp path. 135 of 136 files compile without
# it, 136 with. Everything else about the embed build is shared with the
# firmware, unchanged.
set(GRAPHITE_MICROPYTHON_GCREGS_SETJMP ON)
include(${GRAPHITE_ROOT}/cmake/graphite-micropython.cmake)

# ---- The shared tree ----

add_executable(graphite-shot
    ${GRAPHITE_PORTABLE_SOURCES}
    main_host.cpp
    platform_host.cpp
    display_headless.cpp
    keyboard_host.cpp
    storage_posix.cpp
    psram_arena.cpp
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

target_link_libraries(graphite-shot PRIVATE tinyexpr cephes micropython)
