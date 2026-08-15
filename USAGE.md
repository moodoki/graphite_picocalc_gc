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

There is no `import` of your own modules yet, and `open()` raises — file access
arrives with the `calc` module in a later release. `json` is available.

For one-off expressions, `py <statement>` on the home screen runs a single line
without leaving the calculator.
