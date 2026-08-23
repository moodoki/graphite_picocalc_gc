# Dependencies

All third-party code used in this project, with sources, versions, and licenses. Update this when adding or removing a dependency.

## Toolchain (host)

| Component | Version | License | Source |
|-----------|---------|---------|--------|
| ARM GNU Toolchain | $\geq$ 13.2 | GPL (with runtime exceptions) | https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads |
| CMake | $\geq$ 3.20 | BSD 3-Clause | https://cmake.org |
| Ninja | latest | Apache 2.0 | https://ninja-build.org |
| clangd / clang-format / clang-tidy | $\geq$ 18 | Apache 2.0 with LLVM exceptions | https://clangd.llvm.org |
| Python | $\geq$ 3.10 | PSF | https://python.org |

## Pico SDK (build dependency)

| Component | Version | License | Source |
|-----------|---------|---------|--------|
| pico-sdk | 2.2.0 (tag; checked out in-repo at `./pico-sdk`, gitignored) | BSD 3-Clause | https://github.com/raspberrypi/pico-sdk |

Submodules pulled in by the Pico SDK include TinyUSB, cyw43-driver, and others. Their licenses are documented in the SDK's `LICENSE.TXT`.

## Vendored runtime libraries

These live in `drivers/` as read-only copies. They are vendored (not git submodules) because we want stability and don't expect to track upstream changes closely.

| Directory | Source | Version (commit/release) | License | Notes |
|-----------|--------|-------------------------|---------|-------|
| `drivers/lcdspi/` | Coyote OS | `e86cf36d` (2026-02-05) | GPL-2.0 | ST7365P SPI display driver |
| `drivers/i2ckbd/` | Coyote OS | `e86cf36d` | GPL-2.0 | STM32 I2C keyboard driver |
| `drivers/rp2040-psram/` | Coyote OS (upstream polpo/rp2040-psram) | `e86cf36d` | MIT | 8 MB PSRAM driver |
| `drivers/pwm_sound/` | Coyote OS | `e86cf36d` | GPL-2.0 | PWM piezo buzzer driver |
| `drivers/fatfs/` | http://elm-chan.org/fsw/ff/ | R0.15a | BSD-style (FatFs license) | SD card FAT32 filesystem |

Note: GPL-2.0 vendored drivers make the combined firmware GPL-2.0 when distributed. Fine for this personal-use project; revisit before any public release.

## Source dependencies (Phase 1)

| Library | Version | License | Source | Used for |
|---------|---------|---------|--------|----------|
| tinyexpr (C) | `4a7456e` (2025-12-12) | Zlib | https://github.com/codeplea/tinyexpr | Numeric expression parsing & evaluation |

Chose the original C `tinyexpr` over `tinyexpr++`: the C version is a single ~900-line
`.c`/`.h` pair with a plain C ABI (no exceptions/STL — a natural fit for `-fno-exceptions`
`-fno-rtti`), and the extended function library the spec wants is registered from our C++
side anyway (`src/math/functions.cpp`). tinyexpr++ pulls in `std::` machinery and 68+
`throw` sites we'd have to compile around. Vendored to `drivers/tinyexpr/`; built with
`-DTE_POW_FROM_RIGHT` for TI-style right-associative `^`.

## Source dependencies (Phase 3)

| Library | Version | License | Source | Used for |
|---------|---------|---------|--------|----------|
| cephes (`cprob` subset, 12 files) | netlib release 2.7 (fetched 2026-07-19) | Moshier free-use statement (see `drivers/cephes/readme-netlib.txt`) | https://www.netlib.org/cephes/cprob.tgz | Special functions for 3C distributions: `ndtr`/`ndtri` (normal), `incbet`/`incbi` (t, F, binomial), `igam`/`igamc`/`igami` (chi-square, Poisson), plus their deps (`gamma`/`lgam`, `polevl`, `const`, `expx2`, `mtherr`) |

Vendored per spec §5.3 ("cherry-pick from cephes" — task 3C.1) into
`drivers/cephes/`; see its `README.md` for the file list, the
not-vendored integer-df wrappers, and the CMake symbol renames
(`gamma`/`erf`/`erfc` → `cephes_*`) that prevent libm collisions.

## Git submodules

Unlike everything above, these are **not** vendored copies. The rule from D71:
vendor by default, but a dependency too large and too actively maintained to
hand-port comes in as a submodule instead. MicroPython is the first of these
and set the precedent.

| Path | Pinned to | License | Source | Used for |
|------|-----------|---------|--------|----------|
| `drivers/micropython/` | `v1.28.0` (D71) | MIT | https://github.com/micropython/micropython | The embedded Python interpreter (Phase 6B) |

It has no build system we can call directly, so `drivers/micropython_port/`
holds our port layer — `mpconfigport.h`, `picocalc_mphal.h` and
`micropython_embed.mk`, which drives upstream's **embed port** to generate a
self-contained C tree that our CMake then compiles as the `micropython`
target. A fresh clone needs:

```bash
git submodule update --init --recursive
```

The build fails with an explicit message if that has not been run.

## Reference projects (NOT linked or vendored, used for design only)

| Project | License | Purpose |
|---------|---------|---------|
| Delta Pico (`rbop`) | MIT | Natural math renderer architecture reference |
| DB48X | GPL v3 | Embedded CAS architecture reference |

We borrow design ideas (layout-node tree, expression rewriting strategies) but no code is copied from GPL-licensed sources into this project.

## Adding a dependency

1. **Open an issue or write a note in `docs/notes/decisions.md`** describing what the dependency provides and why it's needed.
2. **Verify the license** is compatible with this project (currently TBD — defaulting to "personal use, license TBD").
3. **Vendor** rather than git-submodule unless you actually want to track upstream:
   - Copy source into `drivers/<name>/` or `src/<subsystem>/<name>/`
   - Add a row to the table above with source URL, version, and license
   - Note the commit SHA (or release version) — it makes future updates traceable
4. **Update `CMakeLists.txt`** to compile the new sources and link them.
5. **Run lint and build**.
6. **Commit** with message: `deps: add <name> for <purpose>`.

## Removing a dependency

1. Verify nothing references it: `grep -r '<header-name>' src/`
2. Delete its directory from `drivers/` or wherever it lives.
3. Remove its row from the table.
4. Update `CMakeLists.txt`.
5. Commit: `deps: remove <name>`.
