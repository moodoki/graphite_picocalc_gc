# Pre-Phase-5 code-review + size-optimization pass — findings

Started 2026-08-02. Grounds the review flagged in
[`next-session.md`](next-session.md) "The next job" #1 and
[`size-optimization-ideas.md`](size-optimization-ideas.md) in **measured
numbers** rather than the rough map. Scope: measurement + trimming, not a
rewrite (per the ideas doc's non-goals).

## Tooling delivered (ideas doc item #1)

`scripts/size-report.sh [build/pico|build/pico2] [top-N]` — prints
`arm-none-eabi-size` totals, computes nominal SRAM headroom for the board,
and lists the largest **demangled** bss/data symbols. Repeatable and
diffable (save-before / save-after / `diff`), so a change's cost is a
tracked number, not a hand-copied one. This replaces the manual `.map`
read that fed `worklog.md` each session.

## Measured baseline (Pico 1, RP2040, HEAD `1073f4f`)

```
text 420,416   data 0   bss 222,528   → ~46 KB nominal SRAM headroom (264 KB)
```

Top bss consumers:

| bytes | symbol | nature |
|------:|--------|--------|
| 57,516 | `math::array_store()::instance` | live storage — 28×2 KB slabs + overhead |
| 20,480 | `gfx::…::strip_buf` | Pico-1 render strips — needed |
| 12,936 | `apps::graph_screen()::instance` | GraphState + point caches |
| 12,288 | `math::listexpr::…::g_lift` | **scratch** — `calc_t[6][256]` list lift |
| 9,876 | `graph::…::g_points` | graph point cache |
| 8,192 | `apps::HomeScreen::load_state()::tail` | **scratch** — boot-time history read |
| 8,192 | `render::…::g_pool` | layout pool |
| 8,192 | `math::listexpr::…::g_op_lift` | **scratch** — `calc_t[4][256]` operand lift |
| 7,692 | `apps::home_screen()::instance` | screen singleton |
| 7,680 | `platform::…::staging` | display staging |
| 3,584 | `math::stats::…::sinusoid_fit::acc` | **scratch** |
| 3×3,200 | `math::matops::…::g_row{a,b,c}` | **scratch** — matrix row buffers |
| 6×2,048 | `math::stats::…::g_{bx,by,bf,buf_a,buf_b,row_sum}` | **scratch** |
| 2,048 ×N | `listops::g_buf`, `listexpr::g_outbuf`, `array/named g_chunk`, `graph::g_buf_{x,y}`, `ListEditor::delete_row::buf` | **scratch** |

## Headline finding — mutually-exclusive scratch buffers (~40 KB reclaimable)

The rough map put ArrayStore (56 KB) first, but a **comparable amount is
tied up in per-module worst-case scratch buffers** — all the same
256-element (`kChunk`) PSRAM-streaming chunk pattern, one private set per
module: `list_expr` (~22.5 KB), `matrix`/`matops` (9.6 KB), `stats`
(~15.9 KB incl. `sinusoid_fit::acc`), `listops` (2 KB), `array`/
`named_lists` persistence (`g_chunk`, 2×2 KB), the home-history `tail`
(8 KB), and `ListEditor::delete_row::buf` (2 KB).

**These are temporally mutually exclusive** — verified, not assumed:

- The calculator evaluates one thing at a time on core 0.
- `list_expr`'s streaming loop (`list_expr.cpp:958-982`) lifts list elements
  into `g_lift`/`g_op_lift`, then per element calls
  `engine().eval_compiled_raw(h)` over **already-lifted scalars** — it does
  not re-read lists or call stats/matrix code while `g_lift` is live.
- Stats reductions (`mean`/`stdev`/`median`/…) are **not** engine functions
  (absent from `functions.cpp`); they run only from the stats screen. So
  `g_bx`/… are never nested inside a list expression or matrix op.
- `matops::g_row{a,b,c}` are used only within `matrix.cpp` row algebra.

**Direction:** fold the mutually-exclusive families into a **single shared
scratch arena** sized to the worst-case single operation (~22.5 KB, the
`list_expr` case). Everything else carves from it instead of owning private
bss. Estimated reclaim **~40 KB**, which would roughly **double** the
current ~46 KB headroom — and it leaves ArrayStore's *live* 56 KB alone
(lower risk than shrinking real storage).

**Gating verification before implementing (the real review work):**

1. Confirm no *single* evaluation holds two families live at once — chiefly
   the **List↔Matrix conversion glue (4D.12)** and any matrix-built-from-lists
   path. If a conversion streams a list (`g_lift`/`g_chunk`) while a matrix
   row buffer (`g_rowa`) is live, those two cannot share a slot.
2. Add a lightweight **scoped-owner guard** (e.g. a debug-only assert that
   the arena has one owner at a time) so future code can't silently
   re-introduce nesting. This is the safety mechanism that makes the union
   maintainable rather than a latent aliasing bug.
3. `static`-local buffers (`load_state::tail`, `sinusoid_fit::acc`,
   `delete_row::buf`) need care when folded — they're function-scoped today.

## Secondary findings

- **`g_chunk` duplication:** `array.cpp:15` and `named_lists_persist.cpp:37`
  each define a private `calc_t g_chunk[256]` (2 KB each). Both are
  persistence-streaming scratch, used only at save/load — never concurrently.
  Trivially foldable into the shared arena (or each other).
- **ArrayStore slab sizing (ideas #3):** 57.5 KB is still the single largest
  number, but it's *live storage*, so trimming it trades headroom for array
  capacity — a capacity/behaviour decision, not a free win. Defer behind the
  scratch-arena work; revisit only if the ~40 KB above proves insufficient
  for the Phase 6 MicroPython budget (56 KB).
- **`MinSizeRel`/`-Os` (ideas #2):** not yet measured. Quick experiment —
  build both configs, diff `text` with `size-report.sh`, and spot-check graph
  recompute timing on device to judge the speed cost. Low effort; do it as a
  standalone measurement so the number exists.

## Shared scratch arena — IMPLEMENTED 2026-08-02 (~21.8 KB reclaimed)

Built `src/math/scratch.{hpp,cpp}`: one `alignas(16)` arena, two disjoint
regions (see the header contract). Buffers rebound onto it by
reference-aliasing, so **all call sites are unchanged**:

- **kCompute** (22,528 B) — `list_expr` (g_lift/g_op_lift/g_outbuf),
  `stats` (g_bx/g_by/g_bf), `infer` (g_buf_a/g_buf_b/g_row_sum), `matops`
  (g_rowa/g_rowb/g_rowc, placement-new'd — RowBuf has a non-trivial ctor).
  All overlay the same bytes; each TU `static_assert`s its layout fits.
- **kListops** (5,120 B) — `listops` (g_buf/g_in_a/g_in_b/g_out), kept
  disjoint because `list_expr` calls `listops`.

**Concurrency verification that gated this** (all confirmed by code read):
`list2mat`/`mat2list` use element-at-a-time `cget`/`set`, never the chunk
buffers; `Array::fill`'s own `g_chunk` was deliberately **left out** of the
arena (it can nest inside a compute op); `list_expr`'s per-element eval runs
over already-lifted scalars; `infer` never calls `stats`; math is core-0
only (core 1 does display).

**Result (measured, `size-report.sh`):**

| board | bss before | bss after | reclaim | headroom |
|-------|-----------:|----------:|--------:|---------:|
| Pico 1 (RP2040) | 222,528 | 200,704 | −21,824 | 46 → 68 KB |
| Pico 2 (RP2350) | 406,208 | 384,384 | −21,824 | (board-independent) |

Host suite green (1627 checks — incl. `test_lists`/`test_infer`/
`test_matrix`/`test_stats`, which drive the shared buffers with real data);
both boards build clean; `lint.sh`/`format.sh` clean.

**Deferred — the debug owner-guard.** The runtime one-owner assertion is
*not* in this cut: correct placement spans many entry points (esp. matops'
internal recursion) and a half-wired guard risks more than it catches, while
the extensive host coverage already exercises every aliased path. The
invariant is instead encoded as the loud `scratch.hpp` contract + per-TU
`static_assert`s. Adding the guard remains a recommended follow-up before
any change that makes a kCompute owner call another. **HW verification on
device is still pending** (host suite is exhaustive on logic, but this
touches hot math paths — worth a flash + matrix/stats/list/infer spot-check).

## Remaining sequencing

3. **Fold the persistence `g_chunk`s** — `array.cpp`'s stays out (nests in
   compute via `fill`); the three `*_persist.cpp` ones are save/load-only
   and could share a small persistence slot. Minor (~6 KB), lower priority.
4. **Measure `-Os`** and record the text/speed tradeoff.
5. **Re-verify the Phase 6 MicroPython 56 KB budget** against the new ~68 KB
   headroom (ideas #6) — now with real margin to check against.
6. ArrayStore slab re-sizing only if still needed after the above (57.5 KB,
   but live storage — a capacity decision, not a free win).

## Non-goals (unchanged)

Not a rewrite; not an open-ended optimization project. Bounded pass to buy
headroom before Phase 5 (CAS pool) and Phase 6 (MicroPython heap) earmark it.
