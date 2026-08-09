# Phase 5.2 measurements — task 5.2.12, on-device

**Both boards, 2026-08-09.** This is the evidence behind **D52**, and the
document Phase 5.2's closure should cite for its performance claims. The prose
lives in D52; the numbers, the method, and the raw per-sample data live here.

Raw runs are committed alongside this file as JSON — four files, one per
(board x build), each carrying every timing sample rather than only the median,
so anything asserted below can be re-derived:

| file | board | firmware |
|---|---|---|
| `B-pico2.json` | Pico 2 | v0.3.2 + probe (`matexpr`/`listexpr`/`complexexpr`) |
| `N-pico2.json` | Pico 2 | `phase-5.2` + probe (unified evaluator) |
| `B-pico1.json` | Pico 1 | v0.3.2 + probe |
| `N-pico1.json` | Pico 1 | `phase-5.2` + probe, **including D53's fix** (`3b7aa2d`) |

**One asymmetry to know before quoting these**: `N-pico1.json` was re-taken after
D53's block-read fix, because the first Pico 1 run lost M3 to that defect — the
stability guard rejected the row when the displayed answer changed between
repetitions. `N-pico2.json` predates the fix. The fix sits *inside* the probe
window (it changes result formatting), so this was worth checking rather than
assuming: every other Pico 1 row reproduced within **0.02-0.05 ms** across the
two builds, i.e. **the fix is timing-neutral** and the Pico 2 figures are not
stale because of it.

## What is being measured, and what is not

**`eval_us`, a firmware probe around evaluation only** — from the top of
`evaluate_input` to entry of `push_entry`, the funnel every branch passes
through. That covers evaluation **plus result formatting**, and excludes the SD
history write and rendering.

**It is deliberately not the host round trip that spec §9 specified.** That
method was tried first and cannot resolve the evaluator; §9's amendment and D52
record why, and `scripts/ab-measure.py`'s docstring repeats it so nobody
reintroduces it. The short version:

- A submit triggers a full-frame push, giving a **~113 ms floor with ~80 ms
  spread** against an evaluator cost of 0.5-40 ms.
- The clinching observation is the *ordering*, not the ratio: the 999-element M2
  row had a **lower minimum (80 ms) than `2+3*4` (104 ms)**.
- §9's "enough repetitions cancel it" fails specifically, because the push cost
  depends on **the result being rendered** — it is correlated with the thing
  under test, not independent overhead.
- Timing whole `submit_line` fails too: it contains the SD write. `2+3*4`
  measured 19.0 ms of which **0.63 ms** was evaluation.

**The baseline is a rebuild of the v0.3.2 tag carrying the same probe**, not the
released binary §9 called for. The probe was applied to both trees by one script
so the instrumentation is provably identical rather than hand-matched twice.
This *strengthens* the M5 control: v0.3.2 differs from `phase-5.2` by the
evaluator work alone, so M5's movement is dispatch overhead and not the
"different commit" confound §9 was guarding against.

Median of 15, first run per line discarded (cold caches, PSRAM wake). **REAL
mode is pinned before every run** — in `a+bi` the answer comes from
`complexexpr` and the comparison measures nothing. One whole pass was
invalidated by exactly that.

## Results

Evaluation time in milliseconds. "Spread" is max-min across the 15 samples —
**every delta below is 10-100x its own spread**, which is what makes them
readable at three decimal places.
#### Pico 2 (RP2350)

| # | input | baseline | 5.2 | delta | spread (base / 5.2) |
|---|---|---|---|---|---|
| M1 | `2+3*4` | 0.884 | **0.627** | -0.257 (-29.1%) | 0.184 / 0.055 |
| M1 | `sin(30)+ln(2)` | 1.487 | **1.165** | -0.322 (-21.7%) | 0.086 / 0.035 |
| M2 | `sin(l1)+2*l2` | 10.889 | **16.552** | +5.663 (+52.0%) | 0.040 / 0.040 |
| M3 | `l1/sum(l1)` | 8.457 | **7.019** | -1.438 (-17.0%) | 0.077 / 0.052 |
| M4 | `sum(sin(l3))` | 2.116 | **1.442** | -0.674 (-31.9%) | 0.034 / 0.036 |
| M4 | `sum(sin(l4))` | 3.412 | **2.676** | -0.736 (-21.6%) | 0.056 / 0.029 |
| M5 | `det([A])` | 0.491 | **0.637** | +0.146 (+29.7%) | 0.029 / 0.029 |
| M5 | `[A]*[B]` | 0.552 | **0.634** | +0.082 (+14.9%) | 0.020 / 0.031 |
| M6 | `seq(x^2,x,1,200,1)` | 1.851 | **2.847** | +0.996 (+53.8%) | 0.051 / 0.136 |

#### Pico 1 (RP2040)

