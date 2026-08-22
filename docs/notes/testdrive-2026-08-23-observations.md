# Testdrive observations — 2026-08-23

**Board/build:** Pico 1 H (RP2040), `phase-6` at `d6882b1`.

Raw feedback from the board-swap hardware pass, not yet investigated or fixed.

The verified-working items from the same pass (file browser move/open/colours/
sort/counter, the Y= hint row, trace peak values, the periodic table app) are
recorded on their GitHub issues; what follows is only the usage friction.

## Observations (verbatim)

1. **ESC behaviour in the file browser and the periodic table app**:

   > "ESC" muscle memory expects to return to previous screen. However, the
   > behavior, while correct, feels awkwards in files and in Periodic table
   > app. In files, after entering a directory, ESC feels like it should go
   > back to the previous directory, but it exits files instead. In periodic
   > app, when entering the keys screen, ESC feels like it should return to
   > the periodic table, but it interrupts and needs another ESC key press to
   > exit periodic. ESC also needs to be pressed twice to exit the periodic
   > table app.

## Areas asked about with nothing to add

- **General navigation** beyond the ESC item above: nothing to add.
- **File browser** beyond the ESC item above: nothing to add.
