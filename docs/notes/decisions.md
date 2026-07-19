# Architecture & Design Decisions

A running log of decisions made during development. Each entry captures the question, the choice, the rationale, and what was traded off. New entries go at the top.

Format:

```
## DXX: <decision title>

**Date**: YYYY-MM-DD
**Status**: Accepted | Superseded by DYY | Deferred
**Context**: <what triggered the decision>
**Decision**: <what was chosen>
**Rationale**: <why this over alternatives>
**Tradeoffs**: <what we gave up>
**Revisit when**: <conditions that should trigger reconsidering>
```

---

## D26: Storage health — retry-forever heartbeat, SD hot-plug, red status-bar indicators

**Date**: 2026-07-19
**Status**: Accepted (Session 15 — implements the Session 14 observation batch, `session14-observations-verbatim.md`)
**Context**: On-device: SD showed "no card" after an extended power-off and only a reboot recovered it. Root cause: the D14 late-init retries ran every 2 s but stopped for good at 30 s uptime. Also unhandled: card eject/insert while powered (DET pin was only read inside init), and there was no visible signal that a subsystem was down.
**Decision**:
1. **Retries never give up**: the 30 s window is now just the fast phase (2 s cadence, D14 rail settle); after it, an unhealthy SD or PSRAM keeps retrying on a **10 s heartbeat** indefinitely. `run_self_tests()` skips subsystems already green (the PSRAM word test bump-allocates 256 B per run and must not repeat forever; the SD probe stops rewriting its file once OK).
2. **Hot-plug**: the main loop polls the DET pin every ~1 s. **Eject** → `Storage::on_card_removed()` (f_unmount + `sd::invalidate()` so FatFs sees NOINIT), storage marked down, `g_sd_test = kNoCard`. **Insert** → the retry timer resets, so the remount attempt is immediate.
3. **Persisted state loads exactly once** per power-on: a late-mounted card loads then, but an eject + re-insert must NOT reload — the in-memory working state is newer than the files. (Lists already load-once.)
4. **Status-bar indicators**: red `SD` / `PSRAM` after the title while the subsystem is unhealthy (`ui::set_health_flags`, drawn by `draw_status_bar`); the main loop updates on any change and repaints the status band (battery-refresh pattern). Only screens using the shared chrome show them (WINDOW/editors draw their own plain bars — acceptable; the home screen is where you look).
5. Same batch: Y=/PAR/POL editor rows truncate long expressions with `...` before the enable checkbox (stored regression models ran beneath it).
**Rationale**: "Disappear when the retries finally work" (the request) requires retries that never stop; eject-drop-the-mount prevents half-written state files; load-once protects the session from stale reloads.
**Tradeoffs**: A failing-but-present card costs a ~1 s blocking init attempt every 10 s forever (visible as a periodic hitch only in that broken state); DET polling assumes the pin is configured (guaranteed — boot always reaches `sd::init()`'s GPIO setup); mid-write ejects can still corrupt the file being written (poll is 1 s — hardware can't prevent it).
**Revisit when**: the periodic init hitch is noticeable in real use (make the attempt async/backoff), or a future RTC/logging feature needs write-behind flushing on eject.

## D25: 3C distributions as-built — two-sided CDFs (P3-4), real-df wrappers, `dist` guided screen

**Date**: 2026-07-19
**Status**: Accepted (Session 14, sub-phase 3C conventions; resolves phase3-spec P3-4)
**Context**: 3C.2-3C.8 needed the naming/convention calls the spec left open: one- vs two-tailed CDFs (P3-4), how discrete arguments behave, error signalling without exceptions, and the helper-screen shape.
**Decision**:
1. **P3-4: TI-style two-sided CDFs** for the continuous distributions — `cdf(lo, hi, ...)` = P(lo <= X <= hi), `hi < lo` is a domain error; open tails use +/-1e99 (any far value works — `normal_cdf` saturates |z| > 40 before cephes, whose `ndtr` overflows on 1e99). `inv(area, ...)` inverts the **lower tail**, area in (0,1) exclusive (chisq/F additionally accept area = 0 -> 0). Rationale: the two-arg form is what the stats workflows actually use, and it matches the TI the UI imitates; lower-tail inv matches invNorm/invT.
2. **Spec naming kept** (`normal_pdf/cdf/inv`, `t_*`, `chisq_*`, `f_*`, `binomial_pmf/cdf`, `poisson_*`, `geometric_*`), registered **full-arity** (no default-arg shorthands; tinyexpr is fixed-arity — spec §5.3). Deviation from the spec sketch: discrete `k`/`n` parameters are `calc_t`, not `int`, so every function binds directly to the parser; the TI integer rule is enforced at runtime — **pmf arguments must be integers** (within 1e-9, else NaN), **cdf floors k** (P(X <= floor(k))). Geometric counts trials until first success (k >= 1). Domain errors return NaN (project error convention; the engine already displays NaN).
3. **Real-valued degrees of freedom throughout**: t/chisq/F are built on `incbet`/`incbi`/`igam`/`igamc`/`igami` directly (the reason the integer-df cephes wrappers were not vendored, 3C.1); pdfs/pmfs are `std::lgamma` closed forms. No local bisection needed — cephes' own inverses cover all three inverse CDFs (`t_inv` via `incbi` + the symmetric-tail transform, `chisq_inv` = 2·igami(df/2, 1-area), `f_inv` via `incbi` + the beta-to-F transform).
4. **`dist` typed command** (D20) → guided form: Distribution and Function rows (L/R cycle; pdf|pmf/cdf/inv per distribution), the combination's parameters as **InputLine full-expression fields** (WINDOW pattern, ENTER edit / DEL clear-and-edit), Calculate. Parameter values live in shared named slots, so switching pdf -> cdf keeps mu/sd. Calculate builds the equivalent catalog call, **evaluates it through the engine** (so Ans updates — TI DISTR paste-and-run behavior) and shows both the call and the result.
5. **Registration mechanics**: catalog fp3/fp4 casts, `kMaxCatalogEntries` 32 -> 56 (18 new rows, 47 total), help FUNC summaries draw at `max(kSummaryCol, sig_width + 1)` so `normal_cdf(lo,hi,mu,sd)` doesn't overlap. Signatures now use descriptive parameter names; the host test correspondingly builds a numeric call at the declared arity instead of parsing the signature text.
6. **`isfinite` link shim** (`src/math/cephes_support.c`, compiled into the cephes target): cephes `gamma.c` calls `isfinite()` as an extern function, but newlib and macOS libm only provide the macro — the cephes static lib had never been linked into a binary until `lgam` got referenced. Fix lives outside the read-only vendored tree per the AGENTS.md driver-workaround rule.
7. **Reference-value infra**: `tests/host/gen_dist_vectors.py` generates the test vectors at 50-digit precision with mpmath; Python dev-deps go in the gitignored `.venv` and are tracked in `requirements-dev.txt` (developer rule, this session).
**Rationale**: Matches the handheld conventions users know, keeps every function parser-bindable with one signature, and reuses cephes' inverses instead of writing a solver.
**Tradeoffs**: No one-sided CDF shorthand (type `-1e99` for the lower tail); pmf integer strictness means `binomial_pmf(2.0000001,...)` errors rather than rounding; `chisq_inv` computes `1-area` (precision loss for area within ~1e-16 of 1 — irrelevant at calculator precision); Pico 1 text grew ~30 KB now that cephes really links.
**Revisit when**: 3D inference needs additional tail conventions or vectorized (list-argument) distribution calls; a one-sided shorthand is repeatedly missed on device.