| # | input | baseline | 5.2 | delta | spread (base / 5.2) |
|---|---|---|---|---|---|
| M1 | `2+3*4` | 1.318 | **0.899** | -0.419 (-31.8%) | 0.045 / 0.054 |
| M1 | `sin(30)+ln(2)` | 2.090 | **1.519** | -0.571 (-27.3%) | 0.024 / 0.022 |
| M2 | `sin(l1)+2*l2` | 28.552 | **38.793** | +10.241 (+35.9%) | 0.040 / 0.048 |
| M3 | `l1/sum(l1)` | 13.850 | **12.610** | -1.240 (-9.0%) | 0.072 / 0.039 |
| M4 | `sum(sin(l3))` | 5.851 | **5.045** | -0.806 (-13.8%) | 0.041 / 0.121 |
| M4 | `sum(sin(l4))` | 7.658 | **6.775** | -0.883 (-11.5%) | 0.072 / 0.087 |
| M5 | `det([A])` | 0.714 | **0.895** | +0.181 (+25.4%) | 0.038 / 0.041 |
| M5 | `[A]*[B]` | 0.965 | **1.055** | +0.090 (+9.3%) | 0.031 / 0.035 |
| M6 | `seq(x^2,x,1,200,1)` | 3.821 | **6.872** | +3.051 (+79.8%) | 0.112 / 0.190 |

## Reading these

- **M1 is the guardrail** and it is *better*, not merely unchanged: REAL mode no
  longer evaluates twice. The old dispatcher ran `complexexpr` as a probe purely
  to ask "would this be non-real?", then `engine` for the answer — the pair
  register row D4 describes. One evaluator answers it once.
- **M2 is the regression §3 promised to report either way**, and it is the
  *predicted* one. +5.66 ms over 999 elements is 5.7 us/element; two extra
  streaming passes x 8 KB x read+write at D10's measured ~6.8 MB/s predicts
  ~4.8 ms. The three-pass lift costs what §3 said it would.
- **M3 confirms the other half of §3's reasoning** — hoisting the loop-invariant
  reduction wins on both boards.
- **M4 improves on both sides of 256**, which is the **SRAM/PSRAM threshold**
  (`kSlabBytes = 2048`, D21), not a streaming chunk boundary. 5.2.12 initially
  mis-read it as one.
- **M5 moved, and that is a cost rather than an invalidation.** §9 itself calls a
  difference there dispatch overhead rather than arithmetic — it is compiling a
  `Program` before running it. Judge it in **milliseconds, not percent**: on a
  0.5 ms row a percentage says more about the divisor than the change. The first
  version of the comparison script failed the control on percentage alone.
- **M6 is the second regression**, quoted-body re-entry against `listexpr`'s
  per-element tinyexpr compile, and it is markedly worse on the Pico 1 (+80%)
  than the Pico 2 (+54%).

**M7 is absent by design.** §9 lists it as "the register's replay script end to
end", which is the differential pass, not a latency row: its expected output is
the change register's own table. Timing it would average ~250 unrelated
expressions and mean nothing. It ran clean on both boards (31 inputs, 36 lines,
no faults).

## Non-latency results from the same pass

| | baseline | 5.2 |
|---|---|---|
| worst stack peak, Pico 2 | 3,972 of 4,096 | **2,344** |
| worst stack peak, Pico 1 | **hard fault** (see D48's amendment) | **3,068** |
| paren nesting | fails at 16 | **62+** |
| matrix nesting | fails at 4 | **14+** (line-length limited, not evaluator limited) |
| operand depth | n/a | **64 exact; 65 = "Too deeply nested", no fault** |
| `.bss`, Pico 1 | 215,868 | **210,508** (-5,360 B) |
| `.bss`, Pico 2 | 399,548 | **394,200** (-5,348 B) |

**Two caveats on those, both recorded rather than smoothed over.** The `.bss`
delta **does not match the phase's own claimed -6,888 B**, and v0.3.2 differs
from the branch point only by D51 (a parse-time fix with no static data), so the
baseline choice does not explain a ~1.5 KB gap — find out which figure counted
what before quoting either. And "depth costs no call frames" holds for
scalar/paren/unary nesting (the ladder plateaus; depths 32-63 register no new
high-water mark at all) but **not for matrix nesting, which costs ~104 B/level**
by a mechanism nobody has explained. Input length was ruled out (2 to 30 flat
terms, zero new marks) and so was the CAS exact-form probe (`>dec` suppresses
it; peaks still climb).

## Reproducing

```bash
# per board: flash the baseline, measure, flash the new build, measure, compare
picotool load -f -x <v0.3.2+probe>.uf2
python3 scripts/ab-measure.py --out B.json --label "v0.3.2+probe" -n 15
picotool load -f -x build/pico/picocalc_graphcalc.uf2
python3 scripts/ab-measure.py --out N.json --label "phase-5.2+probe" -n 15
python3 scripts/ab-measure.py --compare B.json N.json
```

The baseline build is a worktree at the `v0.3.2` tag with the probe applied. The
probe is three edits — a timer in `home_screen.cpp`, an accessor in the header,
and `eval_us=` on the inject echo in `main.cpp`; `phase-5.2` carries it
permanently, so only the baseline tree needs patching. Apply it with a script to
both trees and diff the result, rather than by hand twice: the whole comparison
rests on the two probes being identical.

## See also

- **D52** — the decision entry: results in prose, and why the method changed.
- **D53** — the PSRAM read defect this pass found; still open.
- **D48's amendment** — the depth cap that does not hold on the Pico 1.
- `phase5.2-spec.md` §9 and its amendment — the original plan and its correction.
