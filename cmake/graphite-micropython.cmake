# The MicroPython embed build, shared by the firmware and the desktop
# target (D92, extended in 6.4.3).
#
# Here for the same reason the source lists are: both projects need all of
# it, and the generation step in particular is subtle enough that a second
# copy would be a liability rather than a duplication.
#
# Requires GRAPHITE_ROOT to be set -- include cmake/graphite-sources.cmake
# first, or set it yourself.
#
# ---- MicroPython (Phase 6B) ----
# MicroPython is a git submodule (drivers/micropython), not a vendored copy:
# it is far too large and too actively maintained to hand-port, and it is the
# first dependency of that kind here — see drivers/README.md.
#
# It has no build system we can call directly. Upstream's embed port instead
# GENERATES a self-contained tree of .c/.h, which we then compile ourselves.
# That has to happen at CONFIGURE time, because CMake needs the source list
# before it can define a target. The generator needs `make` and a HOST
# compiler (it preprocesses to collect qstrs); the arm toolchain is not
# involved until the add_library below.
set(MPY_PORT_DIR    ${GRAPHITE_ROOT}/drivers/micropython_port)
set(MPY_EMBED_DIR   ${CMAKE_BINARY_DIR}/micropython_embed)

if(NOT EXISTS ${GRAPHITE_ROOT}/drivers/micropython/py/mkenv.mk)
    message(FATAL_ERROR
        "drivers/micropython is empty — run: git submodule update --init --recursive")
endif()

message(STATUS "Generating MicroPython embed package -> ${MPY_EMBED_DIR}")
# From scratch, every time. The generator's incremental path leaves stale
# per-module fragments in genhdr/module/, so turning a feature OFF in
# mpconfigport.h still emitted its MP_REGISTER_MODULE entry and the link
# failed on a symbol nothing compiled any more. Configure is rare; a
# silently wrong qstr/module table is not worth the seconds saved.
file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/micropython_embed_build ${MPY_EMBED_DIR})
execute_process(
    COMMAND make -f micropython_embed.mk
            BUILD=${CMAKE_BINARY_DIR}/micropython_embed_build
            PACKAGE_DIR=${MPY_EMBED_DIR}
    WORKING_DIRECTORY ${MPY_PORT_DIR}   # embed.mk puts -I. on CFLAGS: this is
    RESULT_VARIABLE MPY_GEN_RESULT      # how it finds our mpconfigport.h
    OUTPUT_VARIABLE MPY_GEN_LOG
    ERROR_VARIABLE  MPY_GEN_LOG
)
if(NOT MPY_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "MicroPython embed generation failed:\n${MPY_GEN_LOG}")
endif()

# Editing the port config must regenerate the tree — the qstr pool and the
# module table are baked in at generation time, so a stale package would
# silently disagree with the config it was built from.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${MPY_PORT_DIR}/mpconfigport.h
    ${MPY_PORT_DIR}/micropython_embed.mk
    # Our own module (6B.3): the generator scans it for MP_QSTR_* and
    # MP_REGISTER_MODULE, so adding a binding means regenerating the qstr pool
    # even though CMake compiles the file itself as ordinary firmware source.
    ${GRAPHITE_ROOT}/src/scripting/mp_calc_module.c
)

# Globbing is normally a smell; here the sources are generated, so there is no
# hand-maintained list for it to drift from.
file(GLOB_RECURSE MPY_SOURCES ${MPY_EMBED_DIR}/*.c)
# Upstream's mphalport.c sends stdout straight to printf. We need it to reach
# the on-device output pane as well, so ours replaces it (src/scripting/).
list(REMOVE_ITEM MPY_SOURCES ${MPY_EMBED_DIR}/port/mphalport.c)

add_library(micropython STATIC ${MPY_SOURCES})
# SYSTEM: our own src/scripting/*.c include py/*.h, and those headers do not
# survive this project's -Wall -Wextra -Wpedantic (zero-size arrays, unused
# parameters, ISO C complaints). -isystem silences third-party headers without
# lowering the bar for the file including them, which is the whole point.
target_include_directories(micropython SYSTEM PUBLIC ${MPY_EMBED_DIR} ${MPY_PORT_DIR})
set_target_properties(micropython PROPERTIES C_STANDARD 99)
# Third-party C against this project's -Wall -Wextra -Wshadow -Wpedantic:
# same treatment as the cephes target, for the same reason.
target_compile_options(micropython PRIVATE -w)

# D96: the aarch64 register scan in shared/runtime/gchelper_generic.c uses
# GCC's `const register long x19 asm ("x19")`, which clang rejects outright.
# MICROPY_GCREGS_SETJMP is MicroPython's own documented escape hatch for
# exactly this -- setjmp spills the callee-saved registers to a buffer the
# GC can then scan portably. 135 of 136 files compile without it; 136 with.
#
# Set by the host build only. The firmware keeps the register-specific path,
# so this is one more place the two targets differ -- filed under the
# spec's general "not a fidelity emulator" warning rather than treated as
# special, though a GC-timing-sensitive bug could in principle reproduce on
# one and not the other.
if(GRAPHITE_MICROPYTHON_GCREGS_SETJMP)
    target_compile_definitions(micropython PUBLIC MICROPY_GCREGS_SETJMP=1)
endif()
