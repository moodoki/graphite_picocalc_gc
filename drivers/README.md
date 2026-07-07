# Vendored drivers

**Read-only third-party C drivers.** Do not edit in place — wrap in `src/platform/` if behavior needs to change.

## Vendored sources

Fill in commit SHAs and dates as drivers are vendored during Phase 0, task 0.3.

| Directory | Source | Commit / Version | License | Notes |
|-----------|--------|------------------|---------|-------|
| `lcdspi/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | TBD | TBD | ST7365P SPI display driver |
| `i2ckbd/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | TBD | TBD | STM32 I2C keyboard driver |
| `rp2040-psram/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | TBD | TBD | 8 MB PSRAM driver |
| `pwm_sound/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | TBD | TBD | PWM piezo buzzer driver |
| `fatfs/` | [elm-chan.org](http://elm-chan.org/fsw/ff/) | R0.15a | BSD-style | SD card FAT32 filesystem |

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
