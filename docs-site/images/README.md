# Generated screenshots

**Do not edit these images by hand.** Every `.png` here is drawn by the
firmware itself, running as a native application, and regenerated with:

```sh
cmake -B build/host -S host && cmake --build build/host
python3 scripts/gen-doc-images.py
```

CI runs `python3 scripts/gen-doc-images.py --check` on every pull request
and fails if a committed image no longer matches what the firmware draws
(D98). So a UI change that alters a documented screen will fail the build
until the images are regenerated — which is the point. The alternative is
the state issue [#33](https://github.com/moodoki/graphite_picocalc_gc/issues/33)
was opened about: screenshots that look authoritative and are quietly wrong.

## What these are, and what they are not

These come from the **host build**, a development instrument rather than a
fidelity emulator. The pixels are exactly what the firmware's renderer
produces, which is what makes them useful for glyph and layout work. They
are **not** photographs of the panel, and they do not model the display
driver, the colour depth conversion, or anything about timing. The
photograph in the project README is doing a different job on purpose.

`scripts/` holds the key scripts that navigate to screens which cannot be
reached by evaluating an expression — the file manager, for instance. The
key names are the ones `platform::key_names` defines, shared with the
MicroPython bindings so the two cannot drift apart.

## Adding an image

Add an entry to `IMAGES` in `scripts/gen-doc-images.py`, run it, and commit
the `.png` alongside your change.
