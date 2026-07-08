# Phase 1: Implementation Plan

**Companion to**: [phase1-spec.md](phase1-spec.md) (the design contract).

This is the active working document for Phase 1 — task list, status tracking, and per-task notes. The spec defines *what* to build; this plan tracks *progress* on building it. Update this document as tasks are completed, not the spec.

**Phase 1 goal**: a daily-driver scientific calculator with basic graphing on the PicoCalc, working on both Pico 1 H and Pico 2 H.

**Total estimate**: ~172 hours, ~10 weeks part-time at ~20 hrs/week.

**Prerequisite**: [Phase 0](phase0-prep.md) checklist complete.

---

## Status legend

- [ ] **Not started**
- [~] **In progress**
- [x] **Complete**
- [!] **Blocked** (notes required)
- [s] **Skipped/deferred** (notes required)

---

## Week 1–2: Bootstrap (HAL + build system)

| # | Task | Est. hrs | Status | Notes |
|---|------|---|---|---|
| 1.1 | Repo + dual-board CMake build | 3 | [x] | 2026-07-08, commits `f76d10c`..`d62d539` |
| 1.2 | Vendor Coyote OS C drivers | 2 | [x] | Vendored `e86cf36d`; CMake integration in `d62d539` |
| 1.3 | `platform::Display` wrapper | 8 | [x] | RGB666 wire format — see D6. HW-PENDING |
| 1.4 | `platform::Keyboard` wrapper | 6 | [x] | Non-blocking poll — see D7. HW-PENDING |
| 1.5 | `platform::Storage` wrapper (FatFs) | 4 | [x] | Own SD SPI driver + diskio glue; LFN — see D8. HW-PENDING |
| 1.6 | `platform::Psram` wrapper | 3 | [x] | Bump allocator over PSRAM addresses (not pointers). HW-PENDING |
| 1.7 | Line-buffer renderer + DMA on core 1 | 8 | [x] | Strip ping-pong on Pico 1, full FB on Pico 2. HW-PENDING |
| 1.8 | `gfx::Font` with 8$\times$16 font | 4 | [x] | Interim 8x12 font1 — see D9 |
| 1.9 | Basic `ScreenManager` | 3 | [x] | Fixed-depth stack; DiagScreen demo in main.cpp |

**Acceptance**: PicoCalc boots, shows text, responds to keyboard, reads/writes SD card. Both `.uf2` files functional on hardware.

**Subtotal**: ~41 hrs

---

## Week 3–4: Calculator core

| # | Task | Est. hrs | Status | Notes |
|---|------|---|---|---|
| 2.1 | Integrate tinyexpr++ into `math::Engine` | 4 | [x] | Used C tinyexpr `4a7456e` (zlib), not ++; simpler C ABI, `TE_POW_FROM_RIGHT`. Host tests green |
| 2.2 | Register extended functions (trig, log, nCr, etc.) | 4 | [x] | ln/log10/nCr/nPr/fac/rand/round/min/max/deg/rad; `!` rewritten to `fac()` in preprocess |
| 2.3 | Angle mode (degree/radian) | 2 | [x] | `math::set_angle_mode`; F4 toggles; trig wrappers convert |
| 2.4 | `format_number()` | 3 | [x] | int / 10-sig-fig / sci; host tests cover edge cases |
| 2.5 | `HomeScreen` with input line + history | 10 | [x] | Ring buffer 50, right-aligned results. HW-PENDING |
| 2.6 | Variables A–Z + Ans | 3 | [x] | +theta; store op `->` (D1); case-insensitive |
| 2.7 | Persist history + variables to SD card | 3 | [x] | Plaintext TSV + binary vars (D4). HW-PENDING |
| 2.8 | Input line editing (cursor, insert, delete) | 5 | [x] | `ui::InputLine`: cursor, backspace, del, home, h-scroll |

**Acceptance**: a working scientific calculator. Type expressions, get results, use variables.

**Subtotal**: ~34 hrs

---

## Week 5–6: Natural math renderer (basic)

| # | Task | Est. hrs | Status | Notes |
|---|------|---|---|---|
| 3.1 | `TextNode`, `HBoxNode` | 4 | [ ] | |
| 3.2 | `FractionNode` (stacked fraction) | 6 | [ ] | |
| 3.3 | `SuperscriptNode` | 4 | [ ] | |
| 3.4 | `ParenNode` with auto-scaling | 4 | [ ] | |
| 3.5 | `LayoutBuilder` parser | 8 | [ ] | |
| 3.6 | Integrate renderer into `HomeScreen` | 4 | [ ] | |
| 3.7 | Pool allocator for layout nodes | 2 | [ ] | |

**Acceptance**: expressions in history render as 2D typeset math.

**Subtotal**: ~32 hrs

---

## Week 7–8: Graphing

| # | Task | Est. hrs | Status | Notes |
|---|------|---|---|---|
| 4.1 | `YEditorScreen` | 6 | [ ] | |
| 4.2 | `GraphScreen` — basic plot | 8 | [ ] | |
| 4.3 | Multi-function graphing with color palette | 3 | [ ] | |
| 4.4 | Grid lines and axis labeling | 4 | [ ] | |
| 4.5 | Discontinuity detection | 2 | [ ] | |
| 4.6 | Trace cursor | 6 | [ ] | |
| 4.7 | Zoom in/out/fit/standard/trig presets | 4 | [ ] | |
| 4.8 | `WindowScreen` | 4 | [ ] | |
| 4.9 | Persist Y-functions and window to SD | 2 | [ ] | |

