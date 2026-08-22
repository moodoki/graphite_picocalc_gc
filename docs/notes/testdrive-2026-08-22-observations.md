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

8. **SD card layout**: "we might also want to keep calc data files in a sub
   folder now that we have other files in the picocalc dir."

9. **SD card layout, TODO**: "since we name the calculator project graphite,
   let's also change the directory for the files to graphite. migration to
   new dir structure should be done manually with a script. we can provide
   this as a micro python script if file operations are supported. keep this
   as a todo, since it involves some design to ensure clean migration and
   backwards compatibility should we swap to an older firmware."

10. **File browser**: "file browser can't move files." (clarified: can't move
    files to another folder; not supported today)

11. **File browser**: "file browser should be able to open scripts and notes
    in the appropriate app."

12. **App launcher, design consideration**: "might be useful to differentiate
    between python apps and native compiled apps (and eventually uf2 apps if
    shipped) in the apps menu, keep this as a design consideration."
