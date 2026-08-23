# Architecture

## Layered design

```
┌─────────────────────────────────────────┐
│            apps/                        │  Application screens
│  HomeScreen, GraphScreen, YEditor,      │  (a Phase 4 addition: CASScreen,
│  WindowScreen, ModeScreen, ...          │   MatrixEditor, ProgramEditor)
├─────────────────────────────────────────┤
│            ui/                          │  Screen manager, widgets,
│  ScreenManager, Widget, InputLine,      │  softkeys, dialogs
│  Softkeys, Dialog                       │
├─────────────────────────────────────────┤
│            math/         render/        │  Math engine + natural math
│  Engine, tinyexpr++,     LayoutNode,    │  rendering. Phase 4 adds
│  Variables, format       LayoutBuilder, │  cas/ subsystem and matrix
│                          glyphs         │  ops alongside Engine.
├─────────────────────────────────────────┤
│            gfx/                         │  Drawing primitives, fonts,
│  Framebuffer, Font, primitives,         │  line-buffer pipeline, color
│  color                                  │  helpers
├─────────────────────────────────────────┤
│            platform/                    │  HAL — only layer that touches
│  Display, Keyboard, Storage, Psram,     │  hardware. Wraps drivers/
│  Audio, System                          │  in C++ classes.
├─────────────────────────────────────────┤
│  drivers/  (vendored C, read-only)      │  Coyote OS drivers (lcdspi,
│  + Pico SDK + ARM newlib                │  i2ckbd, rp2040-psram, pwm_sound),
│                                         │  FatFs; MicroPython (submodule)
└─────────────────────────────────────────┘
```

### Layer rules

The rules are simple but unconditional:

1. **Lower layers never depend on higher layers.** `gfx/` cannot include from `ui/`. `math/` cannot include from `apps/`. The dependency arrow points down.
2. **Application code (`apps/`, `ui/`, `math/`, `render/`) never directly calls Pico SDK functions or accesses hardware registers.** Only `platform/` may. Violations of this rule break the dual-target story and make testing infeasible.
3. **`drivers/` is read-only.** Treat it as upstream third-party code. Bug fixes happen in `platform/` wrappers, not in driver source.
4. **Headers are paired with implementations.** Every `.cpp` has a `.hpp`. Every `.hpp` is includable by name from any layer it's exposed to. Headers use `#pragma once`.
5. **Cross-cutting utilities** (e.g., `config.hpp`, generic helpers) live at `src/` root and are visible to all layers.

### Why this layering

Trying to support both Pico 1 and Pico 2 from one codebase is the project's tightest constraint. Without strict HAL discipline, hardware-specific code metastasizes throughout the codebase and the dual-target build becomes a maintenance nightmare. The layered model contains the dual-target complexity inside `platform/` (and its `#ifdef`-gated implementations), leaving the rest of the codebase target-agnostic.

The same separation lets the math engine and CAS evolve without affecting the UI, the renderer evolve without affecting the math, and individual screens be added or rewritten without touching foundations.

## Dual-target strategy

Both `RP2040` (Pico 1) and `RP2350` (Pico 2) compile from the same source tree. Differences are mediated through:

### Compile-time defines

`src/config.hpp` exposes constants that vary by target:

```cpp
namespace config {

#ifdef PICOCALC_PICO2
    constexpr bool USE_FULL_FRAMEBUFFER = true;
    constexpr size_t CAS_POOL_SIZE      = 128 * 1024;  // SRAM
    constexpr size_t PYTHON_HEAP_SIZE   = 96  * 1024;
    constexpr int    OVERCLOCK_HZ       = 0;            // Pico 2 stock fast enough
#else
    constexpr bool USE_FULL_FRAMEBUFFER = false;       // line-buffer mode
    constexpr size_t CAS_POOL_SIZE      = 64 * 1024;   // PSRAM
    constexpr size_t PYTHON_HEAP_SIZE   = 48 * 1024;
    constexpr int    OVERCLOCK_HZ       = 200'000'000;
#endif

}
```

Application code reads these constants. The `#ifdef` blocks live in one place.

### Build-system board selection

CMake takes `-DPICO_BOARD=pico` or `-DPICO_BOARD=pico2` and the SDK's board files configure everything downstream — clock setup, linker scripts, FPU flags, vector tables.