## D24: Session 13 usability batch — lift operands (literal broadcast fix), range(), reduction args, aliases, pi glyph

**Date**: 2026-07-19
**Status**: Accepted (Session 13 — dispositions for the on-device observation batch in `phase3A-3B-observations-verbatim.md`)
**Context**: First real usage of the Session 12 firmware produced a verbatim observation list: two outright bugs (brace-literal broadcast rejected; home screen unreachable after HOME/trace use in the graph screen), several direct requests (range(), mean/median/std on the home screen, command shortcuts, a computing indicator, pi glyph, list-editor color), and design questions to park.
**Decision**:
1. **Lift operands**: inside a vector-lifted expression, top-level brace literals and wrapper calls (sort_asc/sort_desc/cumsum/delta_list/seq/range) are evaluated into side arrays (4 slots, handed out monotonically per evaluate() and released per lift, so nesting never aliases) and bound as extra engine variables (`lopa`..`lopd`). Fixes the reported `{1,2,3}+2` / `{1,2,3}+{2,2,2}` errors at the general level and makes wrapper results compose: `cumsum(range(1,4))+1`, `range(1,9)*l1`.
2. **`range(lo, hi[, step])`**: inclusive endpoints, default step of +/-1 toward hi (`range(5,1)` counts down), backed by `listops::seq` with the identity formula; same 10000 cap. The quick generator the large-array testing gap asked for.
3. **Reductions gain `mean`/`median`/`stdev`** (sample Sx, via `stats::one_var`; `std` accepted as an alias), **and reduction arguments generalize** from bare list names to any list expression, substituted innermost-first — this lifts the D22 bare-arg limitation (`sum(range(1,10000))`, `mean(l1*2)` work). NaN reduction results (stdev of 1 element) error as "Undefined result".
4. **Typed-command aliases**: `?` = help, `list` = lists, `stat` = stats (no collisions — none parse as expressions).
5. **Stats Calculate pushes one "Computing..." frame** before the synchronous compute (flag + forced `render_frame()`, render stays idempotent). Closes the D23 revisit — added ahead of the timing-feel eval since it costs one frame.
6. **Pi glyph**: `bdf_to_utft.py` gained `--map DEST:CODEPOINT`; the 8x16 main font bakes U+03C0 at the unused DEL slot (0x7F, `gfx::kGlyphPi`) and the layout builder renders the identifier `pi` as that glyph. The 5x8 BDF has no pi, so the small font is untouched (draws blank if ever asked).
7. **HOME nav invariant**: `ScreenManager::switch_to()` never replaces the root screen — at depth 1 it pushes instead. Root cause of the observed breakage: F4 trace from the home screen went through `switch_to` → `replace()`, overwriting the stack root with the graph screen, after which ESC/HOME had no home screen to return to.
8. **List editor placeholder color** is decided by exact cell text ("_" / "---"), not a leading '-', so negative numbers render white like positive ones.
9. **Parked on the wishlist**: greek letters/subscripts in stats output, JuliaMono font swap (licensing + baking check), scientific constants, unit conversions, >6 lists and SD list-data files (CBL/CBR).
**Rationale**: One general operand mechanism fixes the literal bug and delivers range() composability instead of two special cases; generalized reduction args + range() directly serve the observed "no quick way to generate/test large arrays" gap; the rest are verbatim developer requests from device use.
**Tradeoffs**: ~9 KB more bss (operand chunk buffers + 5 static Arrays; Pico 1 bss now ~135 KB of 264 KB); `lopa`..`lopd` are technically bindable identifiers while a literal is present (harmless, undocumented); reduction substitution evaluates arguments eagerly, so a slow argument computes even when the surrounding expression later errors; literals stay capped at 64 elements, operands at 4 per lift.
**Revisit when**: the 4-operand or 64-element caps pinch in practice; complex dtype (4C) reaches the lift/operand path; or `sum({...})`-style reduction-over-operand syntax confusion shows up on device.

## D23: 3B stats as-built — LM for iterative fits, rank-selection quartiles, TI r conventions

