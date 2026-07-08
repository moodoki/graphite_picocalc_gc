# Worklog

Running log of build sessions. Newest entries at the top of each section. This file is the
session-surviving source of truth for "where are we and what's next" — read it first when
resuming work. Task status checkboxes live in `docs/phases/phase1-plan.md` (and the Phase 0
checklist in `docs/phases/phase0-prep.md`); this file carries the narrative: what was done,
what was decided, what's blocked, and what to pick up next.

Conventions:

- One entry per work session or checkpoint, headed by date + summary.
- `HW-PENDING`: implemented and building, but acceptance requires real PicoCalc hardware
  that automated sessions don't have. These accumulate in the table below and get cleared
  manually by the developer.
- Decisions go to `docs/notes/decisions.md`; this log only references them.

---

## Current status

- **Phase**: 1, milestone 1 (bootstrap) CODE COMPLETE → milestone 2 (calculator core) next
- **Next up**: task 2.1 — vendor tinyexpr++, `math::Engine`
- **Both boards build**: yes (`./scripts/build-all.sh` → diagnostics-screen firmware)

## HW-PENDING verification queue

Flash `build/pico/picocalc_graphcalc.uf2` (or `build/pico2/...`) — it boots to a
diagnostics screen that exercises everything below at once.

| Item | Since | What to check on hardware |
|------|-------|---------------------------|
| 1.3 Display | 2026-07-08 | Text + RGBW color bars render; bar colors in R,G,B,W order (checks 565→666 conversion + BGR order) |
| 1.4 Keyboard | 2026-07-08 | Typing updates "Keys seen"/"Last key"; events also on USB serial |
| 1.5 SD card | 2026-07-08 | With FAT32 card inserted: "SD card: OK (file r/w verified)" |
| 1.6 PSRAM | 2026-07-08 | "PSRAM: OK (1KB r/w verified)" |
| 1.7 Renderer/DMA | 2026-07-08 | Frame counter ticks smoothly; no tearing/hangs (dual-core FIFO + DMA) |
| Backlight | 2026-07-08 | Screen visibly lit (set_backlight(200) via STM32 reg 0x05 — unverified register) |

---

## 2026-07-08 — Session 1: environment + skeleton + Phase 0 start

**Done:**

- Verified/repaired host toolchain. Quirk: Homebrew *formula* `arm-none-eabi-gcc` lacks
  newlib (`nosys.specs` link failure); the working compiler is ArmGNUToolchain 15.2.rel1
  from the `gcc-arm-embedded` cask. Documented in `docs/dev-environment.md`, AGENTS.md.
- Pico SDK 2.2.0 + pico-examples checked out in-repo (gitignored). picotool 2.3.0 via brew.
- Extracted the project skeleton package; adapted docs to this host; fixed build-dir naming
  (`build/pico`, `build/pico2`), pinned SDK 2.2.0 in CI, scoped C++-only compile flags.
- Initial commit `f76d10c`. Both boards build the blink stub to .uf2.

**Phase 0 status vs checklist:** 0.1.1–0.1.5 done (verified via hello_serial + skeleton
builds), 0.1.6 HW-PENDING, 0.2.* done, 0.4.* done, 0.5 done, 0.6.1/0.6.2/0.6.5 done.
Remaining: 0.3 (vendor drivers), 0.6.3 (optional CLAUDE.local.md — skipped, developer's
call), 0.7 (remote push — no remote configured yet).

**Next:** task 0.3 — clone Coyote OS, vendor `lcdspi/ i2ckbd/ rp2040-psram/ pwm_sound/`,
vendor FatFs R0.15a, record SHAs + licenses.

### Checkpoint: Phase 0 complete (2026-07-08)

- Vendored from Coyote OS `e86cf36d` (2026-02-05): `lcdspi/` (incl. font1/battery fonts),
  `i2ckbd/`, `rp2040-psram/` (MIT, upstream polpo; examples dropped), `pwm_sound/`, plus
  `coyote_reference/` (config.h SD pinout, keyboard_definition.h scan codes — reference only).
  GPL-2.0 text kept as `drivers/LICENSE.coyote-os`; noted GPL implication in dependencies.md.
- Vendored FatFs R0.15a from elm-chan.org (`ff.c`, `ffsystem.c`, `ffunicode.c`, stub
  `diskio.c`; SD SPI glue to be written in task 1.5).
- Phase 0 checklist all [x] except 0.1.6 (HW-PENDING) and 0.6.3/0.7 ([s], developer's call).
- Not yet in build: drivers are intentionally NOT in CMakeLists.txt (per 0.3.4) —
  integrated incrementally in tasks 1.3–1.6.
- Useful discovery: Coyote OS uses tinyexpr (C) and pico-vfs as submodules; we'll use
  tinyexpr++ per spec (task 2.1) and plain FatFs instead of pico-vfs.

### Checkpoint: Milestone 1 (bootstrap) code complete (2026-07-08)

Tasks 1.1–1.9 all [x] in phase1-plan.md; both boards build the diagnostics firmware.
Decisions D6–D9 recorded (RGB666 wire format, async keyboard, FatFs LFN, interim font).

Layer map as built:
- `src/platform/`: display (565→666 push, DMA), keyboard (async poll SM), sd_card
  (own SD SPI driver) + sd_diskio (FatFs glue) + storage (FatFs API), psram (bump
  allocator over PSRAM addresses), system (battery via STM32), platform::init().
- `src/gfx/`: framebuffer (strip ping-pong on Pico 1 / full FB on Pico 2, clipped
  primitives, core-1 display service over multicore FIFO), font (UTFT-format).
- `src/ui/`: Screen base + fixed-depth ScreenManager.
- `src/main.cpp`: core dispatch + DiagScreen (self-tests for SD/PSRAM, key echo).

Notes / known limitations:
- **Full-frame push is ~98 ms @ 25 MHz SPI** (3 B/px wire format) → ~10 fps if the
  whole screen redraws every frame. Fine for milestone-1 accept ("text visible"),
  but the spec's 30 fps target needs dirty-rect updates and/or SPI overclock —
  planned lever for task 5.6. Milestone 2+ UI should avoid full-screen redraws.
- Overclock constant (`config::kOverclockHz`) intentionally NOT applied yet.
- `set_backlight` uses STM32 reg 0x05 (standard PicoCalc fw) — not in the vendored
  driver, needs HW confirmation.
- lint.sh not run: clang-tidy unavailable (Homebrew `llvm` not installed — ~1.5 GB;
  developer's call). clang-format installed (v22) and applied; line width 100.
- Vendored driver C files emit warnings under our -Wall/-Wextra/-Wpedantic (they
  compile as part of our target via INTERFACE libs). Cosmetic; suppress later if
  it drowns signal.
