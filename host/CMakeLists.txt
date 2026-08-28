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
# and both targets see it (6.4.1). The only src/ paths it names are the
# four MicroPython glue files it removes until 6.4.3, and that removal is
# checked rather than assumed.
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

# ---- The shared tree ----
# The MicroPython glue is host-clean at the include level -- that was
# measured, 136 of 136 embed files compile natively with
# MICROPY_GCREGS_SETJMP=1 (D96). What it still needs is the library to
# link against, and generating the embed tree is 6.4.3. Until then these
# four come out, and host/scripting_stub.cpp answers for them.
#
# DELETE THIS BLOCK in 6.4.3. It is the one place where host/ knows a
# src/ path by name, which is exactly what D92 is trying to avoid, so it
# should not outlive the reason for it.
set(GRAPHITE_HOST_SOURCES ${GRAPHITE_PORTABLE_SOURCES})
list(REMOVE_ITEM GRAPHITE_HOST_SOURCES
    ${GRAPHITE_ROOT}/src/scripting/mp_port.c
    ${GRAPHITE_ROOT}/src/scripting/micropython_embed.cpp
    ${GRAPHITE_ROOT}/src/scripting/calc_api.cpp
    ${GRAPHITE_ROOT}/src/scripting/mp_calc_module.c
)
list(LENGTH GRAPHITE_PORTABLE_SOURCES _n_portable)
list(LENGTH GRAPHITE_HOST_SOURCES _n_host)
math(EXPR _removed "${_n_portable} - ${_n_host}")
if(NOT _removed EQUAL 4)
    # A silent no-op here would mean a renamed file quietly comes back into
    # the build and fails to link with a wall of MicroPython symbols.
    message(FATAL_ERROR
        "expected to drop 4 MicroPython sources, dropped ${_removed} -- "
        "has a path in cmake/graphite-sources.cmake been renamed?")
endif()

add_executable(graphite-shot
    ${GRAPHITE_HOST_SOURCES}
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
