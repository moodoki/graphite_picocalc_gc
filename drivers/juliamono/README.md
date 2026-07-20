# JuliaMono (font test-drive)

[JuliaMono](https://github.com/cormullion/juliamono) by cormullion, under
the SIL Open Font License 1.1 (see [`LICENSE`](./LICENSE)). Reserved Font
Name: JuliaMono.

Used only as an optional **test-drive** alternative to the default Spleen
8x16 main font (testdrive 2026-07-20 observation: real math glyphs instead
of ASCII stand-ins). Enable with:

```sh
cmake -DPICOCALC_FONT=juliamono ...
```

The 3.2 MB TTF is **not vendored** here to keep the repo light. The
committed bitmap header `src/gfx/fonts/juliamono8x16.h` is the artifact;
regenerate it (fetching the TTF to a temp dir) with:

```sh
./scripts/gen-juliamono.sh
```

The bitmap header is a derivative rendering of the font, so this license
and copyright accompany it per the OFL.
