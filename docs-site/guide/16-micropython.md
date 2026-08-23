# 16. Writing MicroPython scripts

## What this is for

**MicroPython on this calculator is not a general-purpose Python, and is not
trying to be one.** It is a way to *drive the calculator that is already
there* — and, just as importantly, a way to put an interface on top of it.

The division is deliberate:

> **The firmware owns the maths. Python owns the shape around it.**
>
> Arithmetic, the CAS, graphing, statistics, matrices, complex numbers — reach
> those through the **`calc` module**, because that is the same engine the
> calculator itself uses. Use Python for the parts a calculator has no way to
> express: loops with state, a menu, a prompt, a drawing, a dataset read from a
> file, a program you can hand to someone else.

Read the rest of this chapter with that split in mind. Nearly every rule below
follows from it, and so does nearly every limitation.

The split is about *meaning*, not about routing every arithmetic operation
through the firmware — crossing into `calc` has a cost, and
[the section below](#calceval-is-not-a-cheap-function-call) says where that
stops being worth paying.

## The two maths, and why you want ours

A `math` module exists. It is not the one you want, and the difference is not
stylistic. Measured on the device:

```python
import calc, math

# with the calculator in DEG mode
calc.eval("sin(90)")    # 1.0
math.sin(90)            # 0.8939966636005579
```

Both are correct. They answer different questions. `math.sin` is radians-only
and knows nothing about the machine it is running on; **`calc.eval` runs the
same four-stage pipeline the home screen runs**, so it respects the MODE
screen's DEG/RAD setting, the Number mode, and the display format — and its
results land in the same variables you can then use from the keyboard.

In RAD mode the two agree, which is exactly what makes this trap quiet: a
script tested in RAD works, and then someone flips the calculator to DEG and it
silently answers a different question.

**So: when the calculator's answer is the one you want, ask the calculator.**
Use `math` for things the calculator has no notion of — and for the case in the
next section.

### `calc.eval` is not a cheap function call

The two are built very differently, and it shows up in a loop.

`math.sin(x)` is a thin wrapper straight onto the C library: unwrap the float,
call `sin()`, wrap the result. `calc.eval("sin(90)")` **parses a string every
time it is called** and runs it through the whole home-screen pipeline —
symbolic check, `solve()` and `convert()` substitution, then a compile to RPN
and a run on the calculator's stack machine.

That is exactly what you want when the mode, the variables and `Ans` should all
apply. It is a poor way to add up ten thousand numbers.

A practical split:

- **Reaching for the calculator's meaning** — mode-aware trig, the CAS,
  anything involving stored variables or `Ans`, anything a user typed — use
  `calc.eval`.
- **Arithmetic inside your own loop**, where the calculator's semantics are not
  in play — use plain Python operators, or `math`.

```python
import calc, math

# Fine: one call, and mode matters.
angle = calc.eval("asin(0.5)")

# Wasteful: parses and compiles the same string 1000 times.
total = 0
for i in range(1000):
    total += calc.eval("sin(" + str(i) + ")")

# Better: settle the units once, then loop in Python.
# NOTE: this assumes the calculator is in DEG. See the warning below.
step = calc.eval("pi/180")      # 0.017453292519943296
total = 0
for i in range(1000):
    total += math.sin(i * step)
```

Note what the rewrite does: it still gets the *conversion* from the calculator,
then does the repetitive part in Python. That is usually the shape to aim for —
let `calc` decide what the numbers mean, and let Python grind through them.

> **Hoisting a `calc` call out of a loop means taking responsibility for what
> it meant.** Those two loops agree in DEG and disagree completely in RAD,
> because the first asks for `sin` of *i in whatever unit the calculator is
> set to* and the second hard-codes degrees. Measured on the device with
> `i = 30`: in DEG both give `0.49999999999999992`; in RAD the `calc.eval`
> version gives `-0.9880316240928618`.
>
> That is the same trap as the opening of this chapter, arriving from the other
> direction. If you optimise a loop this way, either fix the unit deliberately
> — as the comment above does — or read the mode first and branch on it.

This is a structural difference, not a measured ratio: no timing figure is
quoted here because the device has no `time` module to measure one with. Treat
it as "one is a function call and one is a compiler", which is enough to decide
where to put a loop.

## What is actually available

Verified on hardware, `v0.5.0`:

| Module | Available | Notes |
|---|---|---|
| `calc` | **yes** | The calculator. 54 functions — the rest of this chapter |
| `math` | yes | Radians-only, calculator-unaware. See above |
| `json` | yes | Useful for app config and saved state |
| `gc` | yes | `gc.mem_free()`, `gc.collect()` |
| `array` | yes | |
| `sys` | **no** | Compiled out |
| `os`, `time`, `random`, `cmath` | **no** | Not built at this feature level |

There is **no `time` module**, so there is no `time.sleep()`. A script that
wants to wait should wait on input with `calc.wait_key()` rather than spin.

Floats are **doubles**, not single-precision.

### Files, and the missing `open()`

`open()` exists but raises `NotImplementedError: no filesystem; use calc file
I/O`. That is on purpose — it tells you why, instead of failing as a
`NameError`. There is no filesystem behind Python here; the SD card is reached
through `calc`:

```python
calc.write_file("/picocalc/data.txt", "hello")
calc.append_file("/picocalc/data.txt", " again")
calc.read_file("/picocalc/data.txt")      # 'hello again'
calc.file_exists("/picocalc/data.txt")    # True
calc.list_files("/picocalc")              # list of entries
```

You also cannot yet `import` your own modules. A script is one file.

## The `calc` module

### Evaluating, and the calculator's own variables

```python
import calc

calc.eval("2 + 3 * sin(pi/4)")   # 4.121320343559642
calc.store("a", 42)              # the same A-Z variables the keyboard uses,
calc.recall("a")                 #   and they survive a power cycle
```

`calc.eval` returns **a float, a Python complex, or a string, depending on what
kind of answer it is**:

```python
calc.eval("2+2")           # 4.0          — a float
calc.eval("2+3i")          # (2+3j)       — a complex, in a+bi mode
calc.eval("{1,2,3}+1")     # '{2,3,4}'    — a list comes back as text
```

Check the type when the input is not under your control.

### The CAS

Symbolic operations take strings and return strings, because that is what a
symbolic answer is:

```python
calc.simplify("x+x")               # '2*x'
calc.expand("(x+1)^2")             # 'x^2 + 2*x + 1'
calc.factor("x^2-4")               # '(x - 2)*(x + 2)'
calc.diff("x^3-2*x", "x")          # '3*x^2 - 2'
calc.integ("sin(x)", "x")          # '-1*cos(x)'
calc.integ("sin(x)", "x", 0, "pi") # 2.0 — bounds make it a number
calc.solve("x^2-4=0", "x")         # ['2', '-2']
```

### Graphing

```python
calc.plot("x^2-4")            # writes a Y= slot, returns its number
calc.window(-10, 10, -10, 10)
calc.show_graph()             # displayed when the script finishes

calc.graph_zero("Y1", 0, 5)     # (2.0, 0.0)
calc.graph_min("Y1", -5, 5)
calc.graph_max("Y1", -5, 5)
calc.graph_integral("Y1", 0, 3)
calc.graph_deriv("Y1", 2)       # 4.0
calc.graph_value("Y1", 2)
```

> **`calc.plot` overwrites the user's own Y= functions, and the loss is
> saved.** This is a real cost, not an oversight — a script that plots is using
> the same six slots the person at the keyboard uses. If your app plots, say so
> in its description.

### Lists, statistics, matrices

The calculator's six lists and ten matrices are reachable directly:

```python
calc.set_list(1, [1, 2, 3])
calc.get_list(1)                 # [1.0, 2.0, 3.0]
calc.list_append(1, 4)

calc.stat_mean(1), calc.stat_sum(1), calc.stat_stddev(1)
calc.stat_min(1), calc.stat_max(1)

calc.set_matrix("A", [[1, 2], [3, 4]])
calc.det("A"), calc.inverse("A"), calc.transpose("A")
calc.rref("A"), calc.eigenvalues("A"), calc.matmul("A", "B")
```

**Use these instead of Python lists for anything large.** This is the single
biggest memory lesson from building the module, and it was measured: 400
`calc.list_append` calls cost **16 bytes** of Python heap, because the data
lives in the calculator's own storage. The same loop building a Python list
exhausted the whole 40 KB heap.

### Drawing, keys, and input

This is the half of the module that exists so scripts can have a *user
interface* — the "GUI over the calculator's functions" this whole facility is
for.

```python
calc.clear_screen()
calc.draw_pixel(x, y)
calc.draw_line(x0, y0, x1, y1)
calc.draw_rect(x, y, w, h)
calc.draw_text(x, y, "hello")
calc.text_size("hello")        # (width, height) in pixels

calc.key_pressed()             # None if nothing is waiting
calc.wait_key()                # blocks
calc.key_held(k)
calc.input("Name?")            # a prompt, returns what was typed
```

Most drawing calls take an optional trailing colour argument.

### Taking over the screen

Drawing **takes the panel until the script ends**. While your script owns the
screen, the calculator's own display is not being updated — this is why a
drawing script should be a script you can exit.

By default **`ESC` stops the script**. An app that wants `ESC` for "back one
level" asks for it:

```python
calc.capture_esc(True)
```

Now `ESC` arrives as an ordinary key from `wait_key()`, and it is your job to
act on it. **You cannot lock the user out**: two unread presses interrupt the
script regardless of this setting. Design for that rather than against it —
it means a hung script is always recoverable, including yours.

## Memory

The Python heap is **40 KB on the Pico 1** and **96 KB on the Pico 2**. It is a
fixed, statically reserved region — not "whatever is left".

```python
import gc
gc.mem_free()      # 39872 on a Pico 1, on a freshly started interpreter
```

Two things worth knowing:

- **An absolute `mem_free()` reading only means something on a clean
  interpreter.** If you have been experimenting, your own leftover globals are
  still live and the number is pessimistic. To measure what something costs,
  take the *difference* across it, not the absolute value after it.
- **Keep bulk data in the calculator, not in Python** — see the
  `list_append` measurement above. This is what the 40 KB is for: your code and
  its working state, not your dataset.

## A first script

```python
import calc

calc.clear_screen()
calc.draw_text(10, 10, "Roots of x^2 - 4")

roots = calc.solve("x^2-4=0", "x")
y = 40
for r in roots:
    calc.draw_text(10, y, "x = " + r)
    y += 20

calc.draw_text(10, y + 20, "Press any key")
calc.wait_key()
```

Note what each half is doing. The CAS solved the equation; Python decided where
the answers go on the screen and when to wait. That is the division this
chapter opened with, and it is the shape most scripts here should take.

## Where to go next

- [Chapter 15](15-programming.md) — the editor, running scripts, and installing
  an app under `/picocalc/apps/`
- [Key reference](../reference/key-reference.md) — generated from the firmware,
  so it cannot drift
