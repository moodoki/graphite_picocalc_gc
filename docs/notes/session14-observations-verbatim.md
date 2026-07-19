# Session 14 firmware — on-device observations (2026-07-19)

Logged for a future session — **no fixes applied yet** (developer
instruction). Spot-check result first, then the open items, then the
code findings that answer the questions raised.

Verified OK this eval: **new features and functions spot-check OK,
docs/catalogue OK** (Session 14 HW-PENDING row cleared in the worklog).

## Usage notes (verbatim)

1. In function entry screen, long expressions and selection box
   overlaps, hurts readability, we can truncate and show `...`.
2. When powering on after an extended time, sdcard might show no card.
   This can be resolved with a reboot from serial. What's the
   backoff/retry that we have now? Maybe it needs to wait longer.
3. We can show sdcard and psram errors in the top bar as well, use a
   red `SD` to indicate SD card abnormal, and red `PSRAM` text for
   PSRAM abnormal. They can disappear when the retries finally work.
4. Additionally, how does sdcard eject and insertion when powered on
   get handled?

## Investigation notes (answers, Session 14)

**Current SD/PSRAM backoff/retry** (`src/main.cpp` late-init loop,
D14): one init attempt at boot, then retries every **2 s**
(`kLateInitGapMs`) but only inside the first **30 s of uptime**
(`kLateInitWindowMs`). Each retry re-attempts `psram().reinit()` and
`storage().init()` and re-runs the self-tests (a failing SD attempt
with a card inserted blocks ~1 s, hence the spacing). **After 30 s the
retries stop permanently** — that is exactly the observed symptom: if
the card/rail needs longer than the window after an extended
power-off, nothing ever retries again and only a reboot (warm, so no
rail settle needed) recovers. Candidate fixes for the future session:
lengthen the window, or better, drop the window entirely and keep a
slow retry heartbeat (e.g. every 5-10 s) whenever SD or PSRAM is
unhealthy — which is also what the proposed top-bar indicators want
("disappear when the retries finally work" implies retries that never
give up).

**Runtime eject/insert: effectively not handled today.** The DET pin
exists and `sd::card_present()` reads it, but it is only consulted
inside `sd::init()` — nothing polls it after boot.

- **Eject while on**: `storage().mounted_` stays true and FatFs
  `disk_status()` keeps reporting OK (`g_initialized` is only cleared
  by a failed init), so block I/O just starts failing → FatFs errors
  surface as save/load errors; the files screen shows "read error"
  (not "no SD card", since `mounted()` is still true). No unmount, no
  re-detection.
- **Re-insert while on**: nothing re-runs `storage().init()` once the
  30 s window has closed (and even inside it, only when
  `g_init_status.storage` is false — a boot-mounted-then-ejected card
  never re-mounts). Card stays unusable until reboot.
- A future hot-plug pass would poll DET on a slow cadence: on removal,
  unmount + mark storage down (red `SD` indicator); on insertion,
  re-init + remount + reload persisted state, with the indicator
  clearing on success — the same machinery the retry-forever change
  needs.

**Y=/function editor overlap** (note 1): slot editor rows draw the
full expression text over the fixed row layout; long expressions run
under/through the selection highlight of the next row. Fix sketch for
later: clip to the row width and truncate with `...` (the list-editor
cells and history results already do width-capped truncation — reuse
that pattern).

## Disposition

To be worked in a future session alongside sub-phase 3D (record
decisions when made): retry-forever + top-bar red `SD`/`PSRAM`
indicators + hot-plug handling are one coherent storage-health batch;
the editor truncation is a small standalone UI fix.
