# Testdrive observations — 2026-08-22

**Board/build:** Pico 2, latest `phase-6` branch tip.

Raw feedback from a soak session, not yet investigated or fixed.

## Observations (verbatim)

1. **Periodic table app** (`examples/apps/periodic/`): "Works as expected, but
   could look better if element boxes have a outline, currently a little
   difficult to read."

2. **SD app manifests / launcher tiles** (6B.15/16): "Works as expected."

3. **calc module bindings** (`calc.eval`/`plot`/`draw_*`/keys/files/lists/
   matrices): didn't test this session.

4. **General script editing** (ProgramScreen: write, RUN, save, reload, ESC
   navigation): "Works as expected."

5. **File browser**: "File list count draws over top status bar, file sizes
   difficult to read, human readable formatting will help. We can use diff
   colors for folders, scripts, text, and calc data files."

6. **Python editor** (`ProgramScreen` / `TextEditorWidget`): "keywords base
   syntax highlighting will be helpful in python editor."

7. **File browser**: "file browser should sort files by names and with
   directories listed at the top."
