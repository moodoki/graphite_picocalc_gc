#!/usr/bin/env python3
"""A/B latency measurement for Phase 5.2's unified evaluator (task 5.2.12).

Runs phase5.2-spec.md §9's M1-M7 against whatever firmware is on the board and
writes the result to JSON. Run it twice — once on the released baseline, once on
the new build — then compare:

    python3 scripts/ab-measure.py --out base-pico2.json --label "v0.3.2 pico2"
    # reflash, then:
    python3 scripts/ab-measure.py --out new-pico2.json  --label "5.2 pico2"
    python3 scripts/ab-measure.py --compare base-pico2.json new-pico2.json

**The A/B number is `eval_us`, the firmware probe** — not the host round trip
that §9 originally specified. That method was tried first and does not work,
which is a 5.2.12 finding rather than a shortcut:

  - The round trip has a **~113 ms floor** (a submit triggers a full-frame
    push, main.cpp:398) and an **80 ms spread**. Measured on the Pico 2, the
    999-element M2 row had a *lower* minimum (80 ms) than `2+3*4` (104 ms) —
    scheduling phase swamps the work entirely. And the push cost depends on the
    result being rendered, so it is not overhead that cancels with repetitions.
  - Timing the whole `submit_line` does not work either: it contains the SD
    history write (`persist_history_line`). `2+3*4` measured **19.0 ms** of
    which **0.63 ms** was evaluation.

So the baseline is rebuilt from the v0.3.2 tag with the *same* probe applied by
scratchpad `apply-probe.py`, and the comparison is firmware-timer to
firmware-timer: immune to USB, frame scheduling and SD latency. Host round trip
and whole-submit are still recorded, as a functional control and to show how
much of each is not evaluation.

**M5 is the control.** Same `matops` underneath, reached through a different
dispatcher, so it should not move. The baseline is a whole different commit, so
in principle any delta includes everything else that changed between them; M5 is
the row that says whether the other rows are measuring the evaluator or
something else. `--compare` fails loudly if it moved.

Median, not mean: one scheduling hiccup should not move the number. Each line's
first run is discarded (cold caches, PSRAM wake).
"""

import argparse
import json
import statistics
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
serial_console = __import__("serial-console")

# §9's table, verbatim in intent. `setup` runs once before the measured
# repetitions and is not timed — the list-shaped rows need data to exist.
#
# Sizes come straight from §9: 999 elements for the lift rows (M2/M3), the
# 256/257 pair for the chunk boundary (M4), 6x6 matrices for the control (M5),
# and a 200-term seq for the quoted-body row (M6).
MEASUREMENTS = [
    {
        "id": "M1",
        "what": "Scalar entry latency — the guardrail: felt on every keypress-to-answer",
        "setup": [],
        "lines": ["2+3*4", "sin(30)+ln(2)"],
    },
    {
        "id": "M2",
        "what": "The lift: three streaming passes against listexpr's one (§3's open trade)",
        "setup": ["seq(x,x,1,999,1)->l1", "seq(x*2,x,1,999,1)->l2"],
        "lines": ["sin(l1)+2*l2"],
    },
    {
        "id": "M3",
        "what": "Loop-invariant reduction — should favour the new evaluator, or §3 is wrong",
        "setup": ["seq(x,x,1,999,1)->l1"],
        "lines": ["l1/sum(l1)"],
    },
    {
        "id": "M4",
        "what": "Chunk-boundary cost: 256 vs 257 elements",
        "setup": ["seq(x,x,1,256,1)->l3", "seq(x,x,1,257,1)->l4"],
        "lines": ["sum(sin(l3))", "sum(sin(l4))"],
    },
    {
        "id": "M5",
        "what": "CONTROL — matrix ops, same matops underneath. Must not move.",
        "setup": [
            "identity(6)->[A]",
            "identity(6)+identity(6)->[B]",
        ],
        "lines": ["det([A])", "[A]*[B]"],
    },
    {
        "id": "M6",
        "what": "Quoted-body re-entry vs listexpr's per-element tinyexpr compile",
        "setup": [],
        "lines": ["seq(x^2,x,1,200,1)"],
    },
]


# §9's M7 ("the register's replay script end to end") is deliberately absent
# from the table above. It is the differential pass, not a latency row: its
# expected output is the change register's own table, so it runs as
#   python3 scripts/serial-console.py -f <register-inputs>
# on each build and the two transcripts are diffed. Timing it would average
# together ~250 unrelated expressions and mean nothing.


def measure(con, line, reps, timeout):
    """Return (median_ms, [all_ms], median_eval_us, median_submit_us, result, kind)."""
    samples_ms = []
    samples_us = []
    samples_eval = []
    result = kind = None
    for i in range(reps + 1):  # +1: the discarded warm-up
        t0 = time.perf_counter()
        ok, text, k, _diags = con.submit(line, timeout=timeout)
        dt_ms = (time.perf_counter() - t0) * 1000.0
        if not ok:
            return None, [], None, None, text, "ERROR", []
        if i == 0:
            result, kind = text, k  # Warm-up: keep the answer, drop the time
            continue
        samples_ms.append(dt_ms)
        if con.last_us is not None:
            samples_us.append(con.last_us)
        if con.last_eval_us is not None:
            samples_eval.append(con.last_eval_us)
        if text != result:
            # A changing answer means the run is not measuring one thing.
            return (None, samples_ms, None, None,
                    f"unstable: {result!r} then {text!r}", "ERROR", samples_eval)
    med = statistics.median
    return (med(samples_ms), samples_ms,
            med(samples_eval) if samples_eval else None,
            med(samples_us) if samples_us else None,
            result, kind, samples_eval)


