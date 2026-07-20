# Terminus (font test-drive)

[Terminus Font](https://terminus-font.sourceforge.net/) by Dimitar
Toshkov Zhekov, under the SIL Open Font License 1.1 (see
[`OFL.TXT`](./OFL.TXT)). Reserved Font Name: "Terminus Font".

Used only as an optional **test-drive** alternative to the default Spleen
8x16 main font (testdrive 2026-07-20). Enable with:

```sh
cmake -DPICOCALC_FONT=terminus ...
```

The ASCII range (32..126) comes from the Terminus `ter-u16n` BDF; the
math/complex high slots (127..134: π ∠ θ σ Σ χ μ and the imaginary-unit
`i` = U+2139) are baked **pixel-exact from GNU Unifont** (see
`drivers/unifont/`), so the math set matches the Unifont build regardless
of Terminus's own Greek.

Neither the Terminus BDF nor the Unifont `.hex` is vendored. The
committed bitmap header `src/gfx/fonts/terminus8x16.h` is the artifact;
regenerate it (fetching both to a temp dir) with:

```sh
./scripts/gen-terminus.sh
```
