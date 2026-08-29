# The host build

A third target that runs the shared `src/` tree as a native application on
a development machine — Linux and macOS both. Phase 6.4; see
[`docs/phases/phase6.4-spec.md`](phases/phase6.4-spec.md) for the reasoning
and [`docs/notes/decisions.md`](notes/decisions.md) D92–D98 for the
decisions.

```sh
cmake -B build/host -S host
cmake --build build/host
./build/host/graphite-shot --shot home.ppm
```

That is the whole of it. No SDK, no cross toolchain, and no packages
beyond CMake and a C++17 compiler — the renderer writes PPM precisely so
it needs no image library.

## What it is for

**We could not see the screen.** The firmware could be flashed, driven over
serial, and made to read and write the SD card, and still nobody could look
at a visual fix. Two open bugs were visual by nature, and one of them —
[#52](https://github.com/moodoki/graphite_picocalc_gc/issues/52) — is
invisible to the `check-text-fits.py` lint gate *by design*, because
`draw_softkeys` truncates rather than overflowing.

It also draws the documentation screenshots
([#33](https://github.com/moodoki/graphite_picocalc_gc/issues/33)); see
[`docs-site/images/`](../docs-site/images/).

## What it deliberately does not model

**A green host build is not a hardware verification, and never substitutes
for one.** Every phase in this repo is Complete only when hardware-verified
on both boards, and 6.4 does not change that rule for any other phase.

- **No SRAM ceiling.** The host has a process address space. The Pico 1's
  15.2 KB of free SRAM is the constraint that has bitten this project
  hardest, and the host cannot feel it at all.
- **No FPU difference.** The Pico 1 has no hardware FPU; the host does.
  Floating-point results may agree here and not there.
- **No strip pipeline, no core 1.** The host takes the full-framebuffer
  path, so the Pico 1's ping-pong strip rendering is never exercised.
- **No panel, no SPI, no DMA timing.** Rendering is correct-by-construction
  here in a way it is not on glass.
- **No PSRAM.** `math::psram_backend` runs over a `malloc` arena, so
  [#24](https://github.com/moodoki/graphite_picocalc_gc/issues/24)'s
  intermittent read fault cannot reproduce here by construction. This is
  not an investigative route for it.

The hazard is that the host build is *pleasant* — fast, visual, no
flashing — so it gets used first and trusted too far. Treat anything about
memory, float behaviour or timing as unanswered until a board says
otherwise.

## Driving it

| Flag | Effect |
|---|---|
| `--shot <file.ppm>` | Render one frame and exit. Required. |
| `--eval <line>` | Submit a line to the home screen first, as the firmware's serial injection does. Repeatable. |
| `--key <name>` | Queue one key: a name from `platform::key_names` (`up`, `enter`, `esc`, `f1`…) or a single character. Repeatable. |
| `--keyscript <file>` | Replay a file of key names. Whitespace-separated, `#` comments to end of line. |
| `--run <script.py>` | Run a Python file through the same `exec_file()` the program screen uses for an SD app. |

Key scripts are how a screen that must be *navigated to* gets photographed
— the file manager, a modal softkey set, a screen with a flag raised. The
names come from `src/platform/key_names.*`, shared with the MicroPython
bindings, so a name cannot mean one thing to a script and another to an
app.

## Storage

`/picocalc/...` maps to `$PICOCALC_HOME`, or `~/.picocalc` when that is
unset. A directory under `apps/` with an `app.txt` is a launcher tile,
exactly as on the card, so SD app loading is testable without a reader.

Two deliberate differences from the device, both in
`host/storage_posix.cpp`: directory listings are **sorted** (POSIX
`readdir` order is filesystem-dependent, and the committed images must be
byte-identical on macOS and Linux), and paths containing a `..` component
are **refused** (the root is a directory inside someone's home, and paths
are composed from user-supplied manifest data).

## How the source list is shared

`host/CMakeLists.txt` is its own `project()` — the root one is bound to the
Pico SDK from its first lines and shares no toolchain, linker script or
output format with a desktop binary. But it lists **no sources of its
own**: both projects `include(cmake/graphite-sources.cmake)`.

That is D92, and the reason is a measurement rather than a preference. The
downstream Luckfox fork made the same split with a *copy* of the list, and
one phase later the copy was 29 files stale. **A new file goes in one of
the lists in `cmake/graphite-sources.cmake`, and both targets see it.**

## Adding platform coupling

`src/` contains exactly **two** `#if !PICOCALC_HOST` guards
(`gfx/framebuffer.cpp`, `apps/mode_screen.cpp`). That number is a
deliberate metric, not an accident: shim headers were rejected in D94
precisely because they would let the count grow silently.

So when a shared file needs something Pico-specific, the first question is
whether it belongs behind `platform::` instead. Three times in Phase 6.4
the answer was yes — `graph_screen.cpp` used the existing
`platform::uptime_us()`, `micropython_embed.cpp` got
`platform::stack_top()`, and the key-name table moved to
`platform/key_names.*`. Each cost tens of bytes of flash and **no SRAM**.