**Date**: 2026-07-19
**Status**: Accepted (Session 12, sub-phase 3B implementation decisions; resolves phase3-spec P3-3)
**Context**: Task 3B needed four calls the spec left open: the iterative solver for logistic/sinusoidal (P3-3), how to get quartiles/medians without mutating or copying a possibly-80 KB PSRAM list (the D22 API has no `data()` to sort through), what r/r² mean per model, and where the results UI lives.
**Decision**:
1. **P3-3: Levenberg-Marquardt** (classic Marquardt damping, analytic Jacobians, <= 100 iterations, ~2 streaming passes per iteration) for logistic and sinusoidal. Seeds: logistic from a linearized logit at ceiling `1.05*max(y)`; sinusoidal from a frequency scan (0.25-cycle grid over the x-span, <= 64 candidates, each solved as a linear sin/cos fit in one shared streaming pass) — LM alone cannot find the frequency basin. `converged=false` (surfaced as a results-screen warning) when the cap is hit; the fit is still returned.
2. **Quartiles/medians by streaming rank selection, not sorting**: binary search over the order-preserving uint64 image of the doubles, one weighted counting pass per bit, all requested ranks advancing in the same passes (<= 64 passes per batch total). No temp region, no allocation, identical code path for plain, frequency-weighted (integer freq >= 0, TI rule; freq 0 excludes the element), and x-range-filtered selections (which is how median-median gets its group y-medians). Med-med caveat: x-ties straddling a group boundary land whole in the outer group (value-based grouping), a documented deviation from a strict positional split.
3. **r/r² per TI convention**: `r` only for linear and the linearized fits (ln/exp/power, where it describes the linearized regression, as on the handheld); polynomial degree >= 2, logistic, sinusoidal, med-med report `r²` only, computed as 1 - SSE/SST on the original data (NaN skips the results line). Polynomial fits standardize x (center+scale) before forming normal equations, then expand coefficients back — quartic on year-scale x stays conditioned.
4. **UI: typed `stats` command** (D20 pattern) → one screen, form + results phases. Form: Analysis (1-Var / 2-Var / 10 regressions), source lists, optional freq list (1-Var), optional Store-to y1..y7 (regressions, task 3B.8 — writes the numeric model via `format_model`, enables the slot, saves graph state; SinReg coefficients are degree-converted when the global mode is DEGREE per spec §10). Results are cached text lines (strip-safe render), scrollable.
**Rationale**: LM is the spec's lean and the robustness matters more than the extra solve code (shared with the polynomial path anyway); rank selection keeps stats allocation-free and O(passes) on PSRAM lists where a pair-sort would have needed new machinery; TI conventions keep results comparable to the handheld the UI imitates.
**Tradeoffs**: Selection costs up to 64 streaming passes (~0.7 s worst case on a full 10000-element PSRAM list — one-shot per Calculate, acceptable); no `r` for the nonlinear fits (TI-consistent); sinusoidal frequency grid caps at ~16 cycles over the x-span (denser oscillations need pre-scaled x).
**Revisit when**: 3C/3D need the same quantile machinery with real-valued weights; a user hits the frequency-grid cap; or the stats screen needs a "computing..." indicator (compute is synchronous in on_key — judge on device with large lists).

## D22: Array as-built API (get/set, tiered store) + home-screen list syntax

