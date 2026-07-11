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
| 1.3 | `platform::Display` wrapper | 8 | [x] | RGB666 wire format (D6). **HW-verified 2026-07-10** (sync core-0 push, D10) |
| 1.4 | `platform::Keyboard` wrapper | 6 | [x] | Non-blocking poll (D7). **HW-verified 2026-07-10** (I2C timeout fix, D10) |
| 1.5 | `platform::Storage` wrapper (FatFs) | 4 | [x] | Own SD SPI driver + diskio glue; LFN (D8). HW-PENDING (needs FAT32 card) |
| 1.6 | `platform::Psram` wrapper | 3 | [x] | Word r/w **HW-verified**; bulk path hangs on HW, quarantined (D10) |
| 1.7 | Line-buffer renderer + DMA on core 1 | 8 | [x] | Strip render **HW-verified**; runs synchronously on core 0, dual-core/DMA deferred (D10) |
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
| 3.1 | `TextNode`, `HBoxNode` | 4 | [x] | Baseline-aligned HBox; tagged-union nodes (no vtables, RTTI-free) |
| 3.2 | `FractionNode` (stacked fraction) | 6 | [x] | Simple-operand heuristic (D2). Host tests assert structure/size |
| 3.3 | `SuperscriptNode` | 4 | [x] | Right-assoc; raised by base_h/2 (single font size — no 75% shrink) |
| 3.4 | `ParenNode` with auto-scaling | 4 | [x] | Glyph parens for <=1 line; stroked tall parens otherwise |
| 3.5 | `LayoutBuilder` parser | 8 | [x] | Recursive descent; 21 host structural tests. SqrtNode deferred per spec |
| 3.6 | Integrate renderer into `HomeScreen` | 4 | [x] | History exprs render 2D; results stay plain text. HW-PENDING |
| 3.7 | Pool allocator for layout nodes | 2 | [x] | 8 KB bump pool; graceful text fallback on exhaustion |

**Acceptance**: expressions in history render as 2D typeset math.

**Subtotal**: ~32 hrs

---

## Week 7–8: Graphing

| # | Task | Est. hrs | Status | Notes |
|---|------|---|---|---|
| 4.1 | `YEditorScreen` | 6 | [x] | Y1..Y7 list, inline edit, enable checkbox, auto-enable on entry |
| 4.2 | `GraphScreen` — basic plot | 8 | [x] | Column cache via compile-once/eval-many (engine.compile). HW-PENDING |
| 4.3 | Multi-function graphing with color palette | 3 | [x] | 7-color palette in graph_model; all enabled slots plotted |
| 4.4 | Grid lines and axis labeling | 4 | [x] | Grid at Xscl/Yscl; solid axes. Numeric tick labels deferred to 5.x |
| 4.5 | Discontinuity detection | 2 | [x] | Skip line joins jumping >50% of viewport height |
| 4.6 | Trace cursor | 6 | [x] | L/R moves 1px, U/D switches function, (x,y) at bottom (D3). HW-PENDING |
| 4.7 | Zoom in/out/fit/standard/trig presets | 4 | [x] | F2/F3 in/out; S/T = standard/trig presets. ZFit deferred (needs y-range scan) |
| 4.8 | `WindowScreen` | 4 | [x] | 6 editable fields; changes replot on ESC. HW-PENDING |
| 4.9 | Persist Y-functions and window to SD | 2 | [x] | yfuncs.txt (TSV) + window.dat (binary). HW-PENDING |

**Acceptance**: full function graphing with trace and zoom.

**Subtotal**: ~39 hrs

---

## Week 9–10: Polish and integration

| # | Task | Est. hrs | Status | Notes |
|---|------|---|---|---|
| 5.1 | Status bar (mode indicators, title) | 3 | [x] | Shared `ui::draw_status_bar` (title + RAD/DEG + FLT/FIX/SCI + 2nd/A). Used on all screens |
| 5.2 | Softkey bar integration across all screens | 3 | [x] | Shared `ui::draw_softkeys` (6 cells). Home/graph/mode use it |
| 5.3 | Mode screen (angle, display format) | 3 | [x] | `ModeScreen`: angle, FLOAT/FIX/SCI, fix digits, reboot-to-bootloader. F4 from Home |
| 5.4 | Error handling (division by zero, syntax) | 3 | [x] | 1/0→Inf, 0/0→NaN, syntax→"Syntax error" in red; host-tested. No crashes |
| 5.5 | Expression recall (UP key on empty input) | 2 | [x] | UP on empty input recalls last expr; else scrolls history |
| 5.6 | Performance profiling + optimization | 4 | [x] | Recompute 15-17 ms on HW (target met). Dirty-band partial rendering (D13) HW-verified 2026-07-11: typing pushes ~28 rows not 320, feels instant, no artifacts |
| 5.7 | Test on both Pico 1 and Pico 2 hardware | 4 | [x] | Pico 1 fully verified 2026-07-11 (three HW rounds). Pico 2 brought up 2026-07-11/12: full-FB display path works; cold-boot PSRAM/SD rail settle fixed via D14 late-init, verified cold. Functional spot-check queued (worklog) |
| 5.8 | UF2 loader compatibility (clean reboot) | 2 | [x] | Mode screen "Reboot to bootloader" → `reset_usb_boot` (BOOTSEL) |
| 5.9 | README, build instructions update | 2 | [x] | Features, host-tests, usage, flashing-from-firmware documented |

**Acceptance**: release-quality Phase 1 firmware.

**Subtotal**: ~26 hrs

---

## Cumulative tracking

| Week range | Subtotal | Cumulative | Status |
|------------|---------|------------|--------|
| Phase 0 (prep) | ~12 hrs | 12 | [x] |
| Week 1–2: Bootstrap | ~41 hrs | 53 | [x] (code complete; HW verification pending) |
| Week 3–4: Calculator core | ~34 hrs | 87 | [x] (code complete; host tests green; HW verification pending) |
| Week 5–6: Math renderer | ~32 hrs | 119 | [x] (code complete; host tests green; HW verification pending) |
| Week 7–8: Graphing | ~39 hrs | 158 | [x] (code complete; host tests green; HW verification pending) |
| Week 9–10: Polish | ~26 hrs | 184 | [x] (code complete; 5.7 HW test + 5.6 profiling numbers pending hardware) |

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

- [x] All week 1–10 tasks are `[x]` complete or `[~]`/`[!]` explicitly deferred.
- [x] `./scripts/build-all.sh` produces `.uf2` files for both Pico 1 and Pico 2. *(Both boot and run on hardware; Pico 1 fully verified, Pico 2 brought up incl. D14 cold-boot fix.)*
- [~] Lint (`./scripts/lint.sh`) — clang-format applied repo-wide; clang-tidy not run (Homebrew `llvm` not installed on this host — developer's call).
- [x] Hardware test: power-cycle → home screen → `2+3*sin(pi/4)` → `F3` graph `sin(x)` with trace/zoom. **Verified on Pico 1 2026-07-11.**
- [x] History, variables, and Y-functions survive a power cycle. **Verified on Pico 1 2026-07-11** (SD self-test r/w OK, history persists).
- [x] Phase 2 spec exists in `docs/phases/phase2-spec.md` (imported + reconciled 2026-07-11).
- [x] Decisions D1–D5 recorded in `docs/notes/decisions.md` (plus D6–D13 for implementation choices).

**Phase 1 declared done 2026-07-12** — retro at `docs/notes/phase1-retro.md`.
(Two small KIV checks ride along in the worklog queue: charging color at <95%
battery, and a Pico 2 functional spot-check.)
