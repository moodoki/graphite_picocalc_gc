# 15. Apps and programming

The calculator is not only a calculator. Since `v0.5.0` it has an **app
launcher**, and the things on it — Notepad, the file browser, the Python editor
— run on the same framework your own apps run on.

This chapter covers the apps that ship. [Chapter 16](16-micropython.md) covers
writing scripts.

## The launcher

Press **`F6`** on the home screen, or type `apps`.

From inside an app, **`ESC`** goes back to the launcher and **`HOME`** goes
straight back to the calculator from any depth. Those two keys are the whole
navigation model: `ESC` is "up one level", `HOME` is "out".

| App | What it is |
|---|---|
| **Notepad** | Plain-text notes, stored under `/picocalc/notes/` |
| **Python** | Write and run MicroPython, stored under `/picocalc/programs/` |
| **Files** | Browse and manage the SD card |

Anything you install under `/picocalc/apps/` appears on this list too — see
[Installing an app](#installing-an-app) below.

## The text editor

Notepad and the Python editor are the *same* editor, so the keys carry over.

| Key | Action |
|---|---|
| **`F2`** | Save |
| **`F3`** | Load |
| **`F4`** | New file |
| **`F1`** | Run (Python editor only) |

Lines are numbered. `ENTER` after a line ending in `:` indents the next line,
which matters more in the Python editor than in Notepad but works in both.

## Files

The file browser navigates the SD card and manages what is on it.

| Key | Action |
|---|---|
| **`UP`/`DOWN`** | Select an entry |
| **`ENTER`** | Open a folder, or open a file in its app |
| **`LEFT`** or **`ESC`** | Up one folder; `ESC` at the top leaves |
| **`HOME`** | Leave from any depth |
| **`F2`** | CUT — mark for moving |
| **`F3`** | MOVE — move it here |
| **`F4`** | REN — rename |
| **`F5`** | MKDIR — new folder |
| **`DEL`** | Delete (confirms; folders must be empty) |

Moving is a two-step: select a file, `F2` to mark it, navigate to where you
want it, `F3` to drop it there.

`ENTER` on a file opens it in the app that owns its type — a `.py` file opens
in the Python editor, a note in Notepad — so the browser doubles as the way to
reopen your own work.

## Running a script

In the Python editor, **`F1`** saves and runs. The output pane shows whatever
the script printed.

- **`ESC`** returns to the editor; `ESC` again leaves for the launcher.
- **`UP`/`DOWN`** scroll the output.
- An error shows its traceback with the failing line number, and the view
  scrolls to it.

If a script prints more than the pane holds, the **oldest** lines are dropped
and the header says `(trimmed)`. That is deliberate: it keeps the *end* of the
output, which is where a traceback is.

### Stopping a script

**`ESC` stops a running script.** It raises `KeyboardInterrupt`, so a loop that
never ends is not a reason to power-cycle the machine.

A script can ask to receive `ESC` as an ordinary key instead — see
[`calc.capture_esc`](16-micropython.md#taking-over-the-screen) — which is how
an app uses `ESC` for "back one level". Even then you are not locked out: **two
unread presses interrupt it regardless**, so a script that has stopped
responding is always killable.

### One-off lines

For a single statement you do not need the editor at all. On the home screen:

```
py 2 + 2
```

`py <statement>` runs one line and prints the result, without leaving the
calculator.

## Installing an app

A folder under `/picocalc/apps/` with a manifest becomes its own launcher tile.
Installing an app is copying a directory onto the SD card — there is no
registration step and no rebuild.

This is the same mechanism the shipped apps use, which is the point: an app you
write is not a second-class citizen on the list.

## What the calculator still does without a script

Not everything that looks like a programming problem needs one:

- **`seq`** applies an expression across a range and collects the results into
  a list, which handles most "do this for every value of $x$" tasks:

  ```
  seq(x^2,x,1,10,1)->l1
  ```

- **Element-wise list arithmetic** applies a formula to a whole dataset at once
  — see [Lists](10-lists.md).
- **Sequence graph mode** evaluates recurrences, including ones that refer to
  their own earlier terms. See
  [Parametric, polar and sequence](08-parametric-polar-sequence.md).
- **Regression Store-to** writes a fitted model into a Y= slot as a working
  expression — see [Statistics](11-statistics.md).
- **The CAS** does symbolic manipulation directly.

Reach for a script when you need something these cannot express — a user
interface, a loop with state, a dataset read from a file — rather than as the
default way to do arithmetic.
