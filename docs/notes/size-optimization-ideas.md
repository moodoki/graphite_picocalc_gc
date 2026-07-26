# Size optimization — ideas and strategy (research starter)

This is a starting point for the "code review + size optimization pass"
flagged in `next-session.md` before Phase 5 (CAS) and Phase 6 (app
framework + MicroPython). It is deliberately high-level — directions to
investigate, not a design or an implementation plan. Pick an item, dig in,
and turn it into a real decision (`decisions.md`) once it's been scoped.

## Why now

SRAM headroom on the Pico 1 (RP2040, 264 KB total SRAM) has been trending
down each phase: ~76 KB headroom after Phase 4A (D28), ~57 KB pre-Phase-4D,
~48 KB as of the 2026-07-27 build (bss 222,520 B). Nothing is on fire, but
the trend line matters more than the current number, because both
upcoming phases add fixed cost before any new feature work:

- **Phase 5 (CAS)**: adds a dedicated pool (64 KB PSRAM on Pico 1, 128 KB
  SRAM on Pico 2 — already board-split to protect Pico 1's budget).
- **Phase 6 (MicroPython)**: budgeted at 56 KB SRAM on Pico 1 (48 KB heap
  + 8 KB stack), which the spec itself calls "tight" — leaving only ~80 KB
  for everything else once that heap is live. The spec already flags this
  as a risk to re-verify against 4D's actual cost before committing.

Point being: whatever headroom exists gets earmarked, not found, so a
trim pass now is worth more than the same trim pass later.

## What's known to be consuming SRAM today

A rough map, useful for prioritizing where to look first:

- **`ArrayStore`'s 28 x 2 KB SRAM slabs — 56 KB fixed bss.** The single
  largest known static allocator. Worth asking whether 28 slabs at 2 KB
  each is still the right shape, or whether usage patterns since it was
  sized would support fewer/smaller slabs with PSRAM as the overflow
  (which is already the design for anything larger).
- **`GraphState` growth**, notably a persistence-image *mirror* that
  doubled the cost of one addition (SeqFunctions, +1,176 B). Worth
  checking whether other state structs carry a similar in-memory mirror
  of their on-disk format that could instead be serialized on demand.
- **Per-slot expression buffers** (`char expr[slots][256]`) sized for a
  worst case per slot. Worth checking whether 256 B is actually typical,
  or whether a shorter default + occasional truncation warning would
  claw back real bytes across many slots.
- **Framebuffer strategy is already board-split** (Pico 1 uses two
  320x16 strips, ~20 KB; Pico 2 uses a full 320x320 buffer, ~200 KB,
  gated out entirely on Pico 1). This is a good existing example of the
  kind of tradeoff worth replicating elsewhere — not itself a target.
- **Fonts and PSRAM-backed arrays are NOT SRAM costs** — fonts are
  compile-time `const` glyph tables in flash/`.rodata` (only one font
  compiled in per build), and large/complex arrays already route through
  PSRAM by design (D21). Don't waste research time here; the wins are
  elsewhere.

## Candidate directions to research

Roughly in order of likely effort-to-payoff, but untested — that's the
point of the research pass:

1. **Get real numbers before guessing.** There's currently no automated
   size report — bss/text numbers are hand-copied from a `.map`/`size`
   read into `worklog.md` each session. A repeatable script (parse the
   `.map` file, or run `arm-none-eabi-size` post-build and diff against
   the last known number) would turn "headroom is shrinking" from a
   manual observation into a tracked metric, and would surface which
   commit/feature actually cost what.
2. **Try `MinSizeRel` (`-Os`) and measure.** The build currently uses
   Release (`-O3 -DNDEBUG`) unconditionally. Nobody has tried a size-
   optimized build to see the code-size vs. runtime-speed tradeoff on
   this specific codebase — worth a build + benchmark to know if it's
   even a lever worth pulling.
3. **Audit `ArrayStore`'s slab sizing against real usage.** 56 KB fixed
   is the biggest single number on the board. Is it over-provisioned for
   how arrays actually get used in practice, and could the PSRAM overflow
   path absorb more of that load without a perf regression?
4. **Look for other mirrored/duplicated state**, following the
   `GraphState`/SeqFunctions pattern — anywhere else that keeps an
   in-memory copy of exactly what's about to be written to disk, when it
   could serialize from the live struct directly instead.
5. **Right-size worst-case buffers** (expression strings, similar
   per-slot fixed arrays) against what's actually typed in practice,
   rather than the theoretical maximum.
6. **Re-verify the Phase 6 MicroPython budget against current reality**
   before committing to it — the spec's own risk list already asks for
   this; folding it into this same pass avoids doing the measurement
   twice.
7. **General dead-code/unused-feature sweep** — anything still compiled
   in from an earlier phase that no longer needs to be, following the
   precedent already set by the single-font-at-a-time build switch.

## Non-goals for this pass

- Not a rewrite. Nothing above requires restructuring working
  subsystems — this is about measurement and trimming, not redesign.
- Not blocking Phase 5/6 indefinitely — the point is a bounded pass, not
  an open-ended optimization project.
