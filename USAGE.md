# Using the calculator

A quick tour of the screens and their keys. The firmware also has a **built-in
help browser** — `F5` from the Home screen — with the function catalog,
per-screen key reference and syntax notes; that is the authoritative reference
and it ships with the build you are running.

For what the calculator can do, see [FEATURES.md](FEATURES.md). For building
and flashing, see the [README](README.md#quick-start-building-from-source).

## A note on modifier keys

The PicoCalc's keyboard is an STM32 co-processor that translates Shift chords
into their own scan codes rather than passing Shift through to the host — and
it swallows shifted arrows entirely (`Shift+Enter` arrives as `INS`). So every
binding in this firmware uses **Alt** or **Ctrl**, never Shift. Function keys
past `F5` are Shift chords the keyboard reports directly: `F6` = `Shift+F1`,
`F9` = `Shift+F4`.

## Home

Type an expression, `ENTER` to evaluate.

| Key | Action |
|-----|--------|
| `UP` / `DOWN` | Walk back/forward through past inputs (shell-style) |
| `Alt+UP` / `Alt+DOWN` (or `Ctrl+`) | Scroll the history *view* |
| `Alt+ENTER` | Show the last result as a decimal instead of an exact form; on an empty input line, re-run the last exact result as a decimal |
| `F1` … `F5` | Y= editor, window, graph, mode, help |
| `F6` (`Shift+F1`) | Hardware diagnostics (`F6` or `ESC` exits) |
| `HOME` | Return here from anywhere |

Store with `2->A`. Results with a clean closed form appear in amber
(`sqrt(8)` → `2√2`); a trailing `>dec` also forces the decimal.

Several subsystems are reachable by typing their name: `lists`/`list`,
`stats`/`stat`, `calc`/`analyze`, `cas`, `matrix`.

## Mode (`F4`)

Angle (RAD/DEG), display format (FLOAT/FIX/SCI) and fix digits, **graph mode**
(FUNC/PARAM/POLAR/SEQ), **number mode** (REAL/`a+bi`/`r∠θ`), brightness, and
reboot to the USB bootloader for flashing.

## Y= and the other editors

`F5` from the graph opens the active mode's editor.

| Key | Action |
|-----|--------|
| `UP` / `DOWN` | Select a slot |
| `ENTER` / `F1` | Edit |
| `F2` | Toggle enable |
| `F3` | Clear |
| `F4` | Graph |

The parametric editor shows six $X_{nT}/Y_{nT}$ pairs — committing an X
expression auto-focuses its empty partner. The polar editor shows
$r_1 \ldots r_6$ with `theta` typed out.

## Window (`F2`)

Mode-aware: adds `Tmin/Tmax/Tstep` in parametric mode and `THmin/THmax/THstep`
in polar mode, ahead of the shared x/y fields.

## Graph (`F3`)

| Key | Action |
|-----|--------|
| `F1` | Trace — `LEFT`/`RIGHT` move, `UP`/`DOWN` switch curve |
| `F2` / `F3` | Zoom in / out |
| `S` / `T` | Standard / trig presets |
| `F4` | Table |
| `F5` | Editor |
| `F6` | CALC menu |
| `F9` (`Shift+F4`) | Split-screen |

The trace readout shows `x/y`, `t`, or `θ` depending on the mode.

## Table (`F4` from the graph)

Columns adapt to the mode (`x|Y…`, `T|X1T Y1T…`, `th|r…`). Auto mode scrolls
infinitely in both directions; ask mode accumulates typed values (`ENTER` adds,
`F5` deletes).

| Key | Action |
|-----|--------|
| `LEFT` / `RIGHT` | Scroll columns |
| `F1` | Table setup (Start / Step / AUTO-ASK) |
| `F4` or `ESC` | Back to the graph |

## Split-screen (`F9` from graph or table)

Graph pane above, table below. `F4` switches the focused pane; trace and table
row stay in sync. `F9` or `ESC` exits.

## Lists and statistics (`lists` / `stats`)

A spreadsheet-style editor for the six lists $L_1 \ldots L_6$ and any named
lists, then 1-var/2-var descriptive stats, the ten regression models,
distributions (PDF/CDF/inverse), the inference suite, and stat plots
(histogram / box / scatter) drawn by the graph engine.

## Matrices

Type `[A]` … `[J]` directly into a home-screen expression, or open the matrix
editor. Arithmetic, determinant, inverse, transpose, row-echelon form,
`eigenvals` / `eig`, and a numeric equation solver.

## CALC menu (`F6` on the graph, or typed `calc`)

Value, zero, min/max, intersect, `dy/dx`, and numeric `fnInt` — cursor-driven
along the graphed curve, across function/parametric/polar modes.

## Complex numbers

Expressions like `3+2i` or `sqrt(-4)` evaluate according to the MODE screen's
Number row. In REAL mode a non-real result reports "Non-real result" rather
than `NaN`; `a+bi` and `r∠θ` modes display the complex value in that form.

## CAS (`F6` from Home, or typed `cas`)

`simplify`, `expand`, `factor`, `diff`, `integ` and `solve` are also ordinary
functions you can call inline from the home screen — `diff(x^2, x)` works
without opening a menu.
