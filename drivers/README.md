# Vendored drivers

**Third-party C drivers, treated as read-only by default.** Wrap in `src/platform/` if behavior needs to change. Editing in place is the exception, not the rule — when it is unavoidable, follow "Updating a vendored driver" below and record it under [Local modifications](#local-modifications) so a re-vendor cannot silently drop it.

## Vendored sources

Vendored 2026-07-08 from Coyote OS commit `e86cf36d26e90e4891615991c1689b76fb2f90b1` (2026-02-05).

| Directory | Source | Commit / Version | License | Notes |
|-----------|--------|------------------|---------|-------|
| `lcdspi/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | `e86cf36d` | GPL-2.0 (`LICENSE.coyote-os`) | ST7365P SPI display driver; includes `fonts/font1.h`, `fonts/battery.h`; depends on `i2ckbd.h` |
| `i2ckbd/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | `e86cf36d` | GPL-2.0 (`LICENSE.coyote-os`) | STM32 I2C keyboard driver |
| `rp2040-psram/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) (upstream: [polpo/rp2040-psram](https://github.com/polpo/rp2040-psram)) | `e86cf36d` | MIT (`rp2040-psram/LICENSE`, © 2023 Ian Scott) | 8 MB PSRAM driver (PIO SPI); `examples/` and Doxyfile dropped |
| `pwm_sound/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | `e86cf36d` | GPL-2.0 (`LICENSE.coyote-os`) | PWM piezo buzzer driver |
| `coyote_reference/` | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) | `e86cf36d` | GPL-2.0 (`LICENSE.coyote-os`) | Reference headers only (`config.h` SD pinout, `keyboard_definition.h` scan codes) — not compiled |
| `fatfs/` | [elm-chan.org](http://elm-chan.org/fsw/ff/) | R0.15a | BSD-style (`fatfs/LICENSE.txt`) | SD card FAT32 filesystem; `diskio.c` is the stub template — SD SPI glue implemented in `src/platform/` (task 1.5). Local edit: `ffconf.h` LFN=1, CP437 (D8) |
| `tinyexpr/` | [codeplea/tinyexpr](https://github.com/codeplea/tinyexpr) | `4a7456e` (2025-12-12) | Zlib | C expression parser (task 2.1). Built `-DTE_POW_FROM_RIGHT` |

## Submodules

Not everything here is a vendored copy. **MicroPython is a git submodule**, and the rule that decision established is:

| Kind of dependency | How it enters the repo |
|---|---|
| Small, single-purpose, rarely-updated C driver | **Vendored** — copied in, table above, local fixes recorded below |
| Large, actively-maintained upstream project | **Submodule** — pinned to a release tag, never edited |

| Submodule | Path | Pinned to | License | Notes |
|-----------|------|-----------|---------|-------|
| [micropython/micropython](https://github.com/micropython/micropython) | `micropython/` | **v1.28.0** (`e0e9fbb1`, 2026-04-06) | MIT | Phase 6B. Read-only. Our whole configuration is `micropython_port/` — see below |

MicroPython is ~60 MB and releases every few months; hand-porting it and then hand-porting each upstream fix is not a maintenance model that survives contact with reality, which is what the vendoring rationale below implicitly assumed of everything in this directory. Pinning to a tag keeps the stability argument intact.

After cloning, or after a pull that moves the pin:

```bash
git submodule update --init --recursive
```

### `micropython_port/` — our side of the submodule

MicroPython has no build system this project can call directly. Its **embed port** instead *generates* a self-contained tree of `.c`/`.h`, which our CMake compiles as the `micropython` static library. That generation runs at configure time (see `CMakeLists.txt`) and needs `make` plus a **host** compiler — the arm toolchain is not involved until the generated sources are compiled.

`micropython_port/` is the entire local configuration, so the submodule itself is never touched:

| File | What it is |
|------|------------|
| `mpconfigport.h` | The one file that decides what the on-device Python *is*. Read twice — once by the host compiler during qstr generation, once by `arm-none-eabi` when the tree is compiled — so it lives here rather than in CMake defines |
| `micropython_embed.mk` | Runs upstream's `ports/embed/embed.mk`, and adds the `extmod/` modules the embed package does not ship by default (currently `json`) |
| `picocalc_mphal.h` | The embed port's own `mphalport.h` is one line and declares none of the HAL functions the core calls above the minimum ROM level. `mpconfigport.h` repoints `MICROPY_MPHALPORT_H` here |

The functions those headers declare are implemented in `src/scripting/mp_port.c`, which is also where the reasoning about MicroPython's longjmp-based exceptions lives.

## Why vendor instead of submodule

For the vendored drivers in the table above:

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
2. Document it in this README under the "Local modifications" section below.
3. Open an issue or PR upstream when feasible.

## Local modifications

Fixes carried in our copy that are **not** upstream. **Re-vendoring drops these** —
re-apply every row, or confirm the new upstream already contains it.

| Driver | What | Why | Reference |
|--------|------|-----|-----------|
| `fatfs/` | `ffconf.h`: `FF_USE_LFN=1`, code page 437 | Long filenames and the glyph set the UI needs | **D8** |
| `tinyexpr/` | `factor()` rewritten in the `TE_POW_FROM_RIGHT` branch: the leading sign is scanned in `factor()` and the `^` chain is built through an insertion point, so a negation stays outside the sub-chain it introduced | `(-2)^2` returned **-4** — a parenthesised negation was hoisted out of the power, because `-2` and `(-2)` are the same node once the parentheses are gone. The insertion loop had the matching bug on the right, so `2^-3^2` built `2^((-3)^2)` = 512. Upstream [issue #52](https://github.com/codeplea/tinyexpr/issues/52) (`(-1)^0 == -1`), open 2019-2026 and **fixed upstream in [`1e2ba48`](https://github.com/codeplea/tinyexpr/commit/1e2ba481) on 2026-08-05** — after our vendored `4a7456e`, and covering both defects. Their fix is verified equivalent to ours (our 46-case corpus passes on `master`), so this row is **superseded by a re-vendor**, not by more local work | **D51** |

The tinyexpr change is deliberately **iterative** rather than a recursive
`factor()`. A `^` chain contains no parentheses, so `math::Engine`'s paren-count
depth guard cannot bound it and a frame per caret would run at core 0's 4 KB
stack. See D51.
