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

# ---- Main executable ----
add_executable(picocalc_graphcalc
    src/main.cpp
    # More sources added in Phase 1 tasks.
)

target_include_directories(picocalc_graphcalc PRIVATE
    src/
    drivers/
)

target_link_libraries(picocalc_graphcalc
    pico_stdlib
    pico_multicore
    # SDK libraries added as drivers come online:
    # hardware_spi   (display, PSRAM, SD card)
    # hardware_i2c   (keyboard)
    # hardware_dma   (display refresh)
    # hardware_pio   (display refresh)
    # hardware_pwm   (audio, backlight)
    # hardware_interp (softfloat acceleration on Pico 1)
)

# Generate .uf2, .elf, .bin, .hex, .map alongside the binary.
pico_add_extra_outputs(picocalc_graphcalc)

# Enable USB stdio for printf debugging during development.
pico_enable_stdio_usb(picocalc_graphcalc  1)
pico_enable_stdio_uart(picocalc_graphcalc 0)
