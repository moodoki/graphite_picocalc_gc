# Using the calculator

A tour of the screens and their keys.

The firmware also carries its own help: type **`help`** (or **`?`**) on the home
screen for the function catalog, per-screen key reference and syntax notes.
That is the authoritative reference, it ships with the build you are running,
and it cannot drift — the catalog it shows is the same table the expression
parser registers from. The web copies are
[Function catalog](docs-site/reference/function-catalog.md) and
[Key reference](docs-site/reference/key-reference.md), generated from the same
source.

For what the calculator can do, see [FEATURES.md](FEATURES.md). For building
and flashing, see the [README](README.md#quick-start-building-from-source).

## A note on modifier keys

The PicoCalc's keyboard is an STM32 co-processor that translates Shift chords
into their own scan codes rather than passing Shift through to the host — and
it swallows shifted arrows entirely (`Shift+Enter` arrives as `INS`). So every
binding in this firmware uses **Alt** or **Ctrl**, never Shift. Function keys
past `F5` are Shift chords the keyboard reports directly: `F6` = `Shift+F1`,
`F7` = `Shift+F2`, `F8` = `Shift+F3`.

## The five softkeys, everywhere

The same five keys mean the same thing on every screen:

| Key | Goes to |
|-----|---------|
| `F1` EDIT | The active graph mode's editor (Y= / PAR / POLAR / SEQ) |
| `F2` WIN | Window settings |
| `F3` MODE | Mode settings |
| `F4` TRC | Trace — opens the graph if you are not already there |
| `F5` GRPH | Graph, and graph ↔ table from there |
| `HOME` | Back to the home screen |
| `ESC` | Back, or cancel an edit |

On the **home screen only**, `F6` opens the app launcher. Inside an app, `ESC`
returns to the launcher rather than to the home screen — `HOME` still goes
straight home from anywhere, as it always has.

## Typed commands

Several screens have no key of their own and are opened by typing their name on
the home screen:

| Command | Opens |
|---------|-------|
| `help`, `?` | The built-in help browser |
| `lists`, `list` | Data list editor |
| `stats`, `stat` | Statistics |
| `dist` | Distribution helper |
| `test`, `infer` | Inference (tests, intervals, ANOVA) |
| `plot`, `plots` | Stat plot setup |
| `calc`, `analyze` | Graph analysis menu |
| `mat`, `matrix` | Matrix editor |
| `cas` | CAS menu |
| `apps`, `app` | App launcher (also `F6`) |
| `solve`, `solver` | Numeric equation solver |
| `const`, `constants` | Scientific constants |
| `settings`, `setup` | Power and brightness settings |
| `diag` | Hardware diagnostics |
| `files` | SD file browser |
| `py <statement>` | Runs one line of Python and shows what it printed. State persists between calls, so `py a=6*7` then `py print(a)` works |
| `mode`, `mode <setting>` | Mode settings — bare opens the screen, with an argument sets it directly |
| `cls` | Clear the screen, keeping input history |
| `clrhist` | Erase all history |

The on-device help lists a subset of these; the table above is the full set the
home screen accepts.

## Home

Type an expression, `ENTER` to evaluate.

| Key | Action |
|-----|--------|
| `UP` | Recall the last entry |
| `UP` / `DOWN` | Walk the input history |
| `Alt+UP` / `Alt+DOWN` (or `Ctrl+`) | Scroll the history *view* |
| `Alt+ENTER` | Decimal result; on an empty line, redo the last exact result as a decimal |
| `F6` (`Shift+F1`) | CAS menu |

Store with `2->a`. **Input is case-sensitive, and variables are lowercase** —
`a`–`z` plus `theta`, with `ans` holding the last result. `e` and `i` are
reserved (Euler's number and the imaginary unit); storing to either is refused
with a message saying so. In a number literal, `e` or `E` is exponent notation:
`1e10`.

Results with a clean closed form appear in amber rather than as a decimal —
`sqrt(8)` as `2√2`, `pi*2` as `2π`, `1/3` as a stacked fraction, `sin(pi/3)` as
`√3/2`. A trailing `>dec` also forces the decimal, and `>frac` asks for a
fraction (denominators up to 10000).

## Mode (`F3`)

Angle (RADIAN / DEGREE), display format (FLOAT / FIX / SCI / ENG) and fix
digits, **graph mode** (FUNC / PARAM / POLAR / SEQ), **number mode**
(REAL / `a+bi` / `r∠θ`), brightness, and reboot to the USB bootloader for
flashing.

`LEFT`/`RIGHT` change a value; `ENTER` selects, and activates the reboot row.

## Editors (`F1`)

`F1` opens whichever editor matches the active graph mode.

| Key | Action |
|-----|--------|
| `ENTER` | Edit the selected field |
| `SPACE` | Toggle the slot on or off |
| `DEL` | Clear the field |
| `F5` | Graph |

A field that will not parse is drawn in red.

The parametric editor shows six $X_{nT}/Y_{nT}$ pairs — committing an X
expression auto-focuses its empty partner. The polar editor shows
$r_1 \ldots r_6$ with `theta` typed out.

## Window (`F2`)

Mode-aware: adds `Tmin`/`Tmax`/`Tstep` in parametric mode and
`THmin`/`THmax`/`THstep` in polar mode, ahead of the shared x/y fields.
`ENTER` edits a value; `DEL` clears it and starts an empty edit.

## Graph (`F5`)

| Key | Action |
|-----|--------|
| `F4` TRC | Toggle trace |
| `LEFT` / `RIGHT` | Move the trace cursor |
| `UP` / `DOWN` | Next curve, while tracing |
| `-` / `=` | Zoom out / in |
| `S` / `T` | ZStandard / ZTrig |
| `D` / `Q` | ZDecimal / ZSquare |
| `B` | ZBox — pick two corners |
| `F` | ZoomFit — fit y to the curves |
| `Z` | ZoomStat — fit to stat plots |
| `H` | Shade between two expressions |
| `L` | Toggle axis labels |
| `F5` TBL | Value table |
| `Alt+F5` | Split graph \| table |
| `F6` CALC | Analysis menu |

The trace readout shows `x`/`y`, `t`, or `θ` depending on the mode.

**CALC** (`F6`) offers value, zero, minimum, maximum, intersect, `dy/dx` and
numeric integral. `ENTER` places bounds and points, `ESC` cancels. A root is
written back to `x` and to `ans`.

## Table (`F5` from the graph)

Columns adapt to the mode (`x|Y…`, `T|X1T Y1T…`, `th|r…`).

| Key | Action |
|-----|--------|
| `UP` / `DOWN` | Scroll rows |
| `LEFT` / `RIGHT` | Scroll columns |
| `ENTER` | Add a value (ASK mode) |
| `DEL` | Delete a row (ASK mode) |
| `F2` SETP | Table setup — start, step, AUTO/ASK |
| `Alt+F5` | Split graph \| table |

AUTO mode scrolls infinitely in both directions; ASK mode accumulates the
values you type.

## Split screen (`Alt+F5`)

Graph pane above, table below.

| Key | Action |
|-----|--------|
| `F5` | Switch the focused pane |
| `F4` | Trace, in the graph pane |
| `Alt+F5` or `ESC` | Back to full screen |

Trace position and table row stay in sync.

## List editor (`lists`)

| Key | Action |
|-----|--------|
| Arrows | Move between cells |
| `ENTER` or typing | Edit, or append a new row |
| `DEL` | Delete a row |
| `F6` / `F7` (`Shift+F1`/`F2`) | Sort ascending / descending |
| `F8` (`Shift+F3`) | Clear the list |

Six built-in lists $L_1 \ldots L_6$, plus named lists.

## Statistics (`stats`)

`UP`/`DOWN` select a row, `LEFT`/`RIGHT` change a value, `ENTER` on the last
row calculates. Scroll results with `UP`/`DOWN`.

## Distributions (`dist`)

`LEFT`/`RIGHT` pick the distribution and the function (PDF / CDF / inverse),
`ENTER` edits a parameter, `DEL` clears one. `ENTER` on the last row
calculates; the result goes to `ans` and the equivalent function call is shown,
so you can type it directly next time.

## Inference (`test`)

`LEFT`/`RIGHT` cycle the test and its options, `ENTER` edits a field or runs
the test. Where it applies, you choose a **Data** or **Stats** source.

## Stat plots (`plot`)

Three slots: scatter, xy-line, histogram, box, and normal probability. They
draw on the graph screen alongside your functions; `Z` there is ZoomStat.

## Matrices

Type `[A]` … `[J]` directly into a home-screen expression, or use the matrix
editor. Determinant, inverse, transpose, `rref`/`ref`, `rank`, `eigenvals`,
`eigenvec`, and a numeric `solve`.

## Complex numbers

Expressions like `3+2i` or `sqrt(-4)` evaluate according to the MODE screen's
Number row. In REAL mode a non-real result reports "Non-real result" rather
than `NaN`; `a+bi` and `r∠θ` display the complex value in that form.

## CAS (`F6` from home, or `cas`)

`simplify`, `expand`, `factor`, `diff`, `integ` and `solve` are also ordinary
functions you can call inline — `diff(x^2, x)` works without opening a menu.

## Apps (`F6`, or `apps`)

`F6` on the home screen opens the launcher. `ESC` from an app returns to the
launcher; `HOME` goes straight back to the calculator from anywhere.

| App | What it does |
|-----|--------------|
| **Notepad** | Plain-text notes under `/picocalc/notes/`. `F2` saves, `F3` loads, `F4` starts a new file |
| **Python** | Write and run MicroPython. Same editor keys as Notepad, plus `F1` RUN. Files live under `/picocalc/programs/` |
| **Files** | Browse the SD card. `ENTER` opens a directory, `F4` renames, `F5` makes a folder, `DEL` deletes |

Anything you put under `/picocalc/apps/` shows up here too — see
[your own apps on the SD card](#your-own-apps-on-the-sd-card).

### Python

`F1` saves the script and runs it, then shows what it printed. `ESC` returns to
the editor, `ESC` again leaves for the launcher; `UP`/`DOWN` scroll the output.
An error shows its traceback with the failing line number, and the view scrolls
to it.

`ENTER` after a line ending in `:` indents the next line, as you would expect.

**`ESC` stops a running script** — it raises `KeyboardInterrupt`, so a loop that
never ends is not a reason to power-cycle the machine.

If a script prints more than the output pane holds, the **oldest** lines are
dropped and the header says `(trimmed)`. That keeps the end of the output, which
is where a traceback is.

There is no `import` of your own modules yet, and `open()` raises — SD files
are reached through `calc.read_file`/`write_file` instead (below). `json` is
available.

For one-off expressions, `py <statement>` on the home screen runs a single line
without leaving the calculator.

### The `calc` module

`import calc` gives a script the calculator itself.

```python
import calc

calc.eval("2 + 3 * sin(pi/4)")   # 4.121320343559642
calc.eval("2+3i")                 # (2+3j), in a+bi mode
calc.eval("{1,2,3}+1")            # "{2,3,4}" — a list comes back as text

calc.store("a", 42)               # same A-Z variables the calculator uses,
calc.recall("a")                  #   and they survive a power cycle

calc.diff("x^3-2*x", "x")         # "3*x^2 - 2"
calc.integ("sin(x)", "x")         # "-1*cos(x)"
calc.integ("sin(x)", "x", 0, "pi")# 2.0 — bounds make it a number
calc.factor("x^2-4")              # "(x - 2)*(x + 2)"
calc.solve("x^2-4=0", "x")        # ['2', '-2']

calc.c_abs(complex(3, 4))         # 5.0
calc.c_arg(complex(0, 1))         # 1.5707... — always radians

calc.plot("x^2-4")                # writes Y1, returns 1
calc.window(-10, 10, -10, 10)
calc.show_graph()                 # shown when the script finishes

calc.graph_zero("Y1", 0, 5)       # (2.0, 0.0)
calc.graph_max("Y1", -5, 5)       # (x, y) of the maximum
calc.graph_integral("Y1", 0, 3)   # -3.0
calc.graph_deriv("Y1", 2)         # 4.0
calc.graph_value("Y1", 3)         # (3.0, 5.0)
```

### Lists and matrices

```python
calc.set_list(1, [2, 4, 4, 4, 5, 5, 7, 9])
calc.get_list(1)                  # [2.0, 4.0, ...]
calc.stat_mean(1)                 # 5.0 — also stat_sum/min/max/stddev
calc.list_append(1, 12.5)         # grows l1 by one

calc.det([[1, 2], [3, 4]])        # -2.0
calc.inverse([[1, 2], [3, 4]])    # [[-2.0, 1.0], [1.5, -0.5]]
calc.transpose(m); calc.rref(m)
calc.eigenvalues([[2, 1], [1, 2]])# [3.0, 1.0] — a flat list
calc.set_matrix("A", [[1, 2], [3, 4]])
calc.get_matrix("A")
```

Lists are the six real `l1`–`l6`, so anything a script writes shows up in the
list editor and in `stats`. Matrices are ordinary nested Python lists, except
for `set_matrix`/`get_matrix`, which read and write the calculator's own
`[A]`–`[J]`.

**Use `calc.list_append` for logging, not a Python list.** A loop that
collects a few hundred readings in a Python list will run the interpreter out
of memory; the same loop through `list_append` costs essentially nothing,
because the data lives where the calculator's lists live rather than in the
script's own memory.

**Lists and matrices are saved when the script finishes**, not on every call —
that is what keeps a logging loop fast. The trade is that a script you stop
with `ESC`, or one that fails, loses whatever it had not saved yet.

`calc.eigenvalues` on a large matrix needs more stack than most things, so
call it from the top level of your script rather than from inside a function.

**`calc.plot()` replaces your Y= functions.** The first `plot()` a script runs
clears all seven slots, and later calls fill Y2, Y3 and so on; an eighth is an
error. This is deliberate — it means a script draws what it asks for and not
whatever was left over — but it is **permanent**: graph state is saved, so
your own functions do not come back after a power cycle. Keep anything you
care about in a note before running someone else's plotting script.

Each `py` line at the home screen counts as its own script, so two separate
`py calc.plot(...)` lines leave one curve, not two. Put them on one line, or
in a real script, to get both.

A plot's colour is its slot's colour — Y1 blue, Y2 red, and so on, the same as
the Y= editor shows. `plot()` returns the slot it used.

`calc.eval` takes anything you could type on the home screen, and gives back a
number when the answer is one, a string when it is a list, a matrix or an
algebraic expression.

Three things to know:

- **Variable names are strict.** One lowercase letter, or `"theta"` or
  `"ans"`. `calc.store("A", 1)` is an error rather than quietly writing
  somewhere else.
- **Some answers depend on the mode.** `calc.solve("x^2+1=0","x")` finds no
  solution in REAL mode and returns `['i', '-1*i']` in a+bi. Angle mode
  applies to `calc.eval` the same way it applies to typing.
- **Call `calc` from the top of your script, not from deep inside it.** The
  calculator's evaluator needs more stack than Python does. A `calc.eval`
  works at the top level and inside one function; **inside two nested
  functions it raises `ValueError: Not enough stack`**, and `calc.eval` of a
  `solve(...)` is top-level only. Do the calculating up front and pass the
  answers down.
- **A loop that builds thousands of small strings can exhaust memory.**
  `calc.eval("sin(" + str(i) + ")")` makes three throwaway strings every
  time round; a few hundred iterations will fill the 40 KB Python heap. If
  that happens the interpreter resets itself and says so — your script's
  variables are lost, but the calculator keeps working and no power cycle is
  needed. Building the expression once outside the loop avoids it.

### Drawing, keys and files

```python
calc.clear_screen("blue")            # the script now owns the screen
calc.draw_rect(30, 50, 260, 140, "white", True)
calc.draw_rect(30, 50, 260, 140, "red")        # outline
calc.draw_line(30, 190, 290, 50, "green")
calc.draw_pixel(160, 160, (255, 140, 0))       # names or (r, g, b)
calc.draw_text(45, 70, "Hello", "black", "white")
calc.text_size("Hello")              # (40, 16)

k = calc.wait_key()                  # blocks; k["ch"], k["code"], k["shift"]…
calc.key_pressed()                   # None if nothing is waiting
calc.key_held("left")                # True while the key is down
name = calc.input("Your name? ")

calc.write_file("/picocalc/data.txt", "hello")
calc.append_file("/picocalc/data.txt", " again")
calc.read_file("/picocalc/data.txt")
calc.file_exists("/picocalc/data.txt")
```

**A script that draws owns the screen.** `clear_screen()` hands it over: your
drawing stays up when the script finishes, instead of the output pane
appearing. **`ESC` gives the screen back** to the editor — and `ESC` also stops
a running script, so a drawing loop that never ends is not a reason to
power-cycle.

**`draw_text` needs a background colour** (it defaults to black). The screen
cannot be read back, so text is drawn as filled character cells rather than
letters floating over whatever was there.

**A key event's `ch` covers the control keys too**: `ENTER` is `"\r"`,
`BACKSPACE` is `"\b"`, `TAB` is `"\t"` and `DEL` is `chr(127)`. Everything
else printable is itself, and anything with no character — the arrows, the
function keys — is `""`, so use `k["code"]` or `calc.key_held()` for those.

Colours are either a name — `black`, `white`, `blue`, `red`, `green`,
`yellow`, `cyan`, `magenta`, `orange`, `gray` — or an `(r, g, b)` triple.

`calc.read_file` reads the whole file into memory, so it is limited by the
Python heap; a few kilobytes is comfortable, a very large data file is not.

### Your own apps on the SD card

A directory under `/picocalc/apps/` with an `app.txt` in it becomes its own
tile in the launcher, next to Notepad and Python:

```
/picocalc/apps/hello/app.txt
/picocalc/apps/hello/main.py
```

`app.txt` is one `key=value` per line — `#` starts a comment, and spacing and
capitalisation of the key do not matter:

```
name=Hello
icon=H
entry=main.py
```

All three have defaults, so **an empty `app.txt` next to a `main.py` is a
working app**: `name` falls back to the directory's name, `entry` to
`main.py`. An `entry` may also be an absolute path, to share one script
between tiles.

Copy [`examples/apps/`](examples/apps/) onto a card to see both shapes: `hello`
draws and keeps the screen, `quadratic` asks for numbers, prints its answers
and plots the curve.

An app is the same interpreter the `RUN` key uses, on the same calculator —
variables it stores are still there on the home screen afterwards. Two things
differ:

- **`ESC` returns to the launcher**, not to the program editor. An app never
  touches the editor's buffer, so whatever you were writing there is safe.
- **The card is scanned at boot** and whenever it is re-mounted. Adding an app
  to a card that is already in the slot needs a reboot, or an eject and
  re-insert.

An app that will not start is skipped rather than breaking the launcher, and
says why over USB serial — usually an `entry` naming a file that is not there.
The `Files` app (`F5` makes a folder) can create the directories on-device if
you would rather not take the card out.
