# Spleen bitmap font (vendored)

Source: https://github.com/fcambus/spleen (v2.2.0), BSD 2-Clause — see
`LICENSE`. Only the two sizes the firmware uses are vendored.

The C headers in `src/gfx/fonts/` are generated from these BDFs (UTFT
layout read by `gfx::Font`, ASCII 32–126):

```sh
python3 scripts/bdf_to_utft.py drivers/spleen/spleen-8x16.bdf spleen8x16 > src/gfx/fonts/spleen8x16.h
python3 scripts/bdf_to_utft.py drivers/spleen/spleen-5x8.bdf  spleen5x8  > src/gfx/fonts/spleen5x8.h
```

Adopted 2026-07-18 (D9): replaced the interim Coyote OS `font1` 8x12 —
also the first step of the D17 permissive-licensing path.