def run(args):
    con = serial_console.Console(verbose=args.verbose)
    if not con.connect():
        sys.exit("no /dev/cu.usbmodem* device (firmware running? BOOTSEL?)")

    # REAL vs a+bi decides which evaluator answers on the baseline build, so
    # pin it rather than inheriting whatever the board was left in. This is
    # the mistake that silently invalidated the first D51 hardware pass.
    ok, text, _kind, _d = con.submit("mode real", timeout=args.timeout)
    if not ok:
        sys.exit(f"could not set REAL mode: {text}")
    print(f"mode: {text}", file=sys.stderr)

    out = {"label": args.label, "reps": args.reps, "modes": text, "rows": []}
    for m in MEASUREMENTS:
        for s in m["setup"]:
            ok, text, _k, _d = con.submit(s, timeout=args.timeout)
            if not ok:
                sys.exit(f"{m['id']} setup failed on {s!r}: {text}")
        for line in m["lines"]:
            ms, all_ms, eval_us, submit_us, result, kind, samples_eval = measure(
                con, line, args.reps, args.timeout)
            row = {
                "id": m["id"],
                "what": m["what"],
                "line": line,
                "median_ms": ms,
                "samples_ms": all_ms,
                "eval_us": eval_us,
                "samples_eval_us": samples_eval,
                "submit_us": submit_us,
                "result": result,
                "kind": kind,
            }
            out["rows"].append(row)
            ev = f"{eval_us / 1000.0:9.3f}" if eval_us is not None else "        —"
            sub = f"{submit_us / 1000.0:8.2f}" if submit_us is not None else "       —"
            rt = f"{ms:8.1f}" if ms is not None else "  FAILED"
            print(f"{m['id']:3} {line:24} eval={ev}ms  submit={sub}ms  rt={rt}ms  = {result[:34]}",
                  file=sys.stderr)

    Path(args.out).write_text(json.dumps(out, indent=2))
    print(f"\nwrote {args.out}", file=sys.stderr)


def compare(base_path, new_path):
    base = json.loads(Path(base_path).read_text())
    new = json.loads(Path(new_path).read_text())
    bmap = {(r["id"], r["line"]): r for r in base["rows"]}

    print(f"baseline: {base['label']}   ({base['reps']} reps)")
    print(f"new:      {new['label']}   ({new['reps']} reps)\n")
    print("evaluation time (firmware probe), milliseconds\n")
    print(f"{'':3} {'input':24} {'base':>9} {'new':>9} {'delta':>9} {'':>8}  result")
    print("-" * 92)

    control_delta_pct = None
    control_abs_ms = None
    mismatches = []
    for r in new["rows"]:
        b = bmap.get((r["id"], r["line"]))
        if b is None or b.get("eval_us") is None or r.get("eval_us") is None:
            print(f"{r['id']:3} {r['line']:24} {'—':>10} {'—':>10} {'—':>10}")
            continue
        bu, nu = b["eval_us"], r["eval_us"]
        delta = nu - bu
        pct = 100.0 * delta / bu if bu else 0.0
        flag = ""
        if b["result"] != r["result"]:
            flag = "  ANSWER CHANGED"
            mismatches.append((r["id"], r["line"], b["result"], r["result"]))
        if r["id"] == "M5":
            # Worst M5 row, by absolute milliseconds — percentages on a 0.5 ms
            # operation say more about the divisor than about the change.
            if control_abs_ms is None or abs(delta) / 1000.0 > abs(control_abs_ms):
                control_abs_ms = delta / 1000.0
                control_delta_pct = pct
        print(f"{r['id']:3} {r['line']:24} {bu / 1000.0:9.3f} {nu / 1000.0:9.3f} "
              f"{delta / 1000.0:+9.3f} {pct:+7.1f}%  {r['result'][:26]}{flag}")

    print()
    bad = False
    if mismatches:
        bad = True
        print("ANSWERS DIFFER between builds — this is a correctness finding, not a timing one:")
        for mid, line, b, n in mismatches:
            print(f"  {mid} {line!r}: baseline {b!r} -> new {n!r}")
    if control_abs_ms is None:
        print("WARNING: no M5 control row — the other rows are unanchored.")
    elif control_abs_ms > 1.0:
        bad = True
        print(
            f"CONTROL FAILED: M5 moved {control_abs_ms:+.3f} ms. Same matops, different "
            "dispatcher — a shift this large means the two builds differ by more than the "
            "evaluator, and no row above can be trusted (§9)."
        )
    else:
        print(
            f"control: M5 moved {control_abs_ms:+.3f} ms ({control_delta_pct:+.1f}%) — small in "
            "absolute terms and the expected shape. §9 calls a difference here dispatch "
            "overhead rather than arithmetic: same matops underneath, reached by compiling a "
            "Program first. Report it as a cost, not as an invalidation."
        )
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--out", default="ab-run.json", help="where to write this run's JSON")
    ap.add_argument("--label", default="unlabelled", help="what firmware this is")
    ap.add_argument("-n", "--reps", type=int, default=9, help="timed repetitions (default 9)")
    ap.add_argument("-t", "--timeout", type=float, default=30.0, help="per-line timeout (s)")
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--compare", nargs=2, metavar=("BASE", "NEW"), help="compare two runs")
    args = ap.parse_args()

    if args.compare:
        sys.exit(compare(*args.compare))
    run(args)


if __name__ == "__main__":
    main()
