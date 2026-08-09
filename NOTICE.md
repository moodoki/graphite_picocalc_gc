# License notices

This project's **own code** — everything outside `drivers/` and `pico-sdk/`
(i.e. `src/`, `tests/`, `scripts/`, `docs/`, build files) — is licensed under
the **MIT License** (see [LICENSE](LICENSE)), with one carve-out: the
generated bitmap headers in **`src/gfx/fonts/`** are derived works of the
upstream fonts they were rasterized from and carry those fonts' licenses, not
MIT. They are listed separately in the table below.

However, the **combined firmware binary** links GPL-2.0 components (the
vendored display/keyboard drivers and their bitmap font). The combined work
as a whole is therefore distributed under the terms of the
**GNU General Public License v2.0** (see
[drivers/LICENSE.coyote-os](drivers/LICENSE.coyote-os)). The MIT-licensed
portions remain individually reusable under MIT.

## Third-party components

| Component | Location | License | Origin |
|-----------|----------|---------|--------|
| ST7365P display driver (`lcdspi`; its `font1.h` font is no longer compiled in — replaced by Spleen, D9) | `drivers/lcdspi/` | GPL-2.0 | [Coyote OS](https://github.com/laingcc/Picocalc-Coyote-OS) by laingcc |
| Spleen bitmap font (8x16, 5x8; generated headers in `src/gfx/fonts/`) | `drivers/spleen/` | BSD-2-Clause | [fcambus/spleen](https://github.com/fcambus/spleen) by Frederic Cambus |
| STM32 I2C keyboard driver (`i2ckbd`) | `drivers/i2ckbd/` | GPL-2.0 | Coyote OS |
| PWM buzzer driver (`pwm_sound`, currently unused) | `drivers/pwm_sound/` | GPL-2.0 | Coyote OS |
| Reference headers (not compiled) | `drivers/coyote_reference/` | GPL-2.0 | Coyote OS |
| FatFs — generic FAT filesystem | `drivers/fatfs/` | BSD-style 1-clause | [ChaN](http://elm-chan.org/fsw/ff/) |
| tinyexpr — expression parser | `drivers/tinyexpr/` | zlib | [codeplea/tinyexpr](https://github.com/codeplea/tinyexpr) |
| cephes special functions (`cprob` subset: ndtr, ndtri, incbet, incbi, igam, igami + deps) | `drivers/cephes/` | Free use per author's statement (© 1984-1995 Stephen L. Moshier; `drivers/cephes/readme-netlib.txt`) | [netlib cephes](https://www.netlib.org/cephes/) |
| rp2040-psram — PIO SPI PSRAM driver | `drivers/rp2040-psram/` | MIT (© 2023 Ian Scott) | via Coyote OS, upstream [polpo/rp2040-psram](https://github.com/polpo/rp2040-psram) |
| Raspberry Pi Pico SDK (incl. TinyUSB) | cloned alongside, linked | BSD-3-Clause (TinyUSB: MIT) | [raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk) |

## Fonts — the bitmap headers in `src/gfx/fonts/`

The 8x16 main font is a build-time choice (`-DPICOCALC_FONT=…`) and the 5x8
small font ships in every build. Each committed header under `src/gfx/fonts/`
was generated from an upstream font by `scripts/*_to_utft.py` and is a
**derived work of that font**, licensed accordingly. The upstream binaries are
mostly not vendored (see each `drivers/<font>/README.md` for the regeneration
script); the license texts are.

| Header | Upstream font | License | Notes |
|--------|---------------|---------|-------|
| `spleen8x16.h`, `spleen5x8.h` | [Spleen](https://github.com/fcambus/spleen) by Frederic Cambus | BSD-2-Clause (`drivers/spleen/LICENSE`) | `spleen5x8.h` also contains one glyph (slot 127, $\pi$) from the X11 "fixed" donor below |
| `terminus8x16.h` | [Terminus Font](https://terminus-font.sourceforge.net/) by Dimitar Toshkov Zhekov | SIL OFL 1.1 (`drivers/terminus/OFL.TXT`) | **The default build's main font.** Reserved Font Name: "Terminus Font" — see the RFN note below. Math slots 127..134 come from GNU Unifont |
| `unifont8x16.h` | [GNU Unifont](https://unifoundry.com/unifont/) | SIL OFL 1.1 **or** GPL-2.0+ with the GNU Font Embedding Exception (`drivers/unifont/COPYING`) | Copied pixel-exact, not rasterized |
| `iosevka8x16.h` | [Iosevka](https://github.com/be5invis/Iosevka) by Renzhi Li (Belleve Invis) | SIL OFL 1.1 (`drivers/iosevka/LICENSE.md`) | Rasterized from `PkgTTF-IosevkaFixed` |
| `juliamono8x16.h` | [JuliaMono](https://github.com/cormullion/juliamono) by cormullion | SIL OFL 1.1 (`drivers/juliamono/LICENSE`) | Reserved Font Name: JuliaMono |
| (donor only) | X11 "fixed" 5x8 from Markus Kuhn's [ucs-fonts](https://www.cl.cam.ac.uk/~mgk25/ucs-fonts.html) | Public domain (`drivers/ucs-fixed/5x8.bdf` COPYRIGHT) | Donates the $\pi$ glyph to `spleen5x8.h` |

**Reserved Font Names.** The OFL fonts above are used in modified form (subset
to 8-wide cells, remapped into a 135-slot table, and in Terminus's case with
eight math glyphs substituted from Unifont). OFL 1.1 §5 forbids distributing a
modified version under a Reserved Font Name. These headers are internal C
symbols rather than distributed font files, and the identifiers
(`terminus8x16`, `juliamono8x16`) name the *source* the bitmaps came from — but
if any of these is ever exported as a font file, or the build option is
surfaced to users as a font name, rename it first. Spleen (BSD-2-Clause) and
the public-domain X11 donor carry no such restriction, which is part of why
Spleen remains the fallback.

Per-component license texts live next to the code (`drivers/LICENSE.coyote-os`,
`drivers/fatfs/LICENSE.txt`, `drivers/tinyexpr/LICENSE`,
`drivers/rp2040-psram/LICENSE`, `drivers/spleen/LICENSE`) and must be retained
in redistributions.

## Path to a fully permissive release (not planned — future option)

The GPL surface is confined to `drivers/lcdspi`, `drivers/i2ckbd`, and
`drivers/pwm_sound`. The project's `platform/` HAL was designed so these are
replaceable. To make the entire firmware MIT, the work would be roughly:

1. **Display driver rewrite** (largest item, ~2–3 sessions incl. HW
   verification): reimplement the ST7365P SPI init/push path from the
   datasheet — not by transcribing the GPL code. `picocalc_diag` exists as
   the bring-up harness.
2. **Keyboard driver rewrite** (~1 session): the STM32 I2C register protocol
   is small and well understood.
3. **Font replacement** — done 2026-07-18 (D9): `font1.h` swapped for
   Spleen (BSD-2-Clause, `drivers/spleen/`).
4. **Drop `pwm_sound`** from the build (already unused) until an audio HAL is
   written fresh.
5. Re-derive anything sourced from `coyote_reference/` headers (pin maps and
   scan codes are facts, but re-check), then flip README/NOTICE.

Also worth doing beforehand: confirm with the Coyote OS upstream that its own
driver ancestry is clean GPL-2.0 (community PicoCalc drivers have mixed
provenance).
