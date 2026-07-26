# X11 "fixed" 5x8 (ucs-fonts)

`5x8.bdf` from Markus Kuhn's [ucs-fonts](https://www.cl.cam.ac.uk/~mgk25/ucs-fonts.html)
package (the classic X11 `misc-fixed` family with Unicode coverage).
Public domain — the BDF's own COPYRIGHT reads "Public domain font.
Share and enjoy."

Used only as a **glyph donor** for the Spleen 5x8 small font: the Spleen
BDF has no Greek at this size, so `scripts/gen-fonts.sh` maps slot 127
(π, U+03C0) from this font via `bdf_to_utft.py --donor`. Both fonts share
the same cell metrics (5x8, ascent 7), so the glyph bakes in unshifted.

Vendored (unlike the fetched test-drive 8x16 fonts) because the small
font ships in every build and the default-font regen should stay offline,
matching `drivers/spleen/`.
