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

**Status**: **PROPOSED — drafted 2026-08-23, not yet accepted.** D92-D96 are
open. Unlike 6.3, the feasibility question here is already answered by
measurement (§2), so the spike is short and the risk is concentrated in
maintenance rather than in whether it can work at all.

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
| `src/gfx/framebuffer.cpp` | `pico/multicore.h` | The core-1 display service. #33's stated boundary. |
| `src/apps/mode_screen.cpp` | `pico/bootrom.h` | Reboot-to-BOOTSEL menu entry. |
| `src/apps/graph_screen.cpp` | `pico/time.h` | Newer than the fork; not covered by it. |

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

The three files in §2 get `#if !PICOCALC_HOST` guards in the fork's style, with
the host branch declaring what it needs from a backend:

```cpp
#if !PICOCALC_HOST
#include "pico/bootrom.h"
#else
namespace host { void request_exit(); }
#endif
```

The rejected alternative is a `host/shims/pico/*.h` directory of fake headers.
It would need no `src/` edits at all, which is its whole appeal — and that is
the objection. The guard count *is* the coupling metric. Three is a number we
can watch; a shim directory makes new coupling invisible and lets the number
grow silently.

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

## 4. Task breakdown

**6.4.0 — Spike: one screen to a PPM, on both OSes.** Guards for the three
files, `display_headless.cpp`, minimal `platform_host.cpp`, the shared source
list, no MicroPython, no SDL. Ends when `graphite-shot` renders the home screen
to a PPM on macOS *and* Linux. Gates everything else, but §2 says it should be
short.

**6.4.1 — Shared source list.** `cmake/graphite-sources.cmake`, root project
converted to consume it. Verify both Pico targets still build byte-identically
before and after.

**6.4.2 — Platform backends, non-graphical.** `storage_posix`, `psram_arena`,
`system_host`, `power_stub`, `fault_stub`, `commands_file`. Ported from the
fork against current `main`.

**6.4.3 — MicroPython on host.** `MICROPY_GCREGS_SETJMP=1` (D96), host heap and
stack sizing, `picocalc_mp_init` call site. Ends when `py` at the home screen
runs a script and `print()` reaches stdout.

**6.4.4 — Screenshot manifest.** Expression/screen → filename, one command
regenerates the whole doc image set, in the shape `gen-doc-reference.py`
already uses. **Closes #33.**

**6.4.5 — SDL backends.** `display_sdl`, `keyboard_sdl`, `sound_sdl` (D95),
`graphite-desktop`. **Closes #42.**

**6.4.6 — CI.** Build `graphite-shot` on Linux and macOS runners; regenerate
the image set and fail on drift. Note that CI today runs neither the host
suite nor clang-tidy — this adds a third thing it should run and does not fix
the first two.

**6.4.7 — Look at #52.** Not a fix; the first use of the instrument, to
confirm it shows a bug the lint gate structurally cannot.

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

## 6. Open questions

1. **Does `graphite-desktop` need serial injection?** The fork put it behind
   `--uart-inject`. On a desktop the natural equivalent is stdin, and the
   scripted-input path may be more useful than the interactive one for #52-style
   checks.
2. **Should the headless target grow a key-script format** (a list of keypresses
   → a screenshot), so a docs image can be *any* screen reached by navigation
   rather than only a directly-constructed one? Probably yes, probably not in
   6.4.0.
3. **Where do generated images live** — committed to the repo so the README
   works on GitHub without a build, or generated into a build dir and published
   only to the wiki? #33 assumes committed; drift-checking in CI assumes
   regenerable. Both is possible and is what `gen-doc-reference.py` does.

## References

- [#33](https://github.com/moodoki/graphite_picocalc_gc/issues/33) — host-side renderer for docs screenshots
- [#42](https://github.com/moodoki/graphite_picocalc_gc/issues/42) — desktop emulator build target
- [#19](https://github.com/moodoki/graphite_picocalc_gc/issues/19) — device framebuffer capture (complement, not superseded)
- [#52](https://github.com/moodoki/graphite_picocalc_gc/issues/52) — softkey truncation, the first target
- [phase6-spec.md](phase6-spec.md) §4.5 — SD app manifests
- [decisions.md](../notes/decisions.md) — D92-D96
- [beapig/graphite_picocalc_gc_luckfox_lyra](https://github.com/beapig/graphite_picocalc_gc_luckfox_lyra) — the reference port, commit `9f3e499`
