# Hardware Reference

## ClockworkPi PicoCalc

The PicoCalc consists of a swappable Raspberry Pi Pico module mounted on the **ClockworkPi v2.0 mainboard**, which provides display, keyboard, audio, storage, and power management peripherals shared across all Pico module variants.

## Pico module comparison

| | Pico 1 H (RP2040) | Pico 2 H (RP2350) |
|---|---|---|
| **CPU** | 2$\times$ Cortex-M0+ | 2$\times$ Cortex-M33 |
| **Clock** | 133 MHz (overclockable to 200–250 MHz) | 150 MHz (stock) |
| **SRAM** | 264 KB | 520 KB |
| **Flash** | 2 MB onboard | 4 MB onboard |
| **FPU** | None — software float via ROM routines | Single-precision hardware |
| **DSP** | None | SIMD, saturating math |
| **GPIO** | 26 exposed | 26 exposed |
| **Bootrom math** | Hand-optimized softfloat (QFPLIB-derived) | Standard with FPU |
| **PIO** | 2$\times$ state machines (4 each) | 3$\times$ state machines (4 each) |

## Mainboard peripherals (constant across modules)

### Display

- **Panel**: 4-inch IPS LCD, 320$\times$320 pixels, RGB565
- **Controller**: ST7365P (~99% command-compatible with ILI9488)
- **Interface**: 4-wire SPI
- **Backlight**: PWM-controlled via STM32 co-processor
- **Refresh strategy**: PIO + DMA on core 1, line-buffered or full-framebuffer mode

### Keyboard

- **Layout**: 67-key QWERTY, calculator-style
- **Scanning**: managed by an STM32 co-processor on the mainboard
- **Interface**: I2C between STM32 and the Pico module
- **I2C address**: `0x1F` (verify against latest Coyote OS `keyboard_definition.h`)
- **Function keys**: F1–F6 along the top row
- **Modifier keys**: Shift, Ctrl, Alt; "2nd" and "Alpha" are software-only (mapped from key combinations)

### External memory

- **PSRAM**: 8 MB, SPI-attached
  - Read/write speed: ~30–40 Mbit/s sequential
  - Random-access write: ~30$\times$ slower than SRAM
  - Use cases: framebuffer (Pico 1), large data, CAS pool, MicroPython heap (Pico 2 optional)
- **SD card**: full-size slot, 32 GB FAT32 card included
  - File system: FatFs (vendored as `drivers/fatfs/`)
  - Use cases: programs, saved variables, history, configuration, app data

### Audio

- **Output**: piezo buzzer, PWM-driven
- **Drivers**: `drivers/pwm_sound/`
- **Use cases**: optional UI feedback, alarms, simple tones

### Power

- **Battery**: 18650 Li-ion cell
- **Charging**: built-in circuit, USB-C input
- **Voltage monitoring**: exposed via ADC on the Pico (TBD: confirm pin from schematic)

## Pinout reference

Keep canonical pin assignments here as they're discovered during driver integration. Update from `drivers/<driver>/` source files and the ClockworkPi schematic.

| Function | Pico GPIO | Notes |
|----------|-----------|-------|
| LCD SPI MOSI | TBD | From `lcdspi/` |
| LCD SPI SCK | TBD | From `lcdspi/` |
| LCD CS | TBD | |
| LCD DC | TBD | Data/Command |
| LCD RST | TBD | |
| Keyboard I2C SDA | TBD | From `i2ckbd/` |
| Keyboard I2C SCL | TBD | |
| PSRAM SPI MOSI | TBD | From `rp2040-psram/` |
| PSRAM SPI MISO | TBD | |
| PSRAM SPI SCK | TBD | |
| PSRAM CS | TBD | |
| SD SPI MOSI | TBD | |
| SD SPI MISO | TBD | |
| SD SPI SCK | TBD | |
| SD CS | TBD | |
| Audio PWM | TBD | From `pwm_sound/` |
| Battery monitor ADC | TBD | |

## Performance benchmarks

Empirical numbers gathered during development. Update with measurements from each major task.

### Floating-point operations

| Operation | Pico 1 (133 MHz, softfloat) | Pico 2 (150 MHz, FPU) |
|-----------|----------------------------|----------------------|
| `double + double` | ~50 cycles | ~5 cycles |
| `double * double` | ~100 cycles | ~5 cycles |
| `sin(double)` | ~250 cycles | ~50 cycles |
| `exp(double)` | ~300 cycles | ~80 cycles |
| `pow(double, double)` | ~500 cycles | ~150 cycles |

### Display rendering

| Operation | Pico 1 | Pico 2 |
|-----------|--------|--------|
| Full screen clear (320$\times$320) | TBD | TBD |
| 320-point function plot | TBD | TBD |
| Full frame line-buffer DMA | TBD | TBD |

(Fill in from week 7–8 task 4.2 profiling.)

## Schematic and datasheets

Canonical hardware references:

- ClockworkPi PicoCalc schematic — https://github.com/clockworkpi/PicoCalc/blob/master/clockwork_Mainboard_V2.0_Schematic.pdf
- ST7365P datasheet — https://github.com/clockworkpi/PicoCalc/blob/master/ST7365P_SPEC_V1.0.pdf
- RP2040 datasheet — https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
- RP2350 datasheet — https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf
- Pico 1 SDK datasheet — https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf
- Pico 2 SDK datasheet — https://datasheets.raspberrypi.com/picow/connecting-to-the-internet-with-pico-w.pdf

## Known limitations

- The on-board LED GPIO differs between Pico 1 (GPIO 25) and Pico 2 (different pin and connection — verify from board files). Use the `PICO_DEFAULT_LED_PIN` macro from the SDK rather than hard-coding.
- Without a hardware FPU, `double` operations on Pico 1 are ~5$\times$ slower than on Pico 2. Profile heavy compute paths and consider `float` for graph-evaluation hot loops if needed.
- PSRAM access is SPI-mediated; random-access patterns are dramatically slower than sequential. Layout data structures with sequential access in mind when stored in PSRAM.
- The keyboard co-processor introduces ~1–5 ms latency on key events. Not noticeable for typing, but be aware for fast-input scenarios (key-repeat tuning).
