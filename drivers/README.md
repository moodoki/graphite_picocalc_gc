# Vendored drivers

**Read-only third-party C drivers.** Do not edit in place — wrap in `src/platform/` if behavior needs to change.

## Vendored sources

Vendored 2026-07-08 from Coyote OS commit `e86cf36d26e90e4891615991c1689b76fb2f90b1` (2026-02-05).

| Directory | Source | Commit / Version | License | Notes |
|-----------|--------|------------------|---------|-------|
| `lcdspi/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | `e86cf36d` | GPL-2.0 (`LICENSE.coyote-os`) | ST7365P SPI display driver; includes `fonts/font1.h`, `fonts/battery.h`; depends on `i2ckbd.h` |
| `i2ckbd/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | `e86cf36d` | GPL-2.0 (`LICENSE.coyote-os`) | STM32 I2C keyboard driver |
| `rp2040-psram/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) (upstream: [polpo/rp2040-psram](https://github.com/polpo/rp2040-psram)) | `e86cf36d` | MIT (`rp2040-psram/LICENSE`, © 2023 Ian Scott) | 8 MB PSRAM driver (PIO SPI); `examples/` and Doxyfile dropped |
| `pwm_sound/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | `e86cf36d` | GPL-2.0 (`LICENSE.coyote-os`) | PWM piezo buzzer driver |
| `coyote_reference/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | `e86cf36d` | GPL-2.0 (`LICENSE.coyote-os`) | Reference headers only (`config.h` SD pinout, `keyboard_definition.h` scan codes) — not compiled |
| `fatfs/` | [elm-chan.org](http://elm-chan.org/fsw/ff/) | R0.15a | BSD-style (`fatfs/LICENSE.txt`) | SD card FAT32 filesystem; `diskio.c` is the stub template — SD SPI glue implemented in `src/platform/` (task 1.5) |

## Why vendor instead of submodule

- **Stability**: drivers don't get accidentally updated to a breaking version.
- **Self-contained build**: cloning the repo doesn't require `--recurse-submodules` discipline.
- **Local fixes**: when something needs changing, it stays in this repo with our commits.

## Updating a vendored driver

If upstream fixes a bug or adds a feature we need:

1. Identify the upstream commit.
2. Manually port the change into our copy (or, if the change is large, re-vendor the whole driver).
3. Update the commit SHA / version in the table above.
4. Run our test suite (Phase 1+) and confirm nothing regresses.
5. Commit with: `drivers: update <name> to <version>`.

If we make a local fix not yet upstream:

1. Apply the fix in-place.
2. Document it in this README under a "Local modifications" section (added on first such fix).
3. Open an issue or PR upstream when feasible.
