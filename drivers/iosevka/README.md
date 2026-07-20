# Iosevka Fixed (font test-drive)

[Iosevka](https://github.com/be5invis/Iosevka) by Renzhi Li (Belleve
Invis), under the SIL Open Font License 1.1 (see
[`LICENSE.md`](./LICENSE.md)).

Used only as an optional **test-drive** alternative to the default Spleen
8x16 main font (testdrive 2026-07-20). Enable with:

```sh
cmake -DPICOCALC_FONT=iosevka ...
```

The upstream `PkgTTF-IosevkaFixed` zip (~140 MB) is **not vendored**. The
committed bitmap header `src/gfx/fonts/iosevka8x16.h` is the artifact;
regenerate it (fetching + extracting the TTF to a temp dir) with:

```sh
./scripts/gen-iosevka.sh          # or IOSEVKA_VER=34.7.0 ./scripts/gen-iosevka.sh
```

The bitmap header is a derivative rendering of the font, so this license
and copyright accompany it per the OFL.