### Conditional implementations in `platform/`

When a HAL implementation differs by target (e.g., the on-board LED is on different GPIOs), use a single `.hpp` and `#ifdef`-gated implementation in the `.cpp`. Don't fork files.

## Dual-core model

The RP2040 and RP2350 both have two cores. The split is:

- **Core 0**: application loop. Runs the keyboard polling, screen manager, math engine, all of `apps/`, `ui/`, `math/`, `render/`. Never blocks on hardware.
- **Core 1**: display refresh. Owns the SPI peripheral for the LCD and the DMA channels driving it. Receives "render this strip" or "render this framebuffer" signals from core 0 and pushes pixels to the display.

### Core synchronization

The Pico SDK's multicore FIFO (`multicore_fifo_push_blocking`, `multicore_fifo_pop_blocking`) is the only sanctioned mechanism for cross-core communication. It's a 32-bit-wide hardware FIFO with built-in synchronization — no manual locks needed.

Avoid:

- Shared mutable state without explicit synchronization. If two cores need to share state, use a lock-free queue or message-passing through the FIFO.
- Calling `printf` from core 1 — USB stdio is not multi-core-safe in the SDK without spinlocks. Confine logging to core 0.
- Allocating heap from core 1 — newlib's heap is not thread-safe with default Pico SDK configuration.

## Memory layout

### Pico 1 (264 KB SRAM)

- ~16 KB stack (both cores)
- ~12 KB line buffer (16 scanlines $\times$ 320 px $\times$ 2 bytes)
- ~30 KB math + UI + drivers
- ~8 KB DMA buffers, system overhead
- → ~190 KB available for application
- 8 MB PSRAM holds: large data structures, optional full framebuffer, CAS pool.
- The MicroPython GC heap is **not** in PSRAM: it is a static SRAM array
  (`g_heap` in `src/scripting/micropython_embed.cpp`), 40 KB here and 96 KB
  on the Pico 2 — see `config::kPythonHeapSize`.

### Pico 2 (520 KB SRAM)

- Everything from Pico 1
- + 200 KB optional full framebuffer in SRAM (faster than PSRAM)
- + larger CAS pool, larger MicroPython heap (96 KB), scratch space
- 8 MB PSRAM still used for very large datasets, matrix storage, file buffers.

See `docs/hardware.md` for the canonical hardware spec table.

## File organization within `src/`

Each subdirectory contains:

- One or more `.hpp` / `.cpp` pairs implementing the subsystem.
- A `<subsystem>.hpp` aggregator header (e.g., `platform/platform.hpp`) for callers who want everything from that subsystem.

Example:

```
src/platform/
├── platform.hpp        # Aggregator: includes all of the below
├── display.hpp/cpp
├── keyboard.hpp/cpp
├── storage.hpp/cpp
├── psram.hpp/cpp
├── audio.hpp/cpp
└── system.hpp/cpp
```

Application code typically `#include "platform/platform.hpp"` once and uses everything. Tight inner loops can include just what they need.

## Error handling

No exceptions (`-fno-exceptions`). Functions that can fail return:

- `bool` if "succeeded or didn't" is sufficient.
- `std::optional<T>` for "result or nothing."
- A custom result struct (e.g., `EvalResult { bool ok; double value; const char* error; }`) when both success and error details are needed.

`assert()` is allowed for invariant violations during development. In release builds (`-DNDEBUG`), asserts compile out.

Stack-allocated buffers are preferred over heap allocation. Where dynamic sizing is required, use the appropriate pool allocator (`platform::Psram::alloc`, `math::ExprPool::alloc`, `render::layout_pool`).

## Logging

`printf` over USB serial is the development logging channel. Wrap in macros:

```cpp
#ifdef NDEBUG
    #define LOG_DEBUG(...) ((void)0)
#else
    #define LOG_DEBUG(...) printf("[D] " __VA_ARGS__)
#endif

#define LOG_INFO(...)  printf("[I] " __VA_ARGS__)
#define LOG_ERROR(...) printf("[E] " __VA_ARGS__)
```

`LOG_DEBUG` compiles out in release builds; `LOG_INFO` and `LOG_ERROR` are always present.

To monitor logs from a Mac:

```bash
screen /dev/tty.usbmodem* 115200
```

(Exit with `Ctrl-A` `K`.)