**Date**: 2026-07-19
**Status**: Accepted (Session 11, task 3A implementation decisions)
**Context**: Implementing D21's `Array` surfaced two facts the spec sketch (§2.1) didn't account for: (1) the PSRAM is SPI-attached and **not memory-mapped** (`platform::Psram` hands out addresses, access is `read()`/`write()`), so `calc_t& at()` / `calc_t* data()` cannot exist for the PSRAM tier; (2) `Psram::alloc` is bump-only (no free), so resizable lists need an allocation scheme above it. Separately, lists cannot flow through tinyexpr (scalar `double` values only), so §3.2's "callable from the home screen" needed a concrete syntax layer.
**Decision**:
1. **Element access is `get(i)`/`set(i, v)` plus bulk `read_range`/`write_range`** (chunked DMA underneath); no reference-returning accessors, no raw `data()`. This is also the natural shape for D21's tag-aware access rule — future complex elements change the accessor internals, not the callers.
2. **`ArrayStore` = fixed-size recycling on both tiers**: a pool of 12 x 2 KB SRAM slabs (one slab = one small array, <= 256 doubles) and up to 12 x 80 KB PSRAM regions (one region = one large array at full 10000-double capacity) handed out from a free-list over the bump allocator. Fixed sizes make recycling trivial and fragmentation impossible; crossing 256 elements migrates slab→region (and back on shrink, freeing the region).
3. **Home-screen list syntax** (`math::listexpr`, layered above the engine): `{1,2,3}` literals (elements are full expressions), `l1..l6` references (lowercase, D19), `-> lk` store, `sum/prod/length(l1)` **scalar reductions with bare-list args only** (substituted as numeric literals, so they embed in any scalar expression), wrappers `sort_asc/sort_desc/cumsum/delta_list(X)` + `seq(expr, var, lo, hi, step)`, and **vector lift**: any other engine expression mentioning `l1..l6` (e.g. `sin(l1)+2*l2`) is compiled once with `l1..l6` bound as per-element variables and evaluated element-wise in 256-element chunks.
4. **Sort semantics**: `sort_asc(l1)` with a bare list arg sorts **in place** (spec §3.2); compound args (`sort_asc(l1+0)`) sort a copy. Large-list sorts use an external merge sort through one temp PSRAM region (~256-element runs, streaming merges).
5. **`Ans` stays scalar** — list results display but don't set Ans; list persistence (`lists.dat`, magic `PCL1`, per-list dtype+count header + streamed raw elements) saves on every mutating edit/command, all-or-nothing load that waits for PSRAM on cold boot (D14).
6. **Editor entry is the typed `lists` command** (D20 command layer); in-editor ops use non-global keys: ENTER/type to edit, DEL delete row, F6/F7 (Shift+F1/F2) sort, F8 (Shift+F3) clear list. F1-F5 keep the global scheme.
**Rationale**: The seam (`math::psram_backend`) keeps the whole stack host-testable (malloc shim); fixed-size regions bound PSRAM use at 960 KB worst case against 8 MB; the vector lift reuses the engine's compile-once path instead of a second expression grammar; textual reduction substitution keeps tinyexpr untouched.
**Tradeoffs**: Reductions only accept bare list names (`sum(cumsum(l1))` doesn't parse — store the inner result first); list literals can't appear inside element-wise arithmetic (`2*{1,2}` is an error; use a stored list); one 80 KB region per large list even when barely over 256 elements; wrapper nesting capped at 2.
**Revisit when**: 3B stats/regression needs richer expressions (consider promoting list_expr to a real tagged-value evaluator), the complex dtype lands (accessor internals + `lists.dat` tag), or Phase 4 matrices need >80 KB (region size is a constant).

## D21: Phase 3 Array — 999 cap, SRAM-only, double elements with a dtype tag

**Date**: 2026-07-18
**Status**: Accepted (resolves phase3-spec P3-1 and P3-2 ahead of task 3A.1)
**Context**: Phase 3 3A needs the `Array` primitive's shape decided up front. The spec suggested a 10000 cap "given PSRAM", but that predates D10's quarantine of bulk PSRAM transfer (only word r/w is verified), and the complex-numbers wishlist (2026-07-18) raised the element-type stakes for Phase 4's Matrix reconciliation.
**Decision**: (1) **P3-1: max list length 999** (TI parity). (2) **Storage: SRAM-only for Phase 3** — six full lists are ~48 KB, inside even the Pico 1's ~195 KB headroom; `ArrayStore` keeps the backing abstract so PSRAM can be added later without caller churn. (3) **P3-2: elements are `calc_t` (double) only, but `Array` and the `lists.dat` image carry a dtype tag.** The tag is not speculative: **complex-valued lists/matrices are a committed direction** (developer, 2026-07-18) — they land with the unscheduled complex-numbers feature, and the dtype tag + persistence format must make that a non-breaking addition.

**Amended same day (post-D10 fix, developer call): cap raised to 10000 with the PSRAM tier enabled.** The D10 bulk-PSRAM root-cause fix landed hours after this decision (see D10 addendum: ~6.8 MB/s verified), removing the constraint that motivated SRAM-only. Storage now follows the spec's §2.2 design as written: small arrays (<= 256 elements) in the SRAM pool, larger in PSRAM via the chunked bulk path — a full 10000-element list is 80 KB, ~12 ms to stream, six lists 480 KB against 8 MB. Arrays and Phase 4 matrices both use PSRAM. Non-blocking caveat: PSRAM is late on cold boot (D14, unresolved) — lists just aren't loadable until late-init brings it up, which is acceptable since nothing needs PSRAM at boot.
**Rationale**: Zero coupling to D10 — 3A cannot be blocked by a hardware session; fastest stat sweeps (no per-element SPI); TI-parity cap covers the realistic on-device datasets; the dtype tag future-proofs persistence and the Phase 4 `Matrix`-on-`Array` reconciliation for a few bytes now.
**Tradeoffs**: No >999-element lists in Phase 3; ~48 KB of SRAM reserved at full occupancy (acceptable on both boards). Integer lists' space savings forgone.
**Revisit when**: The D10 review un-quarantines bulk PSRAM (raise the cap behind ArrayStore), Phase 4 matrices outgrow SRAM, or the complex-numbers feature is scheduled (the dtype tag then goes live — planned, not conditional; note a full 999-element complex list is ~16 KB, so six lists ~96 KB still fit SRAM, but Phase 4 complex matrices likely force the PSRAM tier).

## D20: Global F-key scheme + typed command layer

**Date**: 2026-07-18
**Status**: Accepted (supersedes the D16 key bindings F4/F9 and the boot-era F6 diag toggle)
**Context**: Test-drive feedback item 7 (inconsistent F-keys across screens, WINDOW unreachable from the graph) plus the wish to demote the diag screen from the prime F6 slot. Settled in a design quiz with the developer.
**Decision**: TI-84-shaped global scheme on every screen: **F1** mode-dependent editor (Y=/PAR/POL), **F2** WINDOW (table screen: table setup), **F3** MODE, **F4** TRACE (opens the graph tracing from other screens; toggles on the graph), **F5** GRAPH / graph↔table toggle (split: pane focus), **Alt+F5** split toggle (HW-verified: Alt reaches F-keys with its flag intact; Shift+F5 is eaten by the STM32). Graph zoom moves to **`-`/`=`** (`+` also zooms in); S/T presets unchanged. Screen-local row/field ops move to non-F keys: **ENTER** edit, **SPACE** toggle slot enable, **DEL** clear (editor rows, WINDOW/setup fields — edit-from-empty; ASK-table row delete). Rarely-used surfaces become **typed commands** on the home screen (lowercase, matched before evaluation): `help`, `diag`, `files`, `cls`, `clrhist`; a grey right-aligned "type help" hint sits on the empty input line. F6-F9 are freed/reserved.
**Rationale**: One muscle-memory map across screens, matching the TI-84 F-row (Y=|WINDOW|·|TRACE|GRAPH); WINDOW finally reachable from the graph; diag demoted without losing access.
**Tradeoffs**: Commands (help included) only work from the home screen; diag lost its toggle-from-anywhere (serial late-init lines cover the cold-boot check). FILES moved out of the diag screen. The old F2-to-Step table shortcut was dropped as low-value.
**Revisit when**: KIV — F3 might become ZOOM (its TI slot, e.g. a preset menu) with MODE moving elsewhere; judge after real use.

## D19: Expression input is case-sensitive

**Date**: 2026-07-18
**Status**: Accepted (refines D11's wording)
**Context**: The typed-command layer (D20) wants exact lowercase matches; the engine blanket-lowercased every expression before tinyexpr, making input case-insensitive. Developer preference: case-sensitive.
**Decision**: Remove the lowercase folds (preprocess + store-op target). Identifiers — functions, variables a-z, `theta`, `ans`, commands — are lowercase only; uppercase input fails with a parse error, and `expr->A` gets a pointed "Variables are lowercase a-z" error. Numeric literals are unaffected (tinyexpr parses via strtod: `1E10` == `1e10`). `e` remains Euler's constant and not a variable; uppercase `E` is now just an unknown identifier.
**Rationale**: Exact matching is simpler to reason about, matches the lowercase-canonical engine internals that already existed, and the STM32 keyboard types lowercase by default so the day-to-day feel is unchanged.
**Tradeoffs**: Previously-persisted uppercase expressions (if any) now fail compile — editor rows render red and need retyping. Caps-lock typing errors instead of silently working.
**Revisit when**: A future need for uppercase identifiers (e.g. distinct A-Z variable bank, TI-style).

## D18: Defer the Pico 1 Phase 2 verification pass to post-Phase 3

**Date**: 2026-07-18
**Status**: Accepted
**Context**: The Phase 2 test drive (2.24) passed on the Pico 2; the Pico 1 pass was
deferred from Session 8. Both Picos share one PicoCalc mainboard, so a Pico 1 pass
costs a tedious physical module swap. Question: run it before Phase 3, or fold it
into Phase 3's own both-boards pass (task 3D.14)?
**Decision**: Defer. One combined Pico 1 pass after Phase 3 covers the Phase 2 sweep
(split-pane clipping headline, Session 8 fix list) plus Phase 3 acceptance — one
board swap instead of two.
**Rationale**: The board-conditional surface is four files, and the Pico 2 "full
framebuffer" mode is the strip path with one buffer-sized strip — the pane/strip clip
intersection (`set_pixel`/`fill_rect`) is shared code already exercised on the Pico 2,
so the D16 bleed worry is largely covered. RAM is a non-issue: the RP2040 build uses
62.5 KB static of 264 KB. The genuinely Pico-1-only risks — render non-idempotency
across ~20 strip passes per band, and perf feel (strip mode re-renders the scene per
strip; split full-redraws both panes) — produce localized visual/tuning bugs, not
architectural rework.
**Tradeoffs**: Bugs surface farther from their commit (mitigated: rebuild any tagged
firmware, e.g. 079a8b2, to bisect phase-2 vs phase-3 fallout in one flash). If Phase 2
introduced a non-idempotent render pattern, Phase 3 may copy it before hardware
catches it — mitigated by the strip-safety rule added to `phase3-spec.md` §8: new
screen `render()`s must be idempotent (may run ~20×/frame in strip mode).
**Revisit when**: Phase 3 grows new rendering machinery beyond ordinary screens
(animations, new split layouts) — then swap boards *before* that work starts; or any
host-side strip-mode regression harness appears, which would shrink the deferred risk
further.

## D17: Licensing — MIT own code, GPL-2.0 combined firmware

**Date**: 2026-07-12
**Status**: Accepted
**Context**: Open-sourcing the project. The firmware links GPL-2.0 vendored drivers (Coyote OS lcdspi/i2ckbd/pwm_sound + font1 bitmap font compiled into gfx::main_font); everything else (FatFs, tinyexpr, rp2040-psram, Pico SDK) is permissive.
**Decision**: Hybrid ("Option 3"): the project's own code is MIT (root LICENSE); the combined firmware binary is distributed under GPL-2.0 (treated as v2-only — Coyote files carry no per-file headers or "or later" language). NOTICE.md carries the component table and the scoped path to a fully permissive release — **kept as a future option, deliberately not on any roadmap**.
**Rationale**: Ships today with zero engineering work; the reusable subsystems (math/graph/render) stay permissively reusable; the door to full-MIT stays open since the GPL surface sits behind the platform/ HAL by design.
**Tradeoffs**: Repo carries two licenses (must stay clearly documented). GPLv2-only firmware means no code may ever be ported from GPLv3 projects (e.g. DB48X, the Phase 4 CAS reference — design reference only). Apache-2.0 was ruled out for the MIT side (incompatible with GPLv2-only).
**Revisit when**: Phase 4 wants GPLv3-licensed code, or the D9 font swap happens anyway (first step of the permissive path), or Coyote upstream clarifies/relicenses.

## D16: Split-screen — horizontal, singleton reuse, nearest-row sync, F4/F9 keys

**Date**: 2026-07-12
**Status**: Accepted (developer decisions, session 7)
**Context**: Task 2.19 needed the P2-1 orientation call plus four implementation choices.
**Decision**: (1) **Horizontal** split (graph top / table bottom) — the graph keeps its full 320px width, so the column caches and trace x-mapping are untouched; only viewport height changes. (2) **Reuse the live GraphScreen/TableScreen singletons** with runtime pane geometry (`set_pane`) — the spec's embedded-instances sketch would fork ~14 KB of caches and trace state. (3) Trace↔table sync is **nearest-row** (option c); "trace steps by table-step in split" (option b) is KIV pending the test drive. (4) **All three modes** supported from the start. (5) Keys: **F4 switches graph↔table** (full-screen: push/pop; split: pane focus), **F9 (Shift+F4) toggles split**, ESC exits.
**Rationale**: Horizontal turns 2.19 into a viewport-height change instead of a cache re-architecture; singleton reuse makes trace sync nearly free; nearest-row keeps round table values while tracing.
**Tradeoffs**: Graph pane is short (138px). Sync is approximate in function mode (pixel-grid x vs table grid). Panes render full logic clipped — ~1.5x frame cost per spec §12.
**Revisit when**: Test drive verdict on sync feel (upgrade to option b) or pane sizes.

## D15: One SlotEditorScreen base, thin per-mode editors

**Date**: 2026-07-12
**Status**: Accepted
**Context**: Task 2.5 needs a parametric editor; polar (2.9) follows. Options: generalize the HW-verified Y= editor into one mode-aware screen (spec §2's implied design), duplicate it per mode, or extract a shared base.
**Decision**: Extract `apps::SlotEditorScreen` (selection, InputLine edit lifecycle, dirty-band row invalidation, key dispatch, render loop) with per-mode virtuals (labels/text/toggle/clear/checkbox, `after_commit`). Y=, parametric, and later polar are thin subclasses. Done in two commits: pure extraction (no behavior change), then the parametric subclass.
**Rationale**: Three variants meet the rule of three. The D13 dirty-band footgun (a missed `invalidate()` = stale rows) lives in exactly one file instead of three; parametric keeps its natural pair-field model (2 fields/slot, checkbox on the X row) without if-mode branches in shared code.
**Tradeoffs**: Virtual-call indirection in the editor (irrelevant: keypress-rate). The extraction touched a HW-verified screen — mitigated by the mechanical two-step and a queued HW spot-check.
**Revisit when**: A mode editor needs a fundamentally different layout (e.g. scrolling lists for more slots) that the base's fixed row model can't express.

## D0: Track decisions in this file

**Date**: TBD (Phase 0)
**Status**: Accepted
**Context**: Solo project, but decisions made early (e.g., expression engine choice, HAL layering) need to be traceable later when their consequences surface.
**Decision**: Maintain `docs/notes/decisions.md` as a chronological log. Append entries as decisions are made, not retrospectively.
**Rationale**: Future-me will not remember why the math engine uses `double` instead of `float`, or why the framebuffer lives in PSRAM on Pico 1. A log is cheaper than re-deriving.
**Tradeoffs**: Minor maintenance overhead. Mitigated by keeping entries short (~10 lines).
**Revisit when**: Project becomes multi-developer and an ADR (Architecture Decision Record) format with separate files might be preferable.

---

## D14: Deferred PSRAM/SD late-init for the RP2350 cold-boot rail settle

**Date**: 2026-07-11
**Status**: Accepted
**Context**: First Pico 2 (RP2350) bring-up: display, keyboard, and battery came up fine, but PSRAM and SD both failed on **cold power-on** and both worked on every warm reboot. Instrumented cold-boot traces (buffered `dbg_log`, dumped over USB serial) measured the failure directly: at 0.5 s the PSRAM reads back zeros; at 0.6-2.5 s it returns almost-correct data (single bit errors, or the whole word shifted one bit — analog-marginal behavior, independent of SPI clock: same at 75 MHz and 18.75 MHz); at 7.5 s it is perfect. The SD card answers CMD0/CMD8 immediately (comms fine, R7 voltage echo clean) but never completes ACMD41 — its power-sensitive init — until, at 7.5 s, it inits instantly. Conclusion: the peripheral rail needs **~5-8 s to settle after cold power-on with the Pico 2 module**; the Pico 1 module doesn't exhibit this (Phase 1 was fully verified on it, same mainboard, same card). Community reports match (RP2350 PSRAM cold-boot failures; fuzix SD failure on PicoCalc Pico 2).
**Decision**: Do not block boot. Boot-time init runs as always (instant home screen); if PSRAM or SD failed, the main loop retries every 2 s for the first 30 s of uptime: PSRAM via `Psram::reinit()` (re-sends the chip reset through the already-configured PIO — deliberately not `psram_spi_init()`, which re-adds the PIO program and claims 2 DMA channels per call), SD via a fresh `Storage::init()` (f_mount re-runs `disk_initialize`). When storage arrives late, the self-tests re-run and history/variables/graph state load then; the current screen is fully invalidated so the UI reflects it.
**Rationale**: A calculator that boots in 0.3 s shouldn't stall 8 s on one board variant. Warm reboots and Pico 1 hit the success path at boot and never enter the retry loop.
**Tradeoffs**: During the first ~10 s of a Pico 2 cold boot, persistence isn't available yet and a failing SD attempt (card inserted, rail still low) blocks the loop up to ~1 s per retry — brief input lag if the user types immediately. History appears a few seconds after boot rather than instantly.
**Revisit when**: The rail settle is understood at the hardware level (measure 3V3 with a scope; possibly a PicoCalc mainboard/Pico 2 SMPS interaction), or a keyboard-firmware/mainboard revision changes the power path. If Phase 3/4 needs PSRAM immediately at boot, reconsider a short blocking wait with a splash.

## D13: Opt-in dirty-band partial redraw (rows, not rectangles)

**Date**: 2026-07-11
**Status**: Accepted; HW-verified same day (typing instant, no stale-row artifacts)
**Context**: With synchronous full-frame rendering (D10), every keypress cost ~200 ms — the SPI push dominates (recompute is only 15-17 ms), and push time is proportional to pixel count. Task 5.6 part 2.
**Decision**: Screens track a dirty **row band** (`[y0, y1)`, full width); `ScreenManager::render_frame()` consumes it and `Framebuffer::render_frame()` renders/pushes only the strips inside the band. Tracking is **opt-in** per screen (`track_dirty()` in the constructor + `invalidate()` on every state change in `on_key`); non-tracking screens keep full-frame redraws. Any screen surfacing to top of the stack is fully invalidated by the manager. Opted in: home screen (typing = input band, ~28 of 320 rows; Enter = everything above the softkeys, which also keeps the battery/mode status fresh) and the Y= editor (per-row bands). An empty band skips the render+push entirely, so unconsumed keys cost nothing.
**Rationale**: A y-band is enough — the hot regions (input line, editor rows) are full-width, so x-cropping would add a strided push path for no measurable win. Opt-in keeps the default safe: a screen that never calls `invalidate()` can't accidentally stop redrawing.
**Tradeoffs**: Tracking screens must invalidate every band their key handler touches — a missed call shows as stale rows on the panel (visible, not corrupting). The battery indicator refreshes only on Enter/screen changes, not per keystroke. Renderers still run their full draw code per band (clipping discards out-of-band work), so CPU cost is unchanged — fine while push time dominates.
**Revisit when**: Graph interactions need help (trace/zoom redraw the ~280-row plot area anyway, so bands don't win there — that wants a faster SPI clock, DMA, or plot-region caching); or a screen needs non-full-width updates.

## D12: Shell-style input recall on UP/DOWN; modifier+arrows scroll the view; HOME pops to root

**Date**: 2026-07-11 (revised same day after HW verification)
**Status**: Accepted; scroll modifier revised to Alt/Ctrl
**Context**: On hardware, UP recalled only the newest expression once, then further UP scrolled the output view — no way to walk back through older inputs. The HOME key did nothing visible.
**Decision**: Plain UP/DOWN walk backward/forward through past inputs (the in-progress line is stashed and restored); **Alt+UP/DOWN or Ctrl+UP/DOWN** scroll the history view. HOME pops to the home screen from any screen (global intercept in the main loop, like F6); on the home screen it falls through to the input line's cursor-to-start. *Revision:* Shift was the original scroll modifier, but HW verification (2026-07-11) showed the STM32 swallows Shift on arrow keys (it emits a shift-release then a plain arrow); Alt and Ctrl pass through with flags intact, so scroll moved to them. Shift is still accepted in case a future keyboard firmware reports it.
**Rationale**: Shell-style recall is the behavior every terminal user expects, and the keyboard has no PgUp/PgDn — shift is the only spare modifier and its state is already tracked in `KeyEvent`.
**Tradeoffs**: Editing a recalled entry then pressing UP discards the edit (bash-like, not zsh-like). View scrolling is now two-handed.
**Revisit when**: a keyboard firmware update reports arrows with shift held. Re-checked on fw v1.6 (2026-07-11): still swallowed — the kShift press arrives with no arrow event at all — so Alt/Ctrl scroll stands.

## D11: `e` is Euler's constant; variable E is reserved

**Date**: 2026-07-11
**Status**: Accepted (test-drive feedback)
**Context**: `e` evaluated to 0 on the device. `build_lookup()` bound all 26 letters as variables, and tinyexpr consults the user lookup before its builtin table — so the variable E shadowed the builtin Euler constant (`pi`, being two letters, never collided).
**Decision**: Do not bind the letter `e` as a variable; `e` reaches tinyexpr's builtin constant. Storing to E (`5->E`) returns "E is reserved (Euler's e)". Convention: single letters = variables, `pi`/`e`/`theta`/`ans` and function names = reserved words.
**Rationale**: A calculator where `e` isn't 2.718... fails the least-surprise test; TI users rarely store to E (on TI it's the exponent token anyway). Case sensitivity (e vs E) was rejected — the preprocessor lowercases everything and the win isn't worth reworking that.
**Tradeoffs**: 25 letter variables instead of 26. The E slot still exists in `Variables` storage (persisted file format unchanged).
**Revisit when**: Someone actually misses variable E.

## D10: Synchronous core-0 rendering; PSRAM bulk path and dual-core display deferred

**Date**: 2026-07-10
**Status**: Accepted (from first hardware bring-up)
**Context**: First flash to real PicoCalc (Pico 1) showed a screen of random colors and a dead keyboard. Bisecting on hardware (vendored-only diagnostic + USB-serial boot tracing) found three distinct bugs:
1. **Boot hang** — `run_self_tests()` called the vendored *bulk* PSRAM transfer (`psram_read`/`psram_write`, 1 KB), which hangs on this hardware, even though single-word `psram_read32`/`write32` work. Boot froze after display init but before the first draw, so the panel showed power-on noise ("random colors").
2. **Dual-core display stall** — routing strip pushes through a core-1 service over the multicore FIFO stalled on the first frame.
3. **Dead keyboard** — the I2C read/write timeouts (2 ms) were shorter than a 2-byte transfer on the 10 kHz keyboard bus (~3.5 ms), so every read timed out.
**Decision**:
- Render **synchronously on core 0** using the vendored blocking `spi_write_fast` path (proven good by the diagnostic). Core 1 is left idle; `display_service_main` and `push_rect_dma` are retained but unused, as the basis for a future revisit.
- Quarantine the **bulk PSRAM** API (`Psram::read`/`write`) as known-hanging; expose and use only single-word `read_word`/`write_word`. Phase 1 needs no bulk PSRAM (framebuffer is line-buffered in SRAM).
- Set the keyboard I2C timeout to 100 ms (`kI2cTimeoutUs`), comfortably above the 10 kHz transfer time.
- Rendering is **event-driven**: a full-frame push is ~200 ms (5 fps), so redraw only after a key press, not every loop.
**Rationale**: Get a correct, working calculator on hardware first. The DMA push, dual-core split, and bulk PSRAM are all optimizations/future-phase needs, not Phase 1 requirements; each is a separate investigation.
**Tradeoffs**: ~200 ms full-screen redraw latency per keypress (single-threaded, full-frame). Acceptable for a calculator; the fix is dirty-rectangle / partial updates (and possibly a faster SPI clock or revisiting DMA), tracked for task 5.6.
**Revisit when**: task 5.6 performance work — profile, then add partial updates and re-evaluate DMA/dual-core (the bulk PSRAM leg is resolved).

**Bulk-PSRAM leg RESOLVED 2026-07-18 (Session 10 round 3, HW-verified on the Pico 2).** Root cause: the PIO program takes 8-bit transfer counts (`out x, 8` / `out y, 8`), so one transaction maxes at 255 bits (31 bytes); the vendored `psram_write()`/`psram_read()` let the count byte wrap above 27/31 data bytes — `(4+count)*8 mod 256` — desyncing the PIO from the DMA byte stream (a wrapped count of 0 underflows `jmp x--` into a ~2^32-bit shift loop), wedging the blocking DMA wait forever. That was the Phase 1 boot hang; the upstream driver's own 4-byte-and-under fast paths never hit it. Fix: `Psram::read`/`write` now chunk internally (27-byte writes / 31-byte reads — also keeps CS-low under the chip's ~8 µs tCEM). Un-quarantined; guarded by a watchdog-armed boot self-test (hang → 2 s reboot → scratch-marker skip, no boot-loop) covering cap-straddling sizes, unaligned starts, and cross-chunk addressing. Measured on HW: 1 KB write 150 µs / read 156 µs (~6.8 MB/s). Dual-core display service remains deferred as before.

## D3: Trace coordinate readout at the bottom of the viewport

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision — show the trace `(x, y)` at the top or bottom of the graph? Top risks overlapping the plotted curves near the peak; bottom risks the softkey bar.
**Decision**: Bottom of the viewport, in a dark strip just above the softkey bar, matching the TI-84.
**Rationale**: Curves cluster around the top/middle more often than the very bottom edge; TI users expect it there.
**Tradeoffs**: A curve that dips to the bottom edge is briefly obscured by the readout. Acceptable.
**Revisit when**: A cleaner overlay (semi-transparent, or auto-repositioning away from the cursor) is worth the code.

## D5: Keep `double` for graph evaluation (float deferred)

**Date**: 2026-07-08
**Status**: Deferred (revisit after hardware profiling)
**Context**: Open decision — use `float` instead of `double` for graph point evaluation on Pico 1 (no FPU) to roughly halve softfloat cost?
**Decision**: Keep `double` (`math::calc_t`) everywhere for now, including the graph sweep. The compile-once/eval-many path already removes the dominant cost (re-parsing per point), so evaluation is 320 `te_eval`s per function, not 320 compiles.
**Rationale**: Correctness first; can't profile without hardware. `calc_t` is a single typedef, so a `float` graph-eval variant is a localized change if profiling shows plotting is too slow.
**Tradeoffs**: Softfloat `double` is ~2x slower than `float` on RP2040; may matter with 7 functions. Measured lever, not a guess.
**Revisit when**: Task 5.6 profiling on real Pico 1 hardware shows graph render missing the <50 ms target.

## D2: Fractions stack only for "simple" operands

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision — should `a/b` always render as a stacked fraction, or only when the operands are simple? Always-stacked is less code; a heuristic reads better for messy expressions.
**Decision**: Stack `a/b` into a `FractionNode` only when both sides are "simple" — a number, a variable, a parenthesized group, an already-built fraction, **a function call, or a power** (the last two added 2026-07-11). Otherwise render inline with a text `/`. Also require the division to be the first operator in its term (no chaining an inline `*` into a stacked fraction), so `a*b/c` stays inline.
**Rationale**: Matches the spec's section 6.2 guidance and TI behavior; `(x+1)/(x-1)` stacks (operands are parens) while `1+2/3+4` keeps `2/3` inline-sized within the sum. Keeps trees shallow and predictable.
**Tradeoffs**: A few expressions a user might expect stacked stay inline; acceptable and consistent.
**Revisit when**: User feedback, or when an equation editor needs full 2D editing (Phase 2+).
**Revision (2026-07-11)**: HW test drive hit the tradeoff — `1/sqrt(2)` rendered inline because a function call parses to an HBox. Calls (recognized structurally: `HBox[alpha-name, paren]`, which excludes unary-minus HBoxes) and superscripts now count as simple, so `1/sqrt(2)` and `x^2/2` stack.

## D1: Variable store operator is `->` (arrow)

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision from the spec — how to store a value into A-Z/theta. Options: TI `→`, `=`, `:=`, or a dedicated STO key.
**Decision**: Use ASCII `->` typed as two chars (e.g. `2->A`, `x^2->B`). The engine splits on the last `->` whose right side is a bare variable name; `=` stays free for future comparison/equation use.
**Rationale**: No special key mapping or font glyph needed now; reads clearly; avoids the `=` ambiguity the spec flagged. A dedicated STO key can emit `->` later without changing the engine.
**Tradeoffs**: Two keystrokes vs. one; `->` can't appear elsewhere in an expression (fine — it has no other meaning).
**Revisit when**: A physical STO/→ key is added, or equation solving needs `=`.

## D4: History persisted as plaintext TSV

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Open decision — history storage format: plaintext vs binary.
**Decision**: Append `expr\tresult\n` lines to `/picocalc/history.txt`. On boot, read the last 8 KB and parse backwards into the ring buffer. Variables persist separately as a binary blob (`variables.dat`, 28 doubles).
**Rationale**: Plaintext history is debuggable and hand-editable; parsing cost is trivial at 50 entries. Variables are fixed-size binary because they're not meant to be edited and round-trip exactly.
**Tradeoffs**: History file grows unbounded (append-only) — a compaction pass is a future cleanup; 8 KB tail read caps what's loaded regardless.
**Revisit when**: History file size becomes a concern, or results need structured metadata.

## D6: RGB565 framebuffers, RGB666 on the wire

**Date**: 2026-07-08
**Status**: Accepted
**Context**: The Coyote OS panel init programs COLMOD 0x66 (18-bit color, 3 bytes/pixel over SPI) — the ILI9488-family serial interface does not accept RGB565. The spec assumed RGB565 end-to-end.
**Decision**: Keep all render buffers RGB565 (as spec'd); convert to 3-byte RGB666 in `platform::Display` during push, using 5-to-8/6-to-8 bit LUTs, chunked through two 4-scanline staging buffers so conversion overlaps DMA.
**Rationale**: Preserves the spec's memory budget (2 B/px buffers) and the proven panel init. Conversion is a few cycles/pixel on core 1, which is otherwise idle waiting on SPI.
**Tradeoffs**: 50% more SPI traffic than true 565 (~98 ms/full frame @ 25 MHz — partial updates and/or a higher SPI clock are the perf levers; see worklog).
**Revisit when**: Profiling (task 5.6) shows the panel accepts COLMOD 0x55 at speed, or SPI overclocking changes the math.

## D7: Non-blocking keyboard poll state machine

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Vendored `read_i2c_kbd()` sleeps 16 ms between the FIFO register select and the data read — unacceptable in a per-frame poll loop.
**Decision**: `platform::Keyboard::poll()` reimplements the same I2C protocol (reg 0x09, addr 0x1F) as a two-phase non-blocking state machine (select, then read at least 10 ms later). The vendored driver still provides bus init and scan-code reference.
**Rationale**: Keeps the main loop responsive; drivers stay unmodified.
**Tradeoffs**: Two places know the STM32 protocol (vendored driver + wrapper).
**Revisit when**: STM32 firmware changes its register map, or an interrupt-driven design is needed.

## D8: FatFs local config — LFN enabled, CP437

**Date**: 2026-07-08
**Status**: Accepted
**Context**: Spec filenames (`variables.dat`) exceed 8.3; the FatFs default config has LFN off and Shift-JIS codepage tables (large flash cost).
**Decision**: `ffconf.h`: `FF_USE_LFN=1` (static buffer), `FF_CODE_PAGE=437`. Documented as a local modification (ffconf.h is FatFs's designated user-config file).
**Rationale**: Smallest change that supports the spec's file layout.
**Tradeoffs**: LFN=1 is not thread-safe — fine, all file I/O happens on core 0.
**Revisit when**: File I/O moves off core 0.

## D9: Interim 8x12 font (Coyote font1) instead of 8x16

**Date**: 2026-07-08
**Status**: Resolved 2026-07-18 (Session 10)
**Context**: Spec calls for an 8x16 font generated from a public-domain BDF; that conversion needs font tooling not yet in the repo.
**Decision**: Ship milestone 1 with the vendored Coyote OS `font1` (8x12, UTFT layout) behind `gfx::Font`, which reads any UTFT-format header. Generate proper 8x16 + 6x8 fonts before milestone 5.
**Rationale**: Unblocks all text rendering now; the Font abstraction makes the swap a data change.
**Tradeoffs**: Slightly smaller glyphs than designed; layout metrics tuned later.
**Resolution (2026-07-18)**: Swapped to **Spleen** (BSD-2-Clause, one family for both sizes): 8x16 main + **5x8 small** (`gfx::small_font()`, in place of the spec's 6x8 — Spleen has no 6x8 and 5x8 suits axis labels better). Tooling: `scripts/bdf_to_utft.py`; sources vendored in `drivers/spleen/`. This was also step 3 of the D17 permissive path — `font1` is no longer compiled in. Layout impact was a data change plus three row-height constants, as designed.

---

<!-- New decisions go above this line. Below: pre-Phase-0 decisions captured retrospectively from the spec & feasibility report. -->

## D-prelude-3: Use C++17 with the Pico SDK

**Status**: Accepted (pre-Phase-0)
**Decision**: C++17 in `src/`, plain C in vendored `drivers/`.
**Rationale**: C++17 gives `std::optional`, `std::variant`, `constexpr if`, structured bindings, and inline variables — all useful for the architecture. The Pico SDK is C with C++ wrappers; both languages mix cleanly. C++20 modules are not yet practical with the SDK.
**Tradeoffs**: Slightly larger binaries than C-only, but well within our 2 MB Pico 1 flash budget. Some templates and STL features are off-limits because they allocate; this is documented in `AGENTS.md`.

## D-prelude-2: Layered architecture with strict HAL discipline

**Status**: Accepted (pre-Phase-0)
**Decision**: `apps → ui → math/render → platform → drivers + Pico SDK`. Application code never calls Pico SDK functions directly.
**Rationale**: Dual-target support (Pico 1 + Pico 2) is the project's tightest constraint. Without HAL discipline, target-specific code metastasizes and the dual build becomes unmaintainable.
**Tradeoffs**: Some duplication in trivial wrappers. Worth it for testability and target portability.
**Revisit when**: Adding a third target (e.g., desktop simulator).

## D-prelude-1: Coyote OS as driver foundation

**Status**: Accepted (pre-Phase-0)
**Decision**: Vendor Coyote OS's C drivers (`lcdspi`, `i2ckbd`, `rp2040-psram`, `pwm_sound`) as read-only third-party code under `drivers/`.
**Rationale**: Coyote OS is the only known PicoCalc-native firmware with working drivers for our target hardware. Reimplementing them from scratch costs weeks and gains nothing.
**Tradeoffs**: We inherit any bugs in those drivers. Mitigated by wrapping them in `platform/` so fixes/workarounds happen at one layer.
**Revisit when**: A driver bug is unfixable from the wrapper layer.

## D-prelude-0: Pico SDK + CMake + Ninja

**Status**: Accepted (pre-Phase-0)
**Decision**: Use the official Raspberry Pi Pico SDK with CMake (Ninja generator). `CMAKE_GENERATOR=Ninja` is set in shell environment.
**Rationale**: Standard, well-supported, dual-target ready (`-DPICO_BOARD=pico` / `pico2`). Ninja is faster than Make for incremental builds and integrates better with clangd's `compile_commands.json`.
**Tradeoffs**: Requires Ninja installation. Negligible.
