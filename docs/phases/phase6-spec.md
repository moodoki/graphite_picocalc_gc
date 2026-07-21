# Phase 6 Spec: Non-Calculator Functions

**Prerequisite phases**: Phase 4 (pre-release milestone — full TI-83/84+-class
GC functionality). Phase 5 (CAS) is not a hard prerequisite for Phase 6 as
a whole, but sub-phase 6B's `calc` module wants it for the `diff`/`integ`/
`solve` bindings — per the intended phase order (4 → 5 → 6), Phase 5 will
already exist by the time 6B is implemented.

**Scope**: everything that isn't graphing-calculator functionality. Where
Phases 1–4 built out a complete TI-83/84+-class calculator and Phase 5
adds symbolic math on top of it, Phase 6 is about turning the device into
a platform that can run things that aren't calculator features at all —
starting with user-written MicroPython programs. **Phase 6's sub-phases
are deliberately scoped to be completable in any order** (mod the one
structural dependency noted in §1.1) — this phase exists specifically so
that kind of open-ended, non-serialized work has a home instead of forcing
a strict week-by-week sequence the way Phases 1–5 do.

**End state**: the calculator has an app launcher distinct from its fixed
calculator screens; MicroPython programs can be written, saved, and run
on-device with access to the calculator's math/graph/matrix/complex/CAS
functions; and the platform is positioned to add future non-calculator
apps without needing a new phase carved out each time.

**Status**: Specced, not started. This document replaces what was Phase 4
sub-phase 4E (MicroPython) — see
[decisions.md](../notes/decisions.md) D33 for why it moved here rather
than staying in Phase 4, and [phase4-spec.md](phase4-spec.md) for the
sub-phase it vacated.

---

## 1. Overview and phasing within Phase 6

| Sub-phase | Content | Depends on |
|---|---|---|
| 6A: App framework | Launcher screen, app registration/lifecycle, screen-ownership handoff | Phase 4 |
| 6B: MicroPython (first base app) | Embedded interpreter, `calc` module bindings, program editor, SD card scripts | 6A |
| 6C+: future apps (unscoped) | Candidates in §9 — additional apps riding the 6A framework | 6A |
| Release engineering (unscoped, can run in parallel) | Docs site ([docs-site-plan.md](../notes/docs-site-plan.md)), versioned firmware releases | none |

Unlike Phases 1–5's strictly ordered sub-phases, once 6A exists, 6B and
any future 6C+ app have no dependency on each other — pick whichever is
most wanted next, or work on more than one across sessions without
worrying about blocking order. Release engineering doesn't even need 6A;
it could start any time (see §9).

### 1.1 Why an app framework at all, and why before MicroPython

The original phase4-spec (as 4E) scoped MicroPython as a single fixed
"program editor screen" — one more screen bolted directly onto the
existing screen manager, the same way the matrix editor or stats screen
are. That's a reasonable design if MicroPython is the *only*
non-calculator thing this platform will ever do. But scoping this as its
own phase, with room for "ideally sub-phases that can be done in any
order," implies more than one such feature is expected eventually.
Building a small app-launcher/registration layer first (6A) means
MicroPython becomes the **first instance of an app pattern** rather than
a one-off special case — so a second non-calculator feature later slots
into the same launcher instead of needing its own bespoke integration
into `apps/nav.{hpp,cpp}`.

