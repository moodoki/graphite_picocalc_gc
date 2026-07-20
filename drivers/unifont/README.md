# GNU Unifont (font test-drive)

[GNU Unifont](https://unifoundry.com/unifont/) — a native 8x16 bitmap
font. Its glyph data is dual-licensed under the **SIL Open Font License
1.1** and the **GNU GPL v2+ with the GNU Font Embedding Exception**; see
[`COPYING`](./COPYING).

Used only as an optional **test-drive** alternative to the default Spleen
8x16 main font (testdrive 2026-07-20). Enable with:

```sh
cmake -DPICOCALC_FONT=unifont ...
```

Unlike the JuliaMono/Iosevka variants, Unifont is **not rasterized** — it
is already an 8x16 pixel font, so its 8-wide glyphs are copied
byte-for-byte from the upstream `unifont_all` `.hex` (pixel-exact).
Unifont's true math-italic `i` (U+1D456) is 16 wide and won't fit, so the
imaginary-unit slot (134) maps to Unifont's own **U+2139** — a serif `i`
that stays visually distinct from a plain variable `i`.

The `.hex` (~8 MB) is **not vendored**. The committed bitmap header
`src/gfx/fonts/unifont8x16.h` is the artifact; regenerate it (fetching
the `.hex` to a temp dir) with:

```sh
./scripts/gen-unifont.sh          # or UNIFONT_VER=17.0.05 ./scripts/gen-unifont.sh
```
