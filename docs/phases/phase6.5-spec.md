# Phase 6.5 Spec: Polish — efficiency, latency and latent traps

**Prerequisite phases**: none structurally. Every package below stands on the
code as it is at `1320862` (branch `fix/boot-wedge-diagnostics`, 2026-08-30).
Packages that touch `src/platform/boot_trace.cpp` or `src/main.cpp`'s bring-up
should wait for D101's soak to merge.

**Scope**: A whole-codebase review turned into **independent work packages**.
It covers three things: hardware resources (static SRAM, stack, PSRAM traffic,
SD I/O, idle power), user-visible latency (key-to-pixel, redraw, table
scroll, persistence stalls), and code patterns that are correct today but
make the next change likely to break something. It also covers what the
current structure costs the features that are planned but not built (6.1,
6.2, 6.3, the rest of 6.4, and the open issue backlog).

**Status**: **PROPOSED — a draft, not a commitment.** Nothing here is agreed
or scheduled. It is built so that **any package can be taken alone, in any
order, or dropped**. Where one package makes another cheaper, §2 says so, but
no package requires another.

**Sequencing (developer call, 2026-09-15): do this before further feature
work.** That means before 6.4.5 (SDL), 6.3, the 6.1 and 6.2 candidates, and
the open `type:feature` issues. Three reasons. **Headroom:** Pico 1 has ~14 KB
of static SRAM left, and every feature spends from it. **Latency:** several
issues (#18, #21, #41, #49) would build on screens whose redraw and dirty-band
behaviour this phase fixes (6.5.3, 6.5.9). **Traps:** the traps here get more
expensive with every owner a feature adds to a shared buffer or a persisted
format (6.5.1, 6.5.2, 6.5.6). The packages stay individually adoptable. The
order is a priority over feature work, not a requirement to finish all of
6.5 first.

**Branch note**: written against `fix/boot-wedge-diagnostics` at `1320862`,
which carries D101's `boot_trace` work that `main` does not have yet. Tasks
citing `boot_trace.cpp` or the new bring-up lines in `main.cpp` (6.5.5.2,
6.5.5.3) apply once that branch merges.

**Sub-phase numbering**: dotted. Polish was never part of Phase 6's goals; it
turned up once the feature set stopped moving. 6.1 and 6.2 are reserved
candidates, 6.3 is drafted, 6.4 is in progress, so this is 6.5. Packages are
`6.5.N`, tasks `6.5.N.m`.

**How this was produced**: seven parallel reviewers, one per slice (platform
and display; evaluator core; numeric libraries and CAS; graph and render; UI
and apps; scripting, host build and CI; bug history against current code).
Each finding cites code that was read, not inferred. The findings that carry
the most weight were re-checked against the source before this was written.
Two findings were dropped for contradicting the recorded decisions (§5).
**Line numbers are as of `1320862`** and will drift.

---

## 1. What the review found, in one page

**Measured baseline** (`scripts/size-report.sh` against the 2026-08-30
builds, whose Pico 1 figure matches D101's post-change number exactly):

| | static SRAM | free | largest stack frame |
|---|---|---|---|
| Pico 1 | 246,864 / 262,144 B | **~14 KB** | `eigen_core` 1,248 B |
| Pico 2 | 499,432 / 524,288 B | ~24 KB | — |

Several recorded comfort margins predate this number and are stale. D35 sized
the stat-plot caches against "~76 KB headroom". That number is now 14 KB.

**Five themes recur across slices**, and the packages are grouped by them
rather than by directory:

1. **Invariants that live only in comments.** Examples: the `kCompute`
   arena's one-owner rule, `Framebuffer::bind()`, `io_scratch`,
   `scratch_pixels()`, the "valid until next `run()`" `Value` lifetime, and
   the watchdog register, which now has three uncoordinated owners. All are
   true today. None is checked, and each phase adds owners.
2. **D53's fix was applied to one call site, not to the pattern.** Loops
   calling `get()`/`cget()`/`set()` per element on PSRAM arrays are still
   live in at least eight places. One of them (`dot`/`norm`) was written
   after D53, three lines below a correct chunked loop.
3. **Pico 1's strip renderer multiplies `render()` cost by up to 40.**
   `kStripHeight` is 8, so a full frame is 40 `render()` calls. D35 found and
   fixed this for stat plots. The home screen's history, the graph's function
   replay and the text editor were never audited for it.
4. **Everything persists synchronously, in place, on every keypress.** It
   runs on the UI thread and has no atomic replace. That means a stall on a
   slow card and a torn file on power loss, and the loaders' fallback turns a
   torn file into silently lost state.
5. **Measurement tools exist but do not gate anything.** Every SRAM, stack
   and timing number in `decisions.md` was taken by hand.
   `size-report.sh`, clang-tidy, `check-text-fits.py` and the host
   interpreter harness all exist, and CI runs none of them as a gate.

**If only four things get done**, take these. All are S effort and each
removes a real user-facing failure:

- **6.5.7.1** — the text editor's `F3` (load) and `F4` (new) discard an
  unsaved buffer without asking. `ESC` asks.
- **6.5.3.1** — typing one character on the home screen rebuilds every
  visible history entry's layout 4 times, and a full redraw does it 40 times.
- **6.5.2.1–.3** — the per-element PSRAM loops of D53's class that are still
  live, including a new one.
- **6.5.8.1** — typing `1e300` into a WINDOW field reaches undefined
  behaviour in `Viewport::px_y` on the next redraw.

---

## 2. Package map

| pkg | theme | tasks | effort | boards | enables / pairs with |
|---|---|---|---|---|---|
| **6.5.0** | Budgets and static gates in CI | 5 | ~10 h | both | every package's verification |
| **6.5.1** | Persistence durability and write-behind | 5 | ~14 h | both | 6.5.10.4 (multi-block SD) |
| **6.5.2** | PSRAM access discipline (D53 sweep) | 6 | ~10 h | pico1 | #24 root cause |
| **6.5.3** | Strip-render and dirty-band latency | 7 | ~16 h | pico1 | #49 |
| **6.5.4** | Compute latency — algorithms and caching | 6 | ~16 h, +L stretch | both | #15 / #16 decision |
| **6.5.5** | Liveness — interrupt, watchdog, stack gates | 4 | ~12 h | both | #54, #55 |
| **6.5.6** | Comment invariants become debug asserts | 7 | ~14 h | both | 6.5.2, #14 |
| **6.5.7** | Nothing disappears silently | 4 | ~6 h | both | #47, 6.3 |
| **6.5.8** | Input validation and numeric edges | 4 | ~7 h | both | — |
| **6.5.9** | Shared UI components that unblock issues | 4 | ~14 h | both | #18, #21, #41, #49, #51 |
| **6.5.10** | SRAM reclaim, idle power, SD throughput | 5 | ~14 h | pico1 / both | 6.2, 6.3 |
| **6.5.11** | Behavioural tests at the seams | 5 | ~16 h | host | 6.5.1, 6.5.5 |
| **6.5.12** | Readiness notes for planned features + doc drift | 6 | ~6 h | — | 6.1, 6.2, 6.3, 6.4.5, #20, #22 |

**Total ~155 h** if everything were taken, which is not the expectation.

**Soft ordering, not gates:**

- 6.5.0 first if anything is taken. Its timing probe (6.5.0.4) gives the
  latency packages a "before" number.
- 6.5.11.1 (persistence round-trip tests) before 6.5.1's write-path changes.
- 6.5.6.1 (arena owner guard) makes 6.5.2's chunked rewrites safer to review,
  because several of them borrow `scratch::compute_region()`.
- 6.5.5.1 and 6.5.5.2 share the same checkpoint sites. Doing them together
  costs about one and a half times one of them, not twice.

---

## 3. Packages

Effort: **S** < 2 h, **M** half a day to 2 days, **L** multi-session.
"Verify" names the instrument, since this project measures rather than
predicts.

### 6.5.0 Budgets and static gates in CI

D69 is the warning here: the headroom tool was silently wrong for months.
D45, D47 and D48 are the other half: frame regressions were invisible until a
board crashed. Both are only caught today by a script nobody is forced to run.

| id | task | eff | done when |
|---|---|---|---|
| 6.5.0.1 | **Size gate** in `.github/workflows/build.yml`, per board. Run `size-report.sh`, fail below a free-SRAM floor (proposal: 8 KB Pico 1, 12 KB Pico 2), and upload the report as an artifact | S | A throwaway 8 KB static array fails the job |
| 6.5.0.2 | **Stack-frame baseline.** Commit the "frames >= 512 B" list and fail on any new symbol over a ceiling (proposal: 1,536 B; largest today is 1,248 B). This is the static half of D47/D48, and `CMakeLists.txt:187-196` already says so | M | A 2 KB stack local in any `src/` function fails CI; a legitimate addition is a one-line baseline bump |
| 6.5.0.3 | **Finish the lint job.** Add `scripts/check-text-fits.py` (no toolchain needed) and clang-tidy to CI's `lint` job, which today runs clang-format only (`build.yml:76-98`). 6.4.6 names clang-tidy as still missing. Dry-run first to size the backlog; land it non-blocking, then flip | S+M | Both run on every PR; clang-tidy is blocking once the backlog is zero |
| 6.5.0.4 | **Latency probe.** A `PICOCALC_TIMING` build option that logs key-event → last-SPI-push-complete in µs over serial, driven by the Phase 5.1 injection harness. Record one baseline table: home keystroke, Notepad keystroke, graph zoom, table 1-row scroll, WINDOW field commit, both boards. **Every latency task below cites this table** | M | `docs/notes/measurements/phase6.5/baseline.md` exists with both boards |
| 6.5.0.5 | **Sanitised host job.** `-fsanitize=address,undefined` variant of `host-tests.sh` and `graphite-shot`, non-blocking at first, with vendored C blocklisted if noisy. Would have flagged the `Viewport` cast (6.5.8.1) and the `%s` read (6.5.7.4) | S | Job runs; any finding is filed or fixed |

### 6.5.1 Persistence durability and write-behind

`platform::Storage::write_file` (`storage.cpp:86-98`) opens with
`FA_CREATE_ALWAYS` and writes in place. So does `append_file` (`:100-112`),
and the host twin at `storage_posix.cpp:182-200`. Every format inherits this.
A power loss mid-write leaves a truncated file, and the loader's validation
turns that into defaults. That is **the same symptom as D101's reports**
(state lost after a power cycle), from a far more ordinary cause.

Separately, ~39 call sites save on the keypress itself. `GraphState::save()`
is a ~6.5 KB synchronous write. The zoom presets
(`graph_model.cpp:136-199`), Y= enable/clear/shade (`y_editor.cpp:39-62`),
the param, polar, seq and table-setup editors, and **`WindowScreen::commit_edit`
all save once per field committed** (`window_screen.cpp:89-98`). The worklog
records FatFs taking seconds on a slow card (`worklog.md:403`).

| id | task | eff | done when |
|---|---|---|---|
| 6.5.1.1 | **Atomic replace.** `write_file` writes `path~` then renames over `path`. D55's rename refuses to clobber, so add an explicit replacing variant rather than weakening D55. Leave `append_file` (history) alone | M | Host test: a write killed between the temp write and the rename leaves the old file loadable. HW: pull power during a graph save 10x, 0 lost states |
| 6.5.1.2 | **Write-behind for graph state.** Mutations set `graph_state_dirty_`; the idle path flushes after ~500 ms without input, and on screen exit or sleep. Same mechanism for `WindowScreen` (save on deactivate, not per field). D26 already named write-behind as a revisit trigger | M | 6.5.0.4's "graph zoom" and "WINDOW field commit" rows drop by the measured write cost; a key storm of 20 zooms produces 1 write |
| 6.5.1.3 | **Flush-on-sleep and flush-on-power-key hook**, so write-behind cannot lose more than the last ~500 ms of edits | S | Soft-sleep with a dirty flag set writes before dimming |
| 6.5.1.4 | **Decide and record** the loss window write-behind introduces, as a D-number. Today an edit is lost only if the write itself is torn; after 6.5.1.2, the last half-second is at risk. With 6.5.1.1 in place the trade is favourable. Without it, 6.5.1.2 alone makes durability worse | S | Decision recorded |
| 6.5.1.5 | **One save scheduler.** Replace per-screen `save_*()` calls with `persist::mark_dirty(Target)` so the next screen cannot reintroduce the synchronous pattern | M | `grep -n 'save_graph_state\|save_window'` in `src/apps/` returns only the scheduler |

Pairs with 6.5.11.1, which should land first so the rewrite is covered, and
6.5.10.4.

### 6.5.2 PSRAM access discipline (the D53 sweep)

D53 proved per-element `Array::get()` on PSRAM-backed arrays intermittently
returns a single-bit-flipped value on Pico 1. It proved chunked
`read_range`/`write_range` is clean. It fixed `format_list` only, and #24
tracks the rest. This review found these sites still live:

| id | site | why it matters | eff |
|---|---|---|---|
| 6.5.2.1 | `unified_vm.cpp:1374-1386` — `dot`/`norm` loop `a.get(i) * b.get(i)` | **Written after D53**, three lines below a correct `read_range` cross product | S |
| 6.5.2.2 | `list_ops.cpp:202-211` `csum`, `:315-341` `copy` (complex branch), `:356-372` `copy_complex` | Complex arrays are **PSRAM-only at any length** (`array.hpp:12-14`), so `sum`/`mean` of any complex list takes this path. `copy_complex` is the list-widen path (`sqrt(l1)` with a negative element, `unified_vm.cpp:602-615`), tested only at 2 elements (`test_unified.cpp:939`). `matrix.cpp:485-506` does the same promotion chunked | S |
| 6.5.2.3 | `array_format.cpp:56-90` `format_matrix_impl` — two per-cell passes | D53's own "not fixed" row | S |
| 6.5.2.4 | `unified_vm.cpp:880-897` `eval_seq` writes `out->set(i, v)` per element | D52 named this ~1.6 µs/element as **"the only cheaply fixable part"** of M6's regression. Buffer 32–64 and `write_range`. Latency, not just exposure | S |
| 6.5.2.5 | `list_editor.cpp:74,161`, `matrix_editor.cpp:74,166` | Editors read the visible window per cell; read the window in one `read_range` per render-prep, **not in `render()`** | S |
| 6.5.2.6 | **Guard the pattern.** Either a `ChunkReader`/`ChunkWriter` helper that makes the right loop the short one, plus a debug-build counter that logs when one PSRAM array sees more than N consecutive `get()` calls; or a lint grep in CI for `.get(`/`.cget(` inside a `for` over `.size()` in `src/math/` | M | — |

**Verify**: the D53 method. Take 30 repetitions on Pico 1 with a fold a
single-element error cannot hide (`sum(x)-expected`, never `sum(x)`). Add
host tests at 999 elements with the widen firing late. For 6.5.2.4, run
`scripts/ab-measure.py` on M6 against `docs/notes/measurements/phase5.2/`.

**Not in this package:** the D53 root cause. #24 already names the next step
(log the failing temporary's `psram_addr_` against a clean one). The
DMA-contention theory is retired (§5).

### 6.5.3 Strip-render and dirty-band latency (Pico 1)

`render()` runs once per 8-row strip inside the dirty band
(`framebuffer.cpp:151-187`, `config.hpp:80`), so a full frame is 40 calls.
`screen.hpp:20-22` tells screens to clip "if profiling demands it". D35 is the
precedent: identical-shaped waste in stat plots, measured as sluggish, fixed
with a clip gate. **Measure each row with 6.5.0.4 before and after. None of
these magnitudes is measured yet.**

| id | task | eff | done when |
|---|---|---|---|
| 6.5.3.1 | **Home screen history clip gate.** `HomeScreen::render()` (`home_screen.cpp:1058-1134`) calls `render::build_layout()` for every visible history entry on every strip. A keystroke's dirty band is `[268, 300)` (`:142-147`), 4 strips, **all outside the history region `[16, 268)`**. Wrap the history loop in `if (fb.clip_y0() < kInputY)` | S | `build_layout` calls per keystroke: 0 (host counter). Home keystroke row improves |
| 6.5.3.2 | **Per-entry layout reuse** across the strips of one frame, rebuilt only when an entry changes. Only if 6.5.3.1's numbers show full-frame redraws (Enter, scroll) still hurt | M | Full home redraw time within ~10% of an empty-history redraw |
| 6.5.3.3 | **Text editor dirty bands.** `TextEditorWidget` (Notepad and the Python editor, the two highest-typing-volume screens) never calls `track_dirty()`, so every keystroke is a full frame, ~146–200 ms on Pico 1 (D13). Invalidate the touched line; full frame only on scroll or load. The pattern is already correct in `list_editor.cpp:85-99` | M | Notepad keystroke row <= 1/4 of baseline; cursor, gutter and status line still correct |
| 6.5.3.4 | **Solver and Settings actually use their dirty tracking.** Both call `track_dirty()` then `invalidate_all()` on every key (`solver_screen.cpp`, 9 sites; `settings_screen.cpp`, 5 sites). Scope to the changed row(s), old and new selection | S | Row-scoped invalidates; visual check on both screens |
| 6.5.3.5 | **Graph function replay clip.** `draw_function` (`graph_screen.cpp:820-829`, loop at `:1480-1493`) replays 320 cached columns x up to 7 slots on each of the 40 strips. Cache each slot's pixel y-extent at `recompute_function()` time (28 B total) and skip slots, and column runs, that cannot touch `[clip_y0, clip_y1)`. The same idiom is in `layout_render.cpp:33-35`. Stays idempotent: the extent is computed in recompute and only read in render | S | Graph redraw (no resample) row improves; host plot-cache tests unchanged |
| 6.5.3.6 | **Split "redraw" from "resample" dirty in the graph screen.** The axis-label toggle sets `dirty_` (`graph_screen.cpp:1434`), which re-evaluates all 320 columns x all slots (~86.6 ms per #16) just to redraw labels | S | Label toggle does not call `recompute_*` |
| 6.5.3.7 | **Glyph fast path** (`font.cpp:27-59`). A glyph fully inside the clip writes rows directly instead of per-pixel `set_pixel` with 4 bounds checks. **Plus** `static_assert` coupling `display.cpp:47-49`'s `kChunkLines = 4` to `config::kStripHeight`, so a future strip re-measure (D70) cannot silently re-chunk | S | Host PPM byte-identical before and after; text-dense screen row improves |

**Guard for the class:** add a sentence to `screen.hpp`'s `render()` contract
and a host-build debug counter. The counter reports, per frame, `render()`
work done for rows outside the clip, so the next screen gets audited by a test
instead of a testdrive.

### 6.5.4 Compute latency — algorithms and caching

| id | task | eff | done when |
|---|---|---|---|
| 6.5.4.1 | **Table: compile once per column, shift on scroll.** `eval_expr_at` (`table_model.cpp:85-94`) runs compile → eval → free **per cell**, so a regenerate is up to 17 rows x 7 slots = 119 compiles (~0.19 ms each per #16, ~23 ms before any evaluation). A 1-row scroll (`table_screen.cpp:137-151`) regenerates all 17. Compile per column per regenerate, then `memmove` the row window and evaluate only the revealed row. The graph sweep already does compile-once (`graph_screen.cpp:378`) | M | 1-row scroll: <= 7 compiles. Host test: shifted table equals full regenerate, scrolling both directions |
| 6.5.4.2 | **Matrix power by squaring.** `matops::power` (`matrix.cpp:829-843`) multiplies p−1 times. At the editor's own limits (99x99, p=100) that is ~96 M soft-float multiply-adds against ~6.7 M. `c_pow` (`complex.cpp:110-129`) already does square-and-multiply | S | Host test: identical results over a corpus; serial-timed `[A]^100` on 10x10, Pico 1 |
| 6.5.4.3 | **VM dispatch profile** — measurement only. Split M6's ~1.7x/op into switch dispatch (`run_from`, `unified_vm.cpp:1773-1794`) and payload (`scalar_binop` always through `c_pow`, `:453-459`). Compare a minimal-payload program with a catalog-call-heavy one, both boards. **This is the missing input to #15 vs #16**, which D52 says was never profiled | S | A measurement note that either names a fixable hotspot or records the gap as irreducible, discharging one of #15 / #16 |
| 6.5.4.4 | **Med-Med multiplicity** — measurement only. `medmed_fit` (`stats.cpp:851-949`) calls `select_ranks` four times; D23 accepted ~0.7 s for one. Time a 10,000-element PSRAM list; fix (share key bounds across groups) only if it is materially worse than D23's figure | S | Number recorded against D23 |
| 6.5.4.5 | **`factor()` divisor search to sqrt of the constant term** (`factor.cpp:126-149` walks d to \|c0\|, capped at 100,000). Pair d with c0/d. Low value, since the RP2040 has a hardware divider; take it only alongside other CAS work | S | `test_cas` unchanged |
| 6.5.4.6 | **Stretch (L): D10 leg B** — parallel `recompute_function` on a second engine and vars context. Still the one open D10 follow-up. **Preconditions are heavy**: `Engine` and tinyexpr state are process-global singletons, core 1 is the display service, and multicore FIFO is the only sanctioned channel. Scope it only after 6.5.4.3 and the #15/#16 decision, since the evaluator it would duplicate may change | L | Separate spec |

### 6.5.5 Liveness — interrupt, watchdog, stack gates

| id | task | eff | done when |
|---|---|---|---|
| 6.5.5.1 | **ESC reaches long `calc.*` bindings.** `picocalc_py_interrupt_requested()` is polled only in the bytecode hook (`mp_port.c:54-58`) and in `wait_key`/`input`. A script inside `calc.solve`, `calc.inverse`/`rref` on a large matrix, or `calc.graph_integral` cannot be stopped until that C call returns. Add a `CalcInterruptFn` hook shaped like `calc_api_set_stack_hook` (a flag read, never a call into MicroPython, so D74 holds), polled at iteration points in `numeric_solve`, `analyze_integral`, `eigen_core`, `rref` and `inverse`, returning `kCalcInterrupted` → `KeyboardInterrupt`. **Measure first**: if the worst case is under ~150 ms on Pico 1, defer | M | ESC during `calc.inverse` of a 32x32 on Pico 1 returns within ~100 ms |
| 6.5.5.2 | **Watchdog after boot.** `boot_trace_end()` (`boot_trace.cpp:109-115`) disarms the only watchdog, and nothing re-arms it. The PSRAM PIO/DMA waits that D10 and D101 both suspect (`psram_spi.h:126-134,171-255`, no timeout) then run unguarded for the whole session. Re-arm a coarse watchdog, fed from the main loop, the VM hook, and **the same checkpoint sites as 6.5.5.1**. Caution: the RP2040 maximum is ~8.3 s, and a 99x99 soft-float inverse may legitimately exceed that between checkpoints, so bench the worst legitimate gap before choosing the timeout. **After D101's soak merges** | M | An injected hang in `Psram::read` post-boot reboots with a crash record; a 99x99 inverse on Pico 1 does not |
| 6.5.5.3 | **One owner for the watchdog register.** `run_psram_bulk_test()` (`main.cpp:106-107`, `:167-168`) arms at 2 s and then clears `WATCHDOG_CTRL_ENABLE_BITS` directly, underneath `boot_trace`'s 5 s guard. It is harmless only because it runs last in `run_self_tests()`. Route it through a `boot_trace` tighten/restore API. With 6.5.5.2 there would be four stakeholders (fault, boot_trace, bulk test, runtime), so this should come first | S | A hang injected in a hypothetical step after the bulk test still reboots (D101's "an escape hatch never fired is a guess") |
| 6.5.5.4 | **Stack gate for `convert()` via `calc.eval`.** `calc_api.cpp:311-326` gates `solve()` on `kSolveStackNeed`, because not gating was measured to hang (2026-08-15). Three lines later, `unitexpr::substitute` (a 416 B frame, calling `eval_field` → `Engine::compile`, 280 B) has no gate, and `convert()` is absent from the measured table at `:76-93`. Measure top level and one Python function deep with `-DPICOCALC_STACK_PROBE=ON` (D76); add `kConvertStackNeed` if needed | S | Probe numbers recorded in the `:76-93` table; gate added or its absence justified |

### 6.5.6 Comment invariants become debug asserts

All of these compile out of release builds and run in the host suite. The
cost is near zero. The payoff is that the next owner added to a shared buffer
finds out from a test, not from wrong numbers on a board.

| id | task | eff |
|---|---|---|
| 6.5.6.1 | **`kCompute` arena owner guard** — `scratch::enter(Owner)`/`leave()` RAII, asserting unclaimed. Scoped and deferred in `pre-phase5-review.md:132-140`. Since then, CAS's pool and 5.2's list-broadcast staging (`unified_vm.cpp:198-221`, with its own hand-derived argument at `:206-217`) became co-owners. `scratch.hpp:30-35` is still a comment | M |
| 6.5.6.2 | **Owner tags on `Framebuffer::bind()`, `gfx::scratch_pixels()` and `io_scratch`** (`framebuffer.hpp:73-89`, `:113-121`; `io_scratch.hpp:21-37`). The 6B.8 script canvas is named in two of those comments as an owner checked by hand | S |
| 6.5.6.3 | **`Value` generation counter** — list and matrix `Value`s carry the temp pool's generation; a debug accessor asserts it matches. Enforces `unified_eval.hpp:340-344`'s "valid until next `run()`" before 5.2's anticipated extra callers arrive, and is a cheap interim for #14's P5/P6 | M |
| 6.5.6.4 | **`calc_api` busy guard as RAII.** Fifteen hand copies of `if (g_in_call) return kCalcBusy; g_in_call = true; … g_in_call = false;` (`calc_api.cpp:509-562`, `704-746`, `1017-1142`). One missed copy on a new binding reopens the GC-finalizer re-entry hazard `:36-46` explains | S |
| 6.5.6.5 | **`ScopedVar` for sweep variables.** `graph_screen.cpp` hand-saves and restores `x`/`t`/theta around each sweep (`:371/393`, `:401/438`, `:446/478`). A future early `return` between the two leaves a stray value in the global engine that the home screen then reads | S |
| 6.5.6.6 | **`calc_matrix_in` validates before mutating** (`mp_calc_module.c:443-462`). `calc_api_mat_begin` resizes the shared `g_mat_scratch` (up to 99x99, PSRAM) before `cols > CALC_CHUNK` (32) is checked. Move the width check above it; it only needs `cols`, already known at `:452`. D74's rule, inside one glue function | S |
| 6.5.6.7 | **Shared constants for twinned magic numbers.** `CALC_DIR_WINDOW 4` (`mp_calc_module.c:894`) and `DirEntry buf[4]` (`micropython_embed.cpp:267`) are linked only by a comment, and the 4 KB stack measurement at `mp_calc_module.c:868-879` depends on them being equal. Move the constant into `calc_api.h` or `static_assert` the match | S |

### 6.5.7 Nothing disappears silently

| id | task | eff |
|---|---|---|
| 6.5.7.1 | **Editor load/new confirm.** `F4` calls `buf_.clear()` unconditionally (`text_editor_widget.cpp:257-265`); `F3` goes to `load()`, which clears first (`:108-141`). `ESC` alone has the two-press "Unsaved!" guard (`:227-233`). One `confirm_discard()` helper for all three. `F1`'s save-then-run stays as it is | S |
| 6.5.7.2 | **File browser past 32 entries.** `kMaxEntries = 32` (`files_screen.hpp:48`); `list_dir`'s `skip` pagination exists (`storage.cpp:170-195`) and is never passed. A 40-file folder shows 32 with no hint, and the rest cannot be opened, renamed or moved. **S**: show "32+" in the counter (`:605-611`). **M**: paginate, keeping #46's sort order across pages | S/M |
| 6.5.7.3 | **Text load past 256 lines.** `insert_newline` refuses past `kMaxLines` (`text_buffer.cpp:127-130`, with a test), but `append_text` — the load path (`:39-61`) — does not. `reindex` stops recording starts (`:15-23`), so lines past 255 are unreachable and rendered corrupt. Refuse with the existing "truncated" message, or copy `OutputLog::reindex`'s drop-oldest approach (`output_log.cpp:31-64`) | S |
| 6.5.7.4 | **Silent caps on registries and loaders.** `kMaxSdApps = 16` breaks silently (`app_registry.hpp:55`, `sd_app_scan.cpp:114-116`): report "+N more" on the launcher/diag line, which matters more once 6.3 adds tiles. **Plus** `named_lists_persist.cpp:124`: `snprintf("%s", img.names[i])` from an on-disk `char[6]` with no guaranteed NUL is an out-of-bounds read on a corrupt index; use `"%.*s"` and add a host test with an unterminated name | S |

### 6.5.8 Input validation and numeric edges

| id | task | eff |
|---|---|---|
| 6.5.8.1 | **Clamp before the cast.** `Viewport::px_x`/`px_y` (`viewport.cpp:5-12`) `static_cast<int>` an unbounded double, which is undefined behaviour out of range. `clamp_px` (`graph_screen.cpp:39-42`) and `plotter.cpp:45-46` clamp the int afterwards. Reachable from a large finite value near an asymptote, and directly from a WINDOW edit. Clamp the double first | S |
| 6.5.8.2 | **Validate WINDOW and mode fields at commit.** `WindowScreen::commit_edit` (`window_screen.cpp:89-98`) accepts any `eval_field` result. Reject non-finite values, magnitudes past ~1e15 and min >= max; bound `Tstep`/`THstep`/`PlotStep`/`nMax`. This also closes the `int` overflow in `parametric_source.cpp:22-23` (and polar's twin) and `lround` in `seq_points.cpp:9-11` | S |
| 6.5.8.3 | **DIST screen says why.** Every `dist::*` returns bare NaN on a domain error (`dist.cpp:26-28`), shown as "NaN". Its siblings (`matops`, `stats`, `infer`, `numeric_solve`) all carry an error string. Cheapest option: the DIST screen maps NaN to "Domain error: check sd>0, df>0, 0<=p<=1". Fuller option: result structs on the guided-flow entry points | S/M |
| 6.5.8.4 | **Record the error-convention rule.** `listops::sum`/`prod` return NaN for a wrong dtype (`list_ops.cpp:170-200`), indistinguishable from a NaN result; the only caller pre-checks today (`unified_vm.cpp:990-1022`). AGENTS.md says "bool, optional or result struct" but not when. Add one sentence ("a caller that renders to a user needs a reason string") and fix or document `sum`/`prod` | S |

### 6.5.9 Shared UI components that unblock issues

6.4.8 generalised the status and softkey bars and closed #61–#63 and #65.
The same duplication, one level up, is the list or menu screen.

| id | task | eff | unblocks |
|---|---|---|---|
| 6.5.9.1 | **`ui::SelectableList`** — index, scroll window, Up/Down, Enter, Esc, digit accelerator. Five hand-rolled copies have already drifted. `calc_menu.cpp:39-67` and `cas_menu.cpp:42-69` are near-identical and **wrap**. `launcher_screen.cpp` wraps too. `const_screen.cpp:57-98` and `files_screen` **clamp**. `calc_menu.cpp:60`/`cas_menu.cpp:63` compute the digit ceiling as `'0'+count`, which passes `'9'` if a menu grows past nine items; `launcher_screen.cpp:87` already fixed that. **Wrap vs clamp is a decision to record, not a refactor detail** | M | #41 (the completion popup is a SelectableList), #51 (launcher grouping) |
| 6.5.9.2 | **Compile-error reason plumbing.** `Engine::compile` drops tinyexpr's `err` (`engine.cpp:322-336`); the unified `compile` already has `const char** err` (`unified_eval.hpp:326`). Add the out-param and cache the reason next to `valid_mask_`, **refreshed in `on_activate`/`on_key`, never in `render()`** (D47) | M | #18 |
| 6.5.9.3 | **One `>frac`/`>dec` suffix matcher.** `autoclose.cpp:16-21` and `home_screen.cpp:475-478` are byte-identical, and the comment claiming "the two cannot disagree" is the only link | S | — |
| 6.5.9.4 | **Chrome regression guard.** All 22 screens use `ui::draw_status_bar`/`draw_softkeys` today; nothing keeps the 23rd on them. Add a host test or lint rule: no `src/apps/*.cpp` fills the status or softkey bands except through `ui::chrome` | S | keeps #61–#65 closed |

### 6.5.10 SRAM reclaim, idle power, SD throughput

| id | task | eff | boards |
|---|---|---|---|
| 6.5.10.1 | **SinReg accumulator into the compute region.** `static calc_t acc[64][7]` in `sinusoid_fit` (`stats.cpp:761-762`) is **3,584 B of permanent bss** for one of ten regression types. The same file overlays its other buffers on `scratch::compute_region()` (`stats.cpp:17-26`), with ~16 KB of that region unused by stats. Do this after 6.5.6.1, so the overlay is guarded | S | pico1: **−3.6 KB of 14 KB free** |
| 6.5.10.2 | **Graph cache audit against 14 KB, not 76 KB.** `graph_screen()::instance` is 12,944 B; the stat-plot module statics (`stat_plot.cpp:42-83`: `g_points` 10,116, `g_buf_x`/`g_buf_y` 2,048 each, `g_tmp_px`/`g_tmp_py` 1,600 each, `g_cache` 1,152) total ~18.6 KB. Candidates: is `kMaxCurvePoints = 340` per parametric slot still right, and can stat-plot staging (`g_buf_*`, `g_tmp_*`) borrow the compute region between recomputes? Measure-and-decide; record which caches stay and why | M | pico1 |
| 6.5.10.3 | **Idle yield.** The main loop (`main.cpp:548-976`) and `power.cpp` `tick()` never `__wfi()` or sleep, so core 0 spins at full clock (200 MHz on Pico 1) even in soft-sleep. D38 scoped out *deep* sleep, not this. Add a bounded `sleep_us(1000)` or `best_effort_wfe_or_timeout` when nothing is dirty and no I2C transfer is in flight. **Bench current draw** before and after; check key repeat and USB CDC are unaffected | M | both |
| 6.5.10.4 | **Multi-block SD transfers.** `sd_card.cpp:216-269` issues only CMD17/CMD24, and `sd_diskio.cpp:26-48` loops per sector. Add CMD18/CMD25 for `count > 1`. It shortens every graph-state write and every large list or matrix load; time a 6.5 KB save before and after | M | both |
| 6.5.10.5 | **Home history memory, recorded not changed.** `home_screen()::instance` is 7,692 B (`kMaxHistory = 50` x `Entry{expr[96], result[48]}`). No action; note the per-entry cost in `config.hpp` beside `kHistorySize` so a "keep more history" request is priced correctly | S | pico1 |

### 6.5.11 Behavioural tests at the seams

Test density is high in `math/` and thin exactly where hardware bugs have come
from. The host infrastructure to fix that already exists and gates nothing.

| id | task | eff |
|---|---|---|
| 6.5.11.1 | **Persistence round-trip suite.** No host test calls `platform::storage()` save and load; `grep` finds only a counting hook in `test_calc_api.cpp:216-247`. Five formats (`var_store`, `lists_persist` PCL2, `matrices_persist` PCM2, `graph_persist` PCG6, `named_lists_persist` PCN1) against `storage_posix.cpp` in a temp `PICOCALC_HOME`: round-trip equality, corrupt magic, truncated file, unterminated strings. **Land before 6.5.1** | M |
| 6.5.11.2 | **D77 fragmentation fixture.** The interpreter's self-rebuild (`micropython_embed.cpp:395-435`, threshold `kMinCompileBytes = 1024`) was found by accident on hardware and has no test. `test_calc_api` never links the VM. Run D77's own 400-iteration string loop through `graphite-shot --run`, then assert a following `--eval` succeeds and the rebuild message fires | S |
| 6.5.11.3 | **Gated `graphite-shot` assertions.** CI's `host-render` checks only that the home screen renders a valid, deterministic PPM. Add a small stable set: `--eval "2+2"` gives `4`; one `examples/apps/` script exits 0; `calc.det` of a 40-column list raises cleanly (exercises 6.5.6.6); one keyscript against a committed PPM | M |
| 6.5.11.4 | **Differential evaluator corpus.** tinyexpr (`functions.cpp`) still drives the graph, table, solver, DIST and slot editor; the unified VM re-derives D46's angle wrapper independently (`unified_vm.cpp:45-77`). `test_unified.cpp:585-589` checks angle mode for the unified side only. Run one expression list through `engine()` and `unified::eval` in both angle modes and assert agreement. **Check the harness by temporarily reverting D46's `m_sin` wrapper**: it must fail. Discharged if #16 retires tinyexpr | M |
| 6.5.11.5 | **`host-tests.sh` onto the shared source list.** 22 hand-written per-binary file lists, e.g. the same ~15-file evaluator cluster repeated four times. That re-creates, one layer down, the duplication D92's `cmake/graphite-sources.cmake` removed. Convert incrementally, largest cluster first; the pass/fail count must not change | M |

### 6.5.12 Readiness notes for planned features, and doc drift

Cheap, decision-independent steps. Each is a paragraph in the relevant spec
or issue, or a small doc fix, so the finding isn't rediscovered later.

| id | task | eff |
|---|---|---|
| 6.5.12.1 | **6.4.5's `platform::Sound` seam, shaped for 6.2.** A blocking `tone(freq, ms)` is the natural minimum for 6.4.5, and §9.4's PCM sampler (`phase6-spec.md:2068-2091`) needs a DMA-paced fill callback. Define the seam as a small queue or fill callback now, driving one tone through it, so 6.2 extends it instead of replacing it. Add a note in `phase6.4-spec.md` §3.4 | S |
| 6.5.12.2 | **6.3's flash-write quiesce as a reusable helper.** `phase6.3-spec.md:260-267` R1 already cites D10's `__not_in_flash_func` precedent. When 6.3.0's spike is built, make "quiesce core 1 and PSRAM, then run RAM-resident" a `platform::` helper rather than a one-off. Add a note in R1 | S |
| 6.5.12.3 | **Headroom is re-measured, never cited, at scoping time** for 6.1, 6.2 and 6.3. 6.3 cites "15 KB / 24 KB" (2026-08-16) and today's figure is 14 / 24; there is no drift yet, but D69 shows it goes stale silently. 6.5.0.1 makes this automatic; until then, add one line to each draft spec's preconditions | S |
| 6.5.12.4 | **Issue notes.** On **#22** (3D), say it is Pico-2-only through the `kUseFullFramebuffer` seam: the 2D column caches and `Plotter` have no depth, and Pico 1 has 14 KB. On **#20** (crosshair), `draw_trace` already holds `px, py` (`graph_screen.cpp:1106-1110`), so it is one guarded `draw_hline` and stays idempotent. On **#49**, highlighting must cache per-line token colours in `on_key`, not tokenise in `render()`, which runs 40x per frame on Pico 1 (`text_editor_widget.cpp:345-353` draws everything `kWhite` today). On **#21**, neither `TextBuffer` nor `InputLine` has a selection anchor and no clipboard exists; scope it as a shared clipboard plus an anchor in both widgets, never per screen | S |
| 6.5.12.5 | **Doc drift.** `docs/architecture.md:26,142` document `platform::Audio`/`audio.hpp`, which does not exist (6.4.5 confirms). `fault.hpp:11-18` describes watchdog scratch registers, but `fault.cpp:27-40` uses a 44 B `__uninitialized_ram` record. `pre-phase5-review.md` cites ArrayStore at 28x2 KB (~57 KB), superseded by D70 (`kSlabCount = 14`, 28,844 B); add a forward pointer | S |
| 6.5.12.6 | **Path-existence rule in `validate_md.py`** for source paths named in `architecture.md`'s tree, the mechanical guard for 6.5.12.5's first item, per the docs-drift lesson | S |

---

## 4. Risks

- **Write-behind without atomic replace is a regression.** 6.5.1.2 alone
  widens the loss window without closing the torn-write window. Take 6.5.1.1
  with it or not at all (6.5.1.4 records this).
- **Watchdog false positives.** 6.5.5.2 turns a legitimately long
  computation into a reboot if a checkpoint is missing. It is gated on
  benching the worst legitimate gap, and it lands after D101's soak.
- **Wrap vs clamp** in 6.5.9.1 changes behaviour at list ends on at least
  three screens. Treat it as a decision, not a refactor.
- **Clip gates can drop pixels.** 6.5.3.1 and 6.5.3.5 are checked by host
  PPM byte-identity *and* on Pico 1. The host renders one strip-free frame,
  so it cannot catch a strip-boundary miss by itself.
- **clang-tidy backlog.** 6.5.0.3 lands non-blocking first; a large backlog
  is a finding, not a reason to skip.
- **Line numbers drift.** Re-grep each site when a task starts.

---

## 5. Considered and not carried

- **"Core-0 PSRAM DMA racing core-1 display DMA causes D53."** D53's
  2026-08-09 investigation ran 40,000+ per-element reads clean under display
  load and judged contention unlikely. #24 says not to retest the driver. The
  sweep (6.5.2) reduces the exposure; the root cause stays with #24's
  `psram_addr_` comparison.
- **"Revisit #38 and restore `kStripHeight = 16`."** #38 is closed by
  measurement. `config.hpp:60-79` records the trade: 3.4% frame time, below
  the ~6.3% originally measured, for 10,008 B (15.2 KB free vs 5.4 KB), about two thirds of remaining
  headroom.
- **Replacing tinyexpr, or re-vendoring it (#15 / #16).** Out of scope for a
  polish phase. 6.5.4.3 supplies the missing measurement, and 6.5.11.4
  protects agreement meanwhile.
- **Collapsing `Kind::kMatrix`/`kList` (#14).** Out of scope. 6.5.6.3 is the
  cheap interim for its P5/P6 lifetime rows.

---

## 6. Verification rules for the phase

1. Both boards build; host suite green; lint and format clean; modified
   markdown passes `validate_md.py` (AGENTS.md).
2. **Every latency task quotes 6.5.0.4's before and after numbers on Pico 1**,
   and Pico 2 where the path differs. A "should be faster" without a number
   does not close a task.
3. **Every SRAM task quotes a `size-report.sh` diff.**
4. **Every PSRAM task (6.5.2) is soaked with D53's method** (30 reps,
   error-revealing fold) on Pico 1.
5. Visual changes (6.5.3, 6.5.9.1, 6.5.12.4) are regenerated through
   6.4.4's image set, so D98's drift check covers them.
6. On acceptance, each task the developer takes becomes a GitHub issue with a
   `6.5` milestone (`issue-tracking.md`: could someone close it?). This spec
   stays the record.
