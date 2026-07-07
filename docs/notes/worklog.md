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

- **Phase**: 0 complete (except HW smoke test) → Phase 1 milestone 1 (bootstrap) starting
- **Next up**: task 1.2 (driver CMake integration), then 1.3 Display wrapper
- **Both boards build**: yes (`./scripts/build-all.sh`, blink stub)

## HW-PENDING verification queue

| Item | Since | What to check on hardware |
|------|-------|---------------------------|
| 0.1.6 blink smoke test | 2026-07-08 | Flash `build/pico/picocalc_graphcalc.uf2` (and pico2) to bare Pico modules; LED blinks |

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
