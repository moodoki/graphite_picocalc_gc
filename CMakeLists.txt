cmake_minimum_required(VERSION 3.20)

# Pull in the Pico SDK (must come before project()).
include(pico_sdk_import.cmake)

project(picocalc_graphcalc
    VERSION   0.1.0
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

# ---- Main executable ----
add_executable(picocalc_graphcalc
    src/main.cpp
    src/platform/display.cpp
    src/platform/keyboard.cpp
    src/platform/platform.cpp
    src/platform/psram.cpp
    src/platform/sd_card.cpp
    src/platform/sd_diskio.cpp
    src/platform/storage.cpp
    src/platform/system.cpp
    src/gfx/font.cpp
    src/gfx/framebuffer.cpp
    src/ui/screen_manager.cpp
    src/ui/input_line.cpp
    src/ui/chrome.cpp
    src/math/engine.cpp
    src/math/functions.cpp
    src/math/format.cpp
    src/math/catalog.cpp
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
    src/graph/trace.cpp
    src/apps/nav.cpp
    src/apps/home_screen.cpp
    src/apps/files_screen.cpp
    src/apps/graph_model.cpp
    src/apps/slot_editor.cpp
    src/apps/y_editor.cpp
    src/apps/param_editor.cpp
    src/apps/polar_editor.cpp
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

# rp2040-psram is an INTERFACE library: its sources compile as part of
# this target, so its pin configuration is set here (PicoCalc mainboard).
target_compile_definitions(picocalc_graphcalc PRIVATE
    PSRAM_MUTEX=1
    PSRAM_ASYNC=0
    PSRAM_PIN_CS=20
    PSRAM_PIN_SCK=21
    PSRAM_PIN_MOSI=2
    PSRAM_PIN_MISO=3
)

target_link_libraries(picocalc_graphcalc
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pio
    hardware_pwm
    hardware_timer
    i2ckbd
    lcdspi
    pwm_sound
    rp2040-psram
    fatfs
    tinyexpr
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