**Acceptance**: full function graphing with trace and zoom.

**Subtotal**: ~39 hrs

---

## Week 9–10: Polish and integration

| # | Task | Est. hrs | Status | Notes |
|---|------|---|---|---|
| 5.1 | Status bar (mode indicators, title) | 3 | [ ] | |
| 5.2 | Softkey bar integration across all screens | 3 | [ ] | |
| 5.3 | Mode screen (angle, display format) | 3 | [ ] | |
| 5.4 | Error handling (division by zero, syntax) | 3 | [ ] | |
| 5.5 | Expression recall (UP key on empty input) | 2 | [ ] | |
| 5.6 | Performance profiling + optimization | 4 | [ ] | |
| 5.7 | Test on both Pico 1 and Pico 2 hardware | 4 | [ ] | |
| 5.8 | UF2 loader compatibility (clean reboot) | 2 | [ ] | |
| 5.9 | README, build instructions update | 2 | [ ] | |

**Acceptance**: release-quality Phase 1 firmware.

**Subtotal**: ~26 hrs

---

## Cumulative tracking

| Week range | Subtotal | Cumulative | Status |
|------------|---------|------------|--------|
| Phase 0 (prep) | ~12 hrs | 12 | [x] |
| Week 1–2: Bootstrap | ~41 hrs | 53 | [x] (code complete; HW verification pending) |
| Week 3–4: Calculator core | ~34 hrs | 87 | [x] (code complete; host tests green; HW verification pending) |
| Week 5–6: Math renderer | ~32 hrs | 119 | [ ] |
| Week 7–8: Graphing | ~39 hrs | 158 | [ ] |
| Week 9–10: Polish | ~26 hrs | 184 | [ ] |

---

## Open decisions to resolve during Phase 1

These are deferred from the spec — make a decision and record it in `docs/notes/decisions.md` when the relevant task starts.

| # | Question | When |
|---|----------|------|
| D1 | Variable store operator: `→`, `=`, `:=`, or dedicated STO key? | Week 3, task 2.6 |
| D2 | Fraction display: always stacked, or only for "simple" operands? | Week 5, task 3.5 |
| D3 | Graph coordinate display position (top vs. bottom of viewport)? | Week 7, task 4.6 |
| D4 | History storage format: plaintext or binary? | Week 3, task 2.7 |
| D5 | `double` vs `float` for graphing on Pico 1 (after profiling)? | Week 7, task 4.2 (revisit week 9) |

---

## Risks (from spec, restated)

- **R1: Natural math renderer complexity** — many edge cases in baseline alignment, nested structures. Mitigation: ship `TextNode + HBoxNode` first (inline expressions work), add fractions/exponents incrementally.
- **R2: Line-buffer renderer constraint** — every UI element must support clipped rendering. Mitigation: consider full-framebuffer mode on Pico 2 first to defer this constraint, then optimize for Pico 1 in week 9.
- **R3: Softfloat performance on Pico 1** — heavy graphing with many functions could lag. Mitigation: budgeted in week 9 task 5.6 for profiling and optimization. Falling back to `float` from `double` for graph eval is the prepared lever.

---

## Per-task workflow

For each task:

1. **Read the relevant section of [phase1-spec.md](phase1-spec.md)** — the spec is the contract. If it's wrong, fix the spec first via a separate commit.
2. **Update task status to `[~]` (in progress)** in this file.
3. **Implement.** Reference patterns from `docs/notes/decisions.md` and the spec.
4. **Run `./scripts/format.sh && ./scripts/lint.sh`.** All warnings must be addressed before marking complete.
5. **Build both targets**: `./scripts/build-all.sh`. Both must succeed.
6. **Test on hardware** for tasks involving HAL or visible UI (most of Phase 1).
7. **Commit** with conventional commit message: `<subsystem>: <imperative summary>`. Examples:
   - `platform: implement Display::clear and fill_rect`
   - `gfx: add line-buffer DMA pipeline on core 1`
   - `math: integrate tinyexpr++ wrapper`
8. **Update task status to `[x]` (complete)** in this file. Add notes if anything was non-obvious.

---

## Phase 1 exit criteria

Before declaring Phase 1 complete:

- [ ] All week 1–10 tasks are `[x]` complete or `[s]` explicitly deferred (with reason).
- [ ] `./scripts/build-all.sh` produces working `.uf2` files for both Pico 1 and Pico 2.
- [ ] Lint (`./scripts/lint.sh`) returns clean.
- [ ] Hardware test: power-cycle the PicoCalc → home screen appears → enter `2+3*sin(pi/4)` → result displays correctly. Press `F3` (graph) → `sin(x)` plots correctly with trace and zoom working.
- [ ] History, variables, and Y-functions survive a power cycle.
- [ ] Phase 2 spec exists in `docs/phases/phase2-spec.md`.
- [ ] Decisions D1–D5 recorded in `docs/notes/decisions.md`.

Once exit criteria are met, write a short retrospective in `docs/notes/phase1-retro.md`: what went well, what didn't, calibration adjustments for Phase 2 estimates.
