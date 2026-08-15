# Example SD apps

Copy either directory to `/picocalc/apps/` on the SD card and it becomes
its own tile in the launcher (`F6` from the home screen, or type `apps`).

```
/picocalc/apps/hello/app.txt
/picocalc/apps/hello/main.py
```

The card is scanned once at boot and again whenever it is re-mounted, so
a card swap replaces the tiles rather than duplicating them — but adding
an app to a card that is already in the slot needs a reboot or an eject
and re-insert.

## app.txt

Flat `key=value`, one per line. `#` starts a comment, blank lines are
ignored, both sides of the `=` are trimmed, and keys are matched
case-insensitively.

| Key | Meaning | Default |
|---|---|---|
| `name` | the launcher row's text (23 chars) | the directory's own name |
| `icon` | a single glyph, optional | none |
| `entry` | the script to run, relative to the app's directory or absolute | `main.py` |
| `type` | `script` (or `python`) | `script` |

So the smallest working app is a directory containing an **empty**
`app.txt` and a `main.py`.

An app is skipped — logged over serial, never fatal — when its `entry`
names a file that does not exist, when the composed path is too long for
the 64-byte field, or when `type` claims to be something this firmware
cannot run.

## What an app can do

Everything the `RUN` key can: an SD app is the same interpreter, on the
same calculator, with the same `calc` module (see [USAGE.md](../../USAGE.md)).
Variables it stores are still there on the home screen afterwards.

Three examples ship here:

| App | What it shows |
|---|---|
| `hello` | The smallest thing that works — draws, keeps the screen, reads a variable back out of the calculator |
| `quadratic` | `calc.input` for numbers, printed results, and a plot handed to the graph screen |
| `periodic` | A full interactive app: an 18-column table walked with the arrow keys, coloured by element series, with its data in an editable CSV next to it |

Two conventions are worth knowing, one for each of the first two:

- **`calc.clear_screen()` takes the screen** (`hello`). Your drawing is
  what stays up when the script ends. `ESC` gives the screen back — and
  `ESC` also interrupts a running script, so a drawing loop that never
  ends is not a reason to pull the battery.
- **Otherwise the output pane appears** (`quadratic`), with everything
  the script printed. `calc.draw_rect` and `calc.draw_text` draw without
  taking the screen, which is how `quadratic` gets a clean field for
  `calc.input` and still ends on its printed results.

From either one, `ESC` returns to the launcher rather than dropping into
the program editor — an app never touches the editor's buffer, so
whatever you were writing there is untouched.
