# Phase 6.4 Spec: Desktop target (host build for Linux and macOS)

**Prerequisite phases**: [Phase 6](phase6-spec.md) closed and merged (it is —
`v0.5.0`). 6A's `AppRegistry` and 6B's MicroPython embed build are both in
scope of the port, so this cannot start before them.

**Scope**: A third build target that runs the shared `src/` tree as a native
application on a development machine — **Linux and macOS both** — plus a
headless variant of the same build that renders screens to image files without
a window. Closes [#33](https://github.com/moodoki/graphite_picocalc_gc/issues/33)
(host-side renderer for docs screenshots) and
[#42](https://github.com/moodoki/graphite_picocalc_gc/issues/42) (desktop
emulator build target).

**Status**: **ACCEPTED 2026-08-28 — in progress.** D92-D98 are accepted; D94
and D95 carry same-day amendments (§2.2). Unlike 6.3, the feasibility question
here is already answered by measurement (§2), so the spike is short and the risk
is concentrated in maintenance rather than in whether it can work at all. The
2026-08-28 pass added the task estimates (§4), the file list (§3.7) and the
verification rules (§7), and closed the three questions the draft left open
(§6, D97-D98). **~50 hrs, or ~40 without the SDL window**, which grew by the
sound seam §2.2 found missing.

**Sub-phase numbering**: dotted. A desktop build was never part of Phase 6's
committed goals; it turned up out of #42's observation that a downstream fork
had already done most of it. 6.1 (home-screen convenience scripts) and 6.2
(PCM sampler audio) are reserved as candidates; 6.3 (native `.uf2` apps) is
drafted but unaccepted. This is independent of all three and gates none of
them.

---

## 1. Why this is being built now

Two arguments, and the second is the stronger one.

**The cheap one: docs images.** #33 exists because the README uses a photograph
of the device, which is the wrong tool for 1-bit glyph work and goes stale
silently. It scoped itself down to *only* the natural math display because
`src/gfx/framebuffer.cpp` was not host-clean — it includes `pico/multicore.h`,
being the dual-core display service rather than the pixel maths. That was a
real boundary when #33 was written. It is a 31-line boundary, and §2 shows it
has already been crossed by someone else.

**The one that actually matters: we cannot see the screen.** The 2026-08-23
session could flash the board, drive MicroPython over serial, push a 10 KB file
to the SD card and read the whole card back — and still could not look at a
single visual fix. Two open bugs are *visual by nature*:
[#52](https://github.com/moodoki/graphite_picocalc_gc/issues/52) (softkey
labels truncate, `MKDIR` renders `MKDI`) is invisible to the text-fits lint
gate **by design**, because `draw_softkeys` truncates to 6 chars a cell
deliberately; and [#49](https://github.com/moodoki/graphite_picocalc_gc/issues/49)
(syntax highlighting) is nothing but appearance. A host renderer is the
cheapest instrument that makes either one checkable.

This does not discharge
[#19](https://github.com/moodoki/graphite_picocalc_gc/issues/19) (capture the
*device's* framebuffer). #19 covers what the real hardware actually put on the
panel, including driver and panel behaviour this target does not model. The two
are complements: #19 is ground truth, 6.4 is the fast loop.

## 2. Current state — measured, not estimated

Everything in this section was compiled on macOS (arm64, Apple clang) against
`main` at `v0.5.0-2-gaeaf440` on 2026-08-23. It is evidence, not projection.

**The portable tree already compiles natively.** With `-DPICOCALC_HOST=1`,
`-Isrc -Idrivers/…`, no shim headers and no source edits: **98 of 101
non-platform sources compiled clean.** Three failed, each on a single include:

| File | Include | Note |
|------|---------|------|
| `src/gfx/framebuffer.cpp` | `pico/multicore.h`, `pico/stdlib.h` | The core-1 display service. #33's stated boundary. **Two includes, not one** — see §2.2. |
| `src/apps/mode_screen.cpp` | `pico/bootrom.h` | Reboot-to-BOOTSEL menu entry. |
| `src/apps/graph_screen.cpp` | `pico/time.h` | Newer than the fork; not covered by it. **Needs no guard** — §2.2. |

**All 29 files of Phase 6 drift are host-clean at the include level** —
`src/scripting/*` (including `mp_calc_module.c` and `mp_port.c`),
`sd_apps.cpp`, `sd_app_scan.cpp`, `app_registry.cpp`, `io_scratch.cpp`,
`launcher_screen.cpp`, `notepad_screen.cpp`, `program_screen.cpp`,
`file_list.cpp`, `text_buffer.cpp`, `text_editor_widget.cpp`, `output_log.cpp`,
`prompt_line.cpp`, `var_store.cpp`. The `platform::` abstraction held through
Phase 6 without anyone checking that it had. That is the single most important
number in this spec: **the port's cost did not grow during 6A/6B/6C.**

**MicroPython builds natively.** `drivers/micropython_port/micropython_embed.mk`
carries no cross-compile assumptions — it generates a portable C tree that
CMake then compiles with whatever compiler is configured. Generated with
`CC=clang` and compiled: **135 of 136 files clean.** The holdout is
`shared/runtime/gchelper_generic.c`, whose aarch64 GC register scan uses the
GCC-only `const register long x19 asm ("x19")` form that clang rejects.
**`MICROPY_GCREGS_SETJMP=1` fixes it — 136/136.** That is MicroPython's own
documented escape hatch for exactly this, not a workaround of ours.

`src/scripting/mp_port.c` is already fully parameterised —
`picocalc_mp_init(heap, heap_size, stack_top, stack_limit)` takes all four from
its caller and references no Pico symbol. The host caller supplies a `malloc`'d
heap and a main-thread stack address.

### 2.1 The downstream fork, and what to take from it

[`beapig/graphite_picocalc_gc_luckfox_lyra`](https://github.com/beapig/graphite_picocalc_gc_luckfox_lyra)
(MIT, same copyright holder) runs the calculator as a Linux/SDL2 app on a
Luckfox Lyra (RK3506). Its port commit `9f3e499` is the reference #42 points
at, and it is a good one: **it reuses the existing `platform::` abstraction
rather than working around it.** It touches only three shared files —
`CMakeLists.txt` (1 line), `config.hpp` (7, folding `PICOCALC_HOST` into the
Pico 2 "full framebuffer, synchronous push" branch) and `framebuffer.cpp` (31,
guarding the core-1 service) — and adds ~1,850 lines of backends under
`host/`. It also added `src/platform/sound.{hpp,cpp}`, an abstraction we since
carry anyway.

Take: the shape, the `PICOCALC_HOST` guard pattern, `storage_posix.cpp`,
`psram_arena.cpp`, `keyboard_sdl.cpp`, `display_sdl.cpp`, `power_stub.cpp`,
`fault_stub.cpp`, `commands_file.cpp`, and the `mode_screen.cpp` guard (reboot
becomes "exit application").

**Do not take:** commits `93ebf1f` and `f522e9a`. They are X11/KMSDRM
workarounds for that device's broken software GLES driver
(`SDL_HINT_FRAMEBUFFER_ACCELERATION=0`, forced `SDL_RENDERER_SOFTWARE`,
`SDL_EVDEV_DEVICES`) and fluxbox's auto-repeat being off. Device-specific, and
actively wrong on a desktop.

**Do not merge the branch.** It forked at `v0.4.1`, before Phase 6 — 29 files
of drift, and its `host/CMakeLists.txt` source list predates every app,
scripting and text-editor file we now have. Port the pattern against current
`main`. §5 treats that drift as this phase's central risk rather than a
one-time cost.

`system_linux.cpp` is misnamed in the fork: it includes only `<ctime>`,
`<cstdio>` and `<cstring>` and is portable as written. **One file is genuinely
Linux-bound**: `sound_alsa.cpp`, which `dlopen`s ALSA and spawns a pthread.
D95 replaces it.

### 2.2 What re-reading the source before writing code changed

§2 was a compile survey: it ran the compiler and recorded which includes
failed. Before starting 6.4.0 the same files were read rather than compiled, and
three things moved. Two of them make the phase smaller and one makes it bigger,
which is roughly what an honest re-check should look like.

**`graph_screen.cpp` needs no guard, so D94's count is two.** Its entire
coupling is two `time_us_64()` calls timing `recompute()` (`:338`, `:359`), and
`platform::uptime_us()` has been there all along (`src/platform/system.hpp:28`)
— `home_screen.cpp:194,391` measures a duration with it in the identical shape.
So it is a three-line cleanup that brings the file in line with the rest of the
tree. **A survey of includes cannot see this; only a reading of uses can.**

**`framebuffer.cpp` has a trap that would have cost a debugging session.** It
gates the frame buffer *and the whole full-frame render body* on the raw macro
`#if PICOCALC_PICO2` (`:16`, `:91`, `:237`), not on
`config::kUseFullFramebuffer`. D92's `config.hpp` fold is therefore necessary
and **not sufficient**: on its own it yields a host build that compiles, links,
runs and draws nothing, with no diagnostic. Those three `#if`s widen to
`PICOCALC_PICO2 || PICOCALC_HOST`. The file also includes `pico/stdlib.h`, which
the §2 table missed.

**There is no `platform::Sound`, and no commands seam.** §2.1 above claimed we
"since carry anyway" the sound abstraction the fork added. We do not:
`drivers/pwm_sound` is linked (`CMakeLists.txt:47`, `:401`) with **zero callers
in `src/`** — this calculator has never made a sound. So 6.4.5 must define the
seam and wire the firmware side before a host backend means anything, which is
why its estimate moves. Likewise `commands_file.cpp` has nothing to implement:
serial injection is an inline `#if PICOCALC_SERIAL_INJECT` block in
`src/main.cpp:757-846`, not a `platform::` interface. It leaves §3.7's list.

The pattern in all three is the same one the 2026-08-23 session recorded twice
over — **probing beat reading, and running the example caught the example being
wrong.** A file list read off another repository is a reading, not a probe.

## 3. Design

### 3.1 A separate CMake project, with a shared source list (D92)

`host/CMakeLists.txt` is its own `project()`, referencing `../src`, exactly as
the fork has it. The root `CMakeLists.txt` is bound to the Pico SDK from its
first lines — `pico_sdk_init()`, the SDK toolchain file,
`pico_add_extra_outputs`, `pico_enable_stdio_usb` — and making it dual-mode
would mean guarding nearly every line of it to serve a target that shares no
toolchain, no linker script and no output format.

**But the fork's source list is a copy, and that copy is why it is 29 files
stale.** So this phase adds what the fork does not have:
`cmake/graphite-sources.cmake`, defining `GRAPHITE_PORTABLE_SOURCES` (the
target-agnostic tree) and `GRAPHITE_PICO_SOURCES` (the platform backends the
firmware uses), `include()`d by both the root project and `host/`. A new file
lands in one list, and both targets see it. This is the mechanism that keeps
§5.1 from happening to us.

### 3.2 Two display backends, headless first (D93)

`platform::Display::push_rect(x, y, w, h, buf)` is the whole seam.

**`display_headless.cpp`** composites into a full-screen RGB565 buffer and
writes PPM on demand. No external dependency whatsoever. This is what CI runs
and what regenerates the docs images.

**`display_sdl.cpp`** is the interactive window, per the fork. SDL2 is a
dependency of the *emulator*, never of the render check.

Built as two executables from one source list: `graphite-shot` (headless,
scriptable) and `graphite-desktop` (SDL). Headless lands first — it closes #33
by itself, it is CI-able with no packages to install, and it is the shorter
path to looking at #52.

### 3.3 Guards, not shim headers (D94)

**Two** of the three files in §2 get `#if !PICOCALC_HOST` guards in the fork's
style, with the host branch declaring what it needs from a backend (§2.2
retired the third):

```cpp
#if !PICOCALC_HOST
#include "pico/bootrom.h"
#else
namespace host { void request_exit(); }
#endif
```

The rejected alternative is a `host/shims/pico/*.h` directory of fake headers.
It would need no `src/` edits at all, which is its whole appeal — and that is
the objection. The guard count *is* the coupling metric. Two is a number we
can watch; a shim directory makes new coupling invisible and lets the number
grow silently. §2.2 is the mechanism working on its first outing: the count made
someone ask whether the third file's coupling belonged, and it did not.

### 3.4 Sound through SDL_audio (D95)

The fork's ALSA backend is the one non-portable file. `SDL_audio` covers
ALSA/PulseAudio/CoreAudio behind one API, SDL2 is already the desktop
dependency, and it removes the `dlopen`/pthread machinery rather than
duplicating it for CoreAudio. The headless target links a silent stub, so the
render check needs no audio library either.

### 3.5 What this target deliberately does not model

It is a **development instrument, not a fidelity emulator**, and the difference
must stay loud because the failure mode is false confidence:

- **No SRAM ceiling.** The host has a process address space. The Pico 1's
  15.2 KB of free SRAM is the constraint that has bitten this project hardest
  and the host cannot feel it at all.
- **No FPU difference.** The Pico 1 has no hardware FPU; the host does.
  Floating-point results may agree here and not there.
- **No strip pipeline, no core 1.** `kUseFullFramebuffer` is true on host
  (D92's `config.hpp` change follows the fork). The Pico 1's ping-pong strip
  path — the one #38 spent two measurement rounds on — is not exercised.
- **No panel, no SPI, no DMA timing.** Rendering is correct-by-construction
  here in a way it is not on glass.

**A green host build is not a hardware verification and never substitutes for
one.** Every phase in this repo is marked Complete only when hardware-verified
on both boards, and 6.4 does not change that rule for any other phase — it is
explicitly *not* a route to skipping the board.

### 3.6 Storage

`storage_posix.cpp` maps `/picocalc` to `~/.picocalc` (fork behaviour). SD app
manifests (6B §4.5) then work unchanged: a directory under
`~/.picocalc/apps/` is a launcher tile, so the app-loading path — the newest
and least-exercised code in the tree — becomes testable without a card reader.
FatFs is not compiled on host; `sd_diskio.cpp` and `sd_card.cpp` are firmware
sources, not portable ones.

### 3.7 New source files

Derived from §2.1's take-list and §3.1-3.6. Nothing here exists yet, and the
`src/` tree gains **no** new files — only the three guards of §3.3.

```
host/
├── CMakeLists.txt          # NEW (6.4.0): own project(), consumes the shared
│                           #   list; builds graphite-shot and
│                           #   graphite-desktop from one source set (D92)
├── main_host.cpp           # NEW (6.4.0): entry point for both executables —
│                           #   argument parsing, init order, the run loop
├── platform_host.cpp       # NEW (6.4.0): the platform:: seam's host half
├── display_headless.cpp    # NEW (6.4.0): full-screen RGB565 composite,
│                           #   PPM on demand, zero dependencies (D93)
├── sound_stub.cpp          # NEW (6.4.0): silent; graphite-shot links this so
│                           #   the render check needs no audio library
├── storage_posix.cpp       # NEW (6.4.2): /picocalc -> ~/.picocalc (§3.6)
├── psram_arena.cpp         # NEW (6.4.2): malloc arena behind the PSRAM
│                           #   allocator (see §5.4)
├── system_host.cpp         # NEW (6.4.2): the fork's system_linux.cpp, which
│                           #   is portable as written and misnamed there
├── power_stub.cpp          # NEW (6.4.2)
├── fault_stub.cpp          # NEW (6.4.2)
├── scripting_stub.cpp      # NEW (6.4.0): PythonInterpreter with init() ->
│                           #   false. home_screen.cpp calls scripting::python(),
│                           #   so the spike cannot link without it; 6.4.3
│                           #   replaces it with the real runtime
├── keyscript.cpp           # NEW (6.4.4): key-script parser and driver,
│                           #   shared by both executables (D97)
├── display_sdl.cpp         # NEW (6.4.5)
├── keyboard_sdl.cpp        # NEW (6.4.5)
└── sound_sdl.cpp           # NEW (6.4.5): SDL_audio, not ALSA (D95)

cmake/
└── graphite-sources.cmake  # NEW (6.4.1): GRAPHITE_PORTABLE_SOURCES and
                            #   GRAPHITE_PICO_SOURCES, include()d by both the
                            #   root project and host/ (D92, §5.1)

scripts/
└── gen-doc-images.py       # NEW (6.4.4): drives graphite-shot over the
                            #   manifest, in gen-doc-reference.py's shape

docs-site/images/           # NEW (6.4.4): committed PNGs, regenerable and
                            #   drift-checked in CI (D98)
```

**Existing files modified** — the complete list, and it is short by design
(§3.3): `src/gfx/framebuffer.cpp` and `src/apps/mode_screen.cpp` gain
`#if !PICOCALC_HOST` guards; `src/apps/graph_screen.cpp` swaps two
`time_us_64()` calls for the `platform::uptime_us()` that already exists, which
is a cleanup and **not** a guard (§2.2); `src/config.hpp` folds `PICOCALC_HOST`
into the full-framebuffer branch (7 lines in the fork); the root
`CMakeLists.txt` switches to the shared list. **Any growth in this list is
§3.3's coupling metric moving, and belongs in a review comment.**

## 4. Task breakdown

Solo developer, part-time. Estimates are calibrated against 6.3's ~53 hrs;
this phase is larger in file count and much smaller in risk, because §2
already answered by compiling what 6.3.0 has to answer by flashing.

| id | task | hrs | done when |
|---|---|---|---|
| **6.4.0** | **Spike — one screen to a PPM, on both OSes.** The two guards (D94, §2.2), `display_headless.cpp`, `platform_host.cpp`, `main_host.cpp`, `scripting_stub.cpp`, `psram_arena.cpp`, an ad-hoc source list. No MicroPython, no SDL, no manifest. **Pulls one Linux CI job forward from 6.4.6**, since the two-OS gate is otherwise unmeetable from a macOS-only machine | 6 | `graphite-shot` writes a PPM of the home screen on **macOS and Linux**, and both Pico targets still build. §2 says this should be short; if it is not, **stop and re-plan** — the measurement was wrong |
| 6.4.1 | **Shared source list** — `cmake/graphite-sources.cmake`, root project converted to consume it, 6.4.0's ad-hoc list replaced | 3 | Both Pico `.uf2`s are **byte-identical before and after** the conversion, and `host/` names no `src/` file directly |
| 6.4.2 | **Platform backends, non-graphical** — `storage_posix`, `psram_arena`, `system_host`, `power_stub`, `fault_stub`, `commands_file`, ported from the fork against current `main` | 5 | The calculator runs headless end to end: an expression evaluates, a variable persists across a restart via `~/.picocalc`, and an SD app manifest under `~/.picocalc/apps/` appears as a launcher tile (§3.6) |
| 6.4.3 | **MicroPython on host** — `MICROPY_GCREGS_SETJMP=1` (D96), host heap and stack sizing, the `picocalc_mp_init` call site | 5 | `py` at the home screen runs a script and `print()` reaches stdout; `examples/apps/periodic/` — the largest script we have — loads and draws |
| 6.4.4 | **Screenshot manifest + key scripts.** `scripts/gen-doc-images.py`, expression/screen → filename, `keyscript.cpp` (D97) so an image can be any screen reachable by navigation. PPM → PNG on the way out. **Closes #33** | 7 | One command regenerates the whole committed image set; a re-run is byte-identical; at least one image is of a screen reached by a key script rather than constructed directly |
| 6.4.5 | **SDL backends** — `display_sdl`, `keyboard_sdl`, the `graphite-desktop` executable, stdin injection (D97). **Plus the sound seam itself**: §2.2 found `platform::Sound` does not exist, so this defines it, wires the firmware side to `drivers/pwm_sound` (which has had no caller ever) and only then writes `sound_sdl` (D95). **Closes #42** | 10 | A window opens on macOS and Linux, the keyboard drives the calculator, a tone plays **on the board as well as on the desktop**, and stdin drives it too. **Separable** — everything above closes #33 without it |
| 6.4.6 | **CI.** Build `graphite-shot` on Linux and macOS runners; regenerate the image set and fail on drift (D98). **Also lands `./scripts/host-tests.sh` in CI**, since §5.1's mitigation is worthless if CI does not actually run | 4 | A PR that changes a rendered screen without regenerating fails; a PR that breaks the host suite fails. clang-tidy stays out of scope and stays named as still missing |
| 6.4.7 | **Verify the instrument — #52.** Not a fix; a screenshot of the file manager's softkey row. **Gates 6.4.8** | 2 | The truncation is visible in a committed image, and #52 carries it as a comment. If it is *not* visible, that is a finding about the instrument, 6.4.8 does not start, and it goes in the phase's notes |
| 6.4.8 | **Sweep every chrome bar** (§4.1) — a manifest entry per softkey set and per status-bar title, including the modal variants and the D26 unhealthy state. **Every defect found is filed as an issue, not fixed here** — they are addressed in a separate bugfix session | 5 | Every `draw_softkeys` and `draw_status_bar` call site in `src/` has a committed image, and every defect the images show is an open issue labelled `area:ui`. **The images stay in the set**, so D98's drift check turns this into a permanent regression gate rather than a one-off audit |
| 6.4.9 | **Docs and close** — README section carrying §3.5's warning verbatim, `ROADMAP.md` row, `dependencies.md` (SDL2 is a developer dependency, never a firmware one), a developer-facing `docs/host-build.md` | 3 | §7's checklist complete |

**Total ~50 hrs**, or **~40 without 6.4.5**, which is separable and drops the
only external package this phase would introduce — and which §2.2 made both
bigger and more clearly optional, since it now carries firmware work the rest of
the phase does not need. The headless path alone
closes #33, feeds CI and answers #52; the SDL window closes #42 and is the
part that is *pleasant* rather than necessary — §5.2 is worth re-reading
before deciding the order.

**Build order is the table order.** 6.4.0 gates everything. The one thing
worth resisting: 6.4.5 is the fun task and the least load-bearing.

### 4.1 What the sweep already has a list of

A static scan on 2026-08-28, before any of this is built, so 6.4.8 starts from
suspects rather than from scratch. **None of these are confirmed** — that is
6.4.8's whole point, and the fact that they had to be derived by hand from
arithmetic is the argument for building the instrument.

**The softkey bar takes labels of 4 characters, not 6.** `draw_softkeys`
renders `"%d:%s"`, so the `n:` prefix costs 2 of the 6 characters a cell holds
at 8 px in a 53 px cell. Every label in `src/` was checked against that:

| screen | labels | verdict |
|---|---|---|
| `files_screen.cpp:590` | `CUT` `MOVE` `REN` **`MKDIR`** | **MKDIR is 5 — #52.** The comment above it claims both labels fit; that claim is wrong |
| `home_screen.cpp:1163` | `Y=`/`PAR`/`R=` `WIN` `MODE` `TRC` `GRPH` `APPS` | fits |
| `graph_screen.cpp:1570` | `Y=`/`PAR`/`R=`/`u=` `WIN` `MODE` `TRC` `TBL` `CALC` | fits |
| `table_screen.cpp:349` | `Y=`/`PAR`/`R=` `SETP` `MODE` `TRC` `GRPH` | fits |
| `text_editor_widget.cpp:389` | `RUN` `SAVE` `LOAD` `NEW` | fits |
| `program_screen.cpp:305` | `EDIT`/`BACK` | fits |
| `launcher`, `mode`, `settings` | all empty | n/a |

So **#52 is currently the only truncating softkey label**, which is worth
knowing: the sweep's value in the softkey bar is not a pile of bugs today, it
is that the 4-character budget is undocumented and one character from being
violated again. Committing the images makes the next violation a diff.

**The status bar is the richer target, because it has no collision check at
all.** `draw_status_bar` draws the title left at `x=4` and the right-aligned
block (`[2nd] [A]` + angle + display mode, then the battery) at a computed
`rx`, and **nothing clamps the title against `rx`**. Two consequences:

1. **A long title is silently overdrawn** by the right block, which is painted
   after it.
2. **D26's health indicators inherit the problem and amplify it.** `SD` and
   `PSRAM` are placed at `4 + text_width(title) + 10`, so a long title pushes
   them rightward into the indicator block — and they only appear when a
   subsystem is *unhealthy*, i.e. exactly when the user most needs to read
   them. This state is not reachable in normal use and has almost certainly
   never been looked at.

The static titles (`STATS`, `SETTINGS`, `CONSTANTS`, `STAT PLOTS` — the
longest at 10 characters) all appear to clear the right block. **The one
genuine suspect is `program_screen.cpp:283`**, which passes `app_name_`
straight through from an SD app manifest. `SdAppManifest::name` is `char[24]`,
so a 23-character app name draws ~184 px from `x=4` — into the region the
right-aligned block occupies on a screen showing `2nd A RAD SCI`. **That is
user-controllable from `app.txt`**, needs no code change to trigger, and is
the first thing 6.4.8 should point the renderer at.

The sweep must therefore cover, per screen: the **modal** softkey variants
(the `Y=`/`PAR`/`R=`/`u=` fork, `MOVE` appearing only while a cut is armed,
`EDIT`/`BACK`), the **flag** states (`2nd`, alpha, each angle mode, each of
FLT/FIX/SCI/ENG), and the **D26 unhealthy** state — not just one screenshot a
screen.

## 5. Risks and mitigations

### 5.1 Drift — the main risk, with evidence

The fork is 29 files stale after one phase. That is what happens to a
duplicated source list, and it is the failure mode most likely to make this
phase worthless in six months: a host target that no longer builds is worse
than none, because it costs CI time and lies about its own coverage.

Three mitigations, in order of strength: the **shared source list** (§3.1)
makes the common case automatic; **CI on every PR** (6.4.6) makes breakage
loud immediately; and the **guard count** (§3.3) stays a reviewable number, so
new Pico coupling in `src/` is visible in a diff rather than discovered later.

### 5.2 False confidence

§3.5 is the mitigation and it is deliberately blunt. The specific hazard is
that the host build is *pleasant* — fast, visual, no flashing — so it will get
used first and trusted too far, particularly for anything touching SRAM or
float. A note in the phase's own README section, and the rule that Complete
still means hardware-verified.

### 5.3 SDL2 on macOS

The `SDL_main` shim renames `main` on macOS to set up `NSApplication`; event
pumping must stay on the main thread. Single-threaded design makes this
routine, and 6.4.0 deliberately proves the headless path first so a macOS SDL
snag cannot block #33.

### 5.4 `array_backend_pico.cpp` over a malloc arena

The fork compiles it against a `malloc`-based `psram_arena`. Worth confirming
that still holds given
[#24](https://github.com/moodoki/graphite_picocalc_gc/issues/24)'s open
intermittent PSRAM read fault — and worth noting the host build cannot
reproduce #24 by construction, since there is no PSRAM. It is not an
investigative route for that bug.

## 6. Questions resolved before acceptance (2026-08-28)

The three questions this spec was drafted with, and how each was closed. Kept
rather than deleted, because the reasoning is the useful part.

**1. Does `graphite-desktop` need serial injection? — Yes, as stdin, and both
executables get a key script instead (D97).** The fork put it behind
`--uart-inject`. The 2026-08-23 session settled the value question by
accident: `PICOCALC_SERIAL_INJECT` turned out to be *a full remote*, enough to
push a 10 KB file to the card and read the whole card back without touching
the device. The desktop equivalent is stdin, and it costs almost nothing on
top of the command path that already exists. The **scripted** path is the more
valuable of the two, and it belongs to both executables, not just the window —
which is why it is D97's subject rather than a `graphite-desktop` flag.

**2. Should the headless target grow a key-script format? — Yes, in 6.4.4, not
6.4.0 (D97).** The spec's own guess was "probably yes, probably not in 6.4.0",
and that holds: 6.4.0 exists to prove the render path on two OSes and must
stay small enough to fail fast. But without key scripts the image set is
limited to screens that can be *constructed*, and #52's softkey row is
reached by navigating into the file manager — so the instrument would not
answer the question 6.4.7 exists to ask — nor reach most of §4.1's
chrome bars for 6.4.8. Deferring it past 6.4.4 would mean
building the manifest twice.

**3. Where do generated images live? — Committed **and** regenerable, with a
CI drift check (D98).** The two constraints only looked opposed: #33 wants
them committed so the README renders on GitHub without a build, and CI wants
them regenerable so drift is caught. `gen-doc-reference.py` already resolves
exactly this shape for `docs-site/reference/`, and the Docs workflow's
`check-reference` job is the pattern to copy. It is also the pattern that
**just failed us in the way that matters** — it worked, nobody read the red
badge, and the wiki served a calculator with no apps for eight days. So 6.4.6
inherits that lesson as a requirement, not just a job: see §7.

## 7. Verification

**What "Complete" means for this phase.** 6.4 is the one phase in this repo
whose deliverable does not run on the calculator, so the standing rule needs
stating rather than assuming:

- **6.4 itself is complete when it is green on both host OSes in CI** — there
  is no hardware pass for a desktop target, and inventing one would be
  theatre.
- **6.4 changes nothing about how any other phase is verified.** §3.5 is the
  reason and it is deliberately blunt. Every other phase stays Complete only
  when hardware-verified on both boards, and a screenshot from `graphite-shot`
  is never evidence about the panel.
- **6.4.1 carries the one hard regression gate**: both Pico `.uf2`s must be
  byte-identical across the source-list conversion. If they are not, the
  shared list changed the firmware, and that is a stop-and-diagnose.

**The checklist:**

- `./scripts/build-all.sh` — both boards build clean, before and after 6.4.1,
  compared by hash.
- `./scripts/host-tests.sh` — the full suite (22 files / 3,386 checks at
  `v0.5.0`) passes when built through `host/` as well as through the existing
  test target.
- `./scripts/lint.sh` and `python3 scripts/validate_md.py` on every touched
  doc.
- `graphite-shot` renders the committed image set on **both macOS and Linux**,
  and a re-run is byte-identical on each.
- `graphite-desktop` opens, takes keyboard input and plays a tone on both OSes
  (6.4.5 only).
- **A deliberate drift test**: change a rendered screen, do not regenerate,
  confirm CI goes red. This is the one check that would have caught the
  2026-08-15 wiki outage, and it is not satisfied by the job merely existing —
  it must be *seen* to fail.
- **A deliberate host-suite test**: break one host check, confirm CI goes red.
  Same reasoning.
- **The chrome sweep is complete when the image set covers every
  `draw_softkeys` and `draw_status_bar` call site** (§4.1), in its modal and
  flag variants, not one image a screen. **Every defect it shows is filed as
  an issue and none are fixed in this phase** — the fixes are a separate
  bugfix session, because a tooling phase that starts repairing the UI it is
  photographing has no natural end. A long list of new issues is a successful
  outcome here, not a blocked one; the phase closes on the images and the
  issues existing, not on the bugs being gone.

**Explicitly not verified here**, and named so no one reads a green build as
more than it is: SRAM headroom, Pico 1 float behaviour, the strip pipeline,
core-1 timing, SPI and DMA, panel colour depth, and anything about the SD card
that is not POSIX file I/O.

## References

- [#33](https://github.com/moodoki/graphite_picocalc_gc/issues/33) — host-side renderer for docs screenshots
- [#42](https://github.com/moodoki/graphite_picocalc_gc/issues/42) — desktop emulator build target
- [#19](https://github.com/moodoki/graphite_picocalc_gc/issues/19) — device framebuffer capture (complement, not superseded)
- [#52](https://github.com/moodoki/graphite_picocalc_gc/issues/52) — softkey truncation, the first target
- [phase6-spec.md](phase6-spec.md) §4.5 — SD app manifests
- [decisions.md](../notes/decisions.md) — D92-D96
- [beapig/graphite_picocalc_gc_luckfox_lyra](https://github.com/beapig/graphite_picocalc_gc_luckfox_lyra) — the reference port, commit `9f3e499`