6A is deliberately scoped small. This is **not** TI's Apps ecosystem —
sideloadable binaries, a computer-link install flow, an app store (see
the [parity doc](../notes/ti-parity-2026-07-21.md)'s "deliberately not
chasing TI" list, which already rules this out). It's an in-firmware
launcher screen plus a lightweight registration table: closer to "a
second menu of screens, entered from Home" than a real OS with
dynamically loaded code. Every app in scope here ships compiled into the
same firmware image as everything else.

---

## 2. New source files

```
src/
├── apps/
│   ├── launcher_screen.hpp / .cpp  # NEW (6A): app launcher/menu screen
│   └── program_screen.hpp / .cpp   # NEW (6B): MicroPython editor + runner
│                                     #   (moved here from the old 4E plan)
├── platform/
│   └── app_registry.hpp / .cpp     # NEW (6A): app registration table + lifecycle
└── scripting/
    ├── micropython_embed.hpp/.cpp  # NEW (6B): MicroPython interpreter wrapper
    ├── calc_module.hpp / .cpp      # NEW (6B): Python 'calc' module (C++ bindings)
    └── script_runner.hpp / .cpp    # NEW (6B): load + execute .py from SD card
```

---

## 3. Sub-phase 6A: App framework

### 3.1 App registry

```cpp
namespace platform {

// One entry per launchable app. Apps are statically registered at boot
// (compiled-in, not dynamically loaded — see §1.1).
struct AppEntry {
    const char* name;          // "MicroPython", shown in the launcher
    const char* icon_glyph;    // optional single-glyph icon (font slot map)
    void (*launch)();          // hands control to ui::ScreenManager
};

class AppRegistry {
public:
    static void register_app(const AppEntry& entry);
    static int count();
    static const AppEntry* get(int index);
};

}  // namespace platform
```

Registration happens once at boot (`main.cpp`, alongside the existing
catalog/screen-manager setup) — each app's translation unit calls
`AppRegistry::register_app` for itself, the same pattern the function
catalog already uses to stay a single source of truth (README: *"function
catalog driven by the same table the parser registers from"*).

### 3.2 Launcher screen

Entered from Home via a new softkey or typed command (`apps`/`app`,
mirroring the `lists`/`stats` typed-command convention from Phase 3).
`UP`/`DOWN` selects, `ENTER` launches. This is intentionally the
simplest possible menu — a vertical list, no icons grid, no categories —
because with one app (MicroPython) at launch there's nothing to organize
yet. Revisit the layout once a second or third app exists (§9).

### 3.3 Screen-ownership handoff

Apps take over the full screen the same way any existing full-screen
`apps/*` module does (matrix editor, stats screen, etc.) — there's no
new windowing/compositing model. The only new behavior is *how you get
there*: through the launcher rather than a fixed softkey, and *how you
leave*: a consistent "exit to launcher" convention (e.g. `ESC` returns to
the launcher, not directly to Home, so a user who came from the launcher
doesn't lose their place). `HOME` still short-circuits to the home
screen from anywhere, matching every other screen's existing behavior.

---

## 4. Sub-phase 6B: MicroPython programming (first base app)

Unchanged in substance from the original 4E plan — only the entry point
changes (launched via 6A's launcher rather than a direct Home softkey)
and the framing (this is now explicitly *an app*, not a fixed calculator
mode).

### 4.1 Embedding strategy

MicroPython provides an `embed` port specifically designed for hosting
MicroPython inside a larger C/C++ application. The firmware includes the
MicroPython interpreter as a library, not a standalone runtime.

```cpp
namespace scripting {

class PythonInterpreter {
public:
    // Initialize the MicroPython runtime.
    // heap_size: bytes allocated for the Python heap.
    //   Pico 1: 48 KB from SRAM (leaves ~80KB for app + stack)
    //   Pico 2: 96 KB from SRAM
    // PSRAM is NOT directly usable as Python heap (too slow for
    // GC scanning), but individual large allocations can be proxied.
    bool init(size_t heap_size);

    void shutdown();

    // Execute a Python string (single statement or block).
    // Returns true if execution succeeded.
    // Output is captured and passed to output_callback.
    bool exec(const char* code);

    // Execute a .py file from the SD card.
    bool exec_file(const char* path);

    // Register a C function as a Python built-in.
    // Used to expose calculator APIs to scripts.
    void register_function(const char* module, const char* name,
                           void* c_func);

    // Check if interpreter is initialized.
    bool is_running() const;

    // Set callback for print() output.
    using OutputCallback = void (*)(const char* text);
    void set_output_callback(OutputCallback cb);

    // Set callback for input() requests.
    using InputCallback = const char* (*)(const char* prompt);
    void set_input_callback(InputCallback cb);

private:
    bool initialized_ = false;
};

} // namespace scripting
```

### 4.2 Calculator API bindings (`calc` Python module)

User scripts import a `calc` module that exposes calculator functionality:

```python
import calc

# Expression evaluation
result = calc.eval("2 + 3 * sin(pi/4)")

# Variables
calc.store("A", 42)
val = calc.recall("A")

# Graphing
calc.plot("sin(x)", color="blue")
calc.plot("cos(x)", color="red")
calc.window(-10, 10, -2, 2)
calc.show_graph()

# CAS operations (Phase 5 — see prerequisite note at top of this doc)
d = calc.diff("x^3 - 2*x", "x")    # Returns "3*x^2 - 2"
i = calc.integ("sin(x)", "x")       # Returns "-cos(x)"
s = calc.solve("x^2 - 4 = 0", "x") # Returns ["-2", "2"]
s2 = calc.solve("x^2 + 1 = 0", "x")# Returns ["i", "-i"] (complex-aware)
f = calc.factor("x^2 - 4")          # Returns "(x-2)*(x+2)"

# Complex numbers
z = calc.complex(3, 2)              # 3 + 2i
mag = calc.c_abs(z)                 # 3.606...
ang = calc.c_arg(z)                 # 0.588...
cj = calc.c_conj(z)                 # 3 - 2i

# Graph analysis (numeric)
root = calc.graph_zero("Y1", -5, 5)      # numeric root in bracket
mx   = calc.graph_max("Y1", 0, 10)       # local maximum
area = calc.graph_integral("Y1", 0, 3.14)# numeric definite integral
slope= calc.graph_deriv("Y1", 2.0)       # numeric dy/dx at x=2

# Matrix operations
m = calc.matrix([[1, 2], [3, 4]])
det = calc.det(m)
inv = calc.inverse(m)

# Lists (from Phase 3)
calc.set_list(1, [1, 2, 3, 4, 5])
mean = calc.stat_mean(1)

# Display
calc.clear_screen()
calc.draw_text(10, 10, "Hello from Python!")
calc.draw_line(0, 0, 319, 319, "white")
calc.draw_rect(50, 50, 100, 80, "blue", fill=True)

# Input
key = calc.wait_key()
text = calc.input("Enter value: ")

# File I/O (SD card)
calc.write_file("/picocalc/data.txt", "hello")
content = calc.read_file("/picocalc/data.txt")
```

Each `calc.*` function is a thin C++ wrapper that calls into the existing
`math::Engine`, `math::cas::*` (Phase 5), `math::Matrix`,
`platform::Display`, `platform::Keyboard`, and `platform::Storage`
classes.

### 4.3 Program editor screen

A simple on-device text editor for writing and editing Python scripts,
now launched from 6A's app launcher rather than a Home softkey:

```
┌──────────────────────────────────┐
│  Edit: program.py                 │
├──────────────────────────────────┤
│  1│ import calc                    │
│  2│                                │
│  3│ for i in range(10):            │
│  4│   y = calc.eval(f"sin({i})")   │
│  5│   calc.draw_text(10, i*20, str │
│  6│ |                              │
│  7│                                │
│                                    │
├──────────────────────────────────┤
│ F1:RUN F2:SAVE F3:LOAD F4:NEW    │
└──────────────────────────────────┘
```

**Features**:

- Line-numbered display with cursor (row + column)
- Arrow key navigation, `ENTER` inserts newline
- `BACKSPACE` / `DEL` work as expected
- Auto-indent on `ENTER` after `:` (match Python expectations)
- `F1` (RUN): save current buffer, execute via `PythonInterpreter::exec_file()`
- `F2` (SAVE): write to `/picocalc/programs/<name>.py`
- `F3` (LOAD): file browser for `/picocalc/programs/`, select a `.py` file
- `F4` (NEW): clear buffer, prompt for filename
- Syntax highlighting is a stretch goal — basic keyword coloring (`import`, `def`, `for`, `if`, `while`, `return`) if time permits

**Output capture**: when a script runs, `print()` output goes to a
scrollable output pane that replaces the editor temporarily. Press
`ESCAPE` to return to the editor (per 6A's convention, `ESCAPE` from the
top-level program screen returns to the launcher). Errors display with
the line number and exception message.

### 4.4 Memory budget for MicroPython

| Component | Pico 1 (SRAM) | Pico 2 (SRAM) |
|-----------|---------------|---------------|
| MicroPython interpreter + stdlib | ~60 KB flash | ~60 KB flash |
| Python heap (GC-managed) | 48 KB | 96 KB |
| C stack for Python calls | 8 KB | 8 KB |
| **Total SRAM impact** | **~56 KB** | **~104 KB** |

On Pico 1, this leaves ~80 KB SRAM for the rest of the application (HAL,
UI, math engine, line buffers). This is tight but workable because the
CAS pool lives in PSRAM and the framebuffer uses line-buffer rendering.
The MicroPython interpreter is only initialized when the user enters the
program screen — it doesn't consume memory when doing normal calculator
work. **Re-verify this budget against whatever 4D's list-cap increase and
complex-storage widening (phase4-spec.md §7.3) actually cost** — both
land before Phase 6 in the intended order and both touch SRAM.

On Pico 2, it's comfortable — 520 KB SRAM minus ~104 KB for Python minus
~200 KB for framebuffer still leaves ~216 KB.

---

## 5. Task breakdown

Solo developer, part-time (~20 hrs/week).

### Sub-phase 6A: App framework

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 6A.1 | `AppRegistry` + static registration pattern | 3 | Apps self-register at boot, `count()`/`get()` work |
| 6A.2 | Launcher screen (list, select, launch) | 5 | Launch a stub app from the launcher |
| 6A.3 | Screen-ownership handoff + exit-to-launcher convention | 4 | `ESC` from an app returns to launcher, not Home |
| 6A.4 | Home-screen entry point (softkey or typed command) | 2 | `apps`/`app` command opens the launcher |
| | **Subtotal** | **~14 hrs** | |

### Sub-phase 6B: MicroPython programming (first base app)

| # | Task | Est. hrs | Acceptance |
|---|------|---|---|
| 6B.1 | Build MicroPython embed lib (both boards) | 8 | `print(1+1)` → "2" on serial |
| 6B.2 | `PythonInterpreter` wrapper | 4 | Init/exec/shutdown clean |
| 6B.3 | `calc` module: eval, variables, store/recall | 6 | `calc.eval("sin(pi/4)")` correct |
| 6B.4 | `calc` module: CAS bindings (incl. complex solve) | 4 | `calc.solve("x^2+1=0","x")` → `["i","-i"]` |
| 6B.5 | `calc` module: complex bindings | 3 | `calc.c_abs(calc.complex(3,4))` = 5 |
| 6B.6 | `calc` module: graph-analysis bindings | 4 | `calc.graph_zero`, `graph_integral` work |
| 6B.7 | `calc` module: matrix bindings | 3 | Create/multiply/invert from Python |
| 6B.8 | `calc` module: display primitives | 4 | Script draws graphics |
| 6B.9 | `calc` module: keyboard input | 3 | Read keys, text input |
| 6B.10 | `calc` module: file I/O | 2 | Read/write SD files |
| 6B.11 | Program editor screen, registered as a 6A app | 10 | Write a 20-line script on-device |
| 6B.12 | Execution: output capture, error display | 4 | print output + line-numbered errors |
| 6B.13 | Load/save scripts to SD | 3 | Save, power cycle, reload, run |
| 6B.14 | Memory management: lazy init, cleanup | 3 | Heap freed on leaving program screen |
| | **Subtotal** | **~61 hrs** | |

### Summary

| Sub-phase | Hours | Deliverable |
|-----------|-------|-------------|
| 6A: App framework | ~14 | Launcher screen, app registry, screen-handoff convention |
| 6B: MicroPython | ~61 | Interpreter, `calc` module, editor, SD scripts — first app on 6A |
| **Total (scoped so far)** | **~75 hrs** | |
| *6C+ future apps, release engineering* | *unscoped* | *see §9 — no estimate until something is actually picked up* |

---

## 6. Performance expectations

### App framework (6A)

Negligible — a static registration table and a list-menu screen, the
same cost class as any other menu screen already in the app. No new
performance-sensitive path.

### MicroPython (6B)

Unchanged from the original 4E estimate: interpreter init/exec cost is
dominated by MicroPython's own bytecode VM, not anything this firmware
adds. The one guardrail worth repeating — the Python heap is only
allocated when the program screen is entered (§4.4), so MicroPython has
zero steady-state cost during normal calculator use, graphing included.

---

## 7. Risks and mitigations

*(Risks 6 and 7 below were originally numbered this way in phase4-spec.md
before MicroPython moved here — numbering kept for continuity with any
existing cross-references.)*

### Risk 6: MicroPython heap too small on Pico 1

48 KB Python heap limits script complexity. **Mitigation**: document the
limit; store large data in `calc`-module lists/matrices (PSRAM, outside
the Python heap); Pico 2 doubles the heap.

### Risk 7: Building MicroPython for both RP2040 and RP2350

**Mitigation**: MicroPython officially supports both. Build the embed lib
as a separate CMake external project per board target. Test the embed
build early (6B.1) before writing bindings.

### Risk 10: App framework becomes over-engineered for one app

With only MicroPython as a concrete consumer at launch, 6A risks being
speculatively generalized for apps that don't exist yet. **Mitigation**:
keep 6A to exactly what §3 specs — a static table, a list menu, and an
exit convention. No dynamic loading, no per-app permission model, no
icon grid. Expand only when a second app (§9) actually needs more.

### Risk 11: `calc` module CAS bindings ship before Phase 5 is stable

If Phase 6 starts before Phase 5 (CAS) has fully landed, `calc.diff`/
`calc.integ`/`calc.solve` (§4.2) have nothing to bind to yet.
**Mitigation**: per the intended phase order (4 → 5 → 6) this shouldn't
happen, but if 6B ever gets picked up out of order, ship the non-CAS
`calc` bindings (eval, graphing, matrices, complex, display, I/O) first
and add the CAS bindings as a follow-up task once Phase 5 exists — the
module is additive, nothing else in 6B depends on the CAS bindings being
present.

---

## 8. Open questions

*(P6-1/P6-2 below carry the same content as phase4-spec.md's former
P4-4/P4-5, renumbered into this document.)*

| # | Question | Options | When |
|---|----------|---------|------|
| P6-1 | Python heap: static at boot or lazy on first use? | Lazy saves ~56 KB when unused | 6B implementation |
| P6-2 | `calc.plot()` from Python: immediate graph switch or buffered? | Immediate vs. buffered | 6B implementation |
| P6-3 | Launcher entry point: dedicated Home softkey, or typed-command-only like `lists`/`stats`? | Softkey is more discoverable; typed-only matches the Phase 3 precedent and doesn't consume a scarce F-key slot | 6A implementation |
| P6-4 | Does leaving an app via `HOME` (not `ESC`) skip the launcher entirely, or route through it? | Direct-to-Home matches every other screen's existing `HOME` behavior; routing through the launcher is more consistent but adds a hop | 6A implementation |

---

## 9. Candidate future sub-phases (unscoped)

Deliberately left open rather than pre-scoped — the point of Phase 6's
structure is that these can be picked up whenever, in whatever order,
without a spec rewrite each time. Listed here so they're not lost, not
because any of them is committed:

- **Additional built-in apps**, TI-Apps-style: a finance/TVM solver, a
  periodic table reference, a probability simulator. On this platform
  the general answer to "TI ships this as an app" is usually "write it
  in MicroPython" (6B) rather than a new C++ app — but a few
  high-value ones might warrant a native app for speed/polish reasons.
  Evaluate case by case.
- **Desktop emulator build target**: a third build target (alongside
  Pico 1/Pico 2) that runs the UI on a development machine. Flagged
  independently in the [wishlist](../notes/wishlist.md) (font
  antialiasing testing, D31) and the
  [docs-site-plan.md](../notes/docs-site-plan.md) (screenshot tooling)
  — both would benefit, but neither depends on 6A/6B, so this could
  start any time. It's dev/user tooling more than a user-facing "app,"
  so it sits loosely in this phase rather than squarely in it.
- **Release engineering**: standing up the [docs site
  plan](../notes/docs-site-plan.md) for real, and a versioned firmware
  release process (tagged builds, changelog). Doesn't depend on 6A/6B
  either — could run in parallel with any of the above, or even before
  Phase 6 starts in earnest.

---

## 10. References

1. Phase 4 spec (prerequisite — GC completeness milestone) — [phase4-spec.md](phase4-spec.md)
2. Phase 5 spec (CAS — soft dependency for 6B's `calc` module) — [phase5-spec.md](phase5-spec.md)
3. TI parity stocktake ("deliberately not chasing TI" — why 6A stays small) — [ti-parity-2026-07-21.md](../notes/ti-parity-2026-07-21.md)
4. Docs site plan (release-engineering candidate, §9) — [docs-site-plan.md](../notes/docs-site-plan.md)
5. MicroPython embed port — https://docs.micropython.org/en/latest/develop/embed.html
6. MicroPython RP2040/RP2350 support — https://micropython.org/download/RPI_PICO/
