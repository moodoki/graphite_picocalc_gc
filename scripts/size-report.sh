#!/usr/bin/env bash
# Static-footprint report for a built firmware ELF: total text/data/bss plus
# the largest bss/data symbols (demangled). Turns "headroom is shrinking"
# (size-optimization-ideas.md) into a repeatable, diffable metric.
#
# Also lists the largest stack frames — core 0 has only 4 KB before it runs
# into core 1's stack, so an oversized frame is a hang waiting to happen
# (D47).
#
# Usage:
#   ./scripts/size-report.sh [build/pico|build/pico2] [top-N] [min-frame-bytes]
#   ./scripts/size-report.sh                 # default: build/pico, top 25, 512
#
# Compare two builds by saving the output and diffing, e.g.:
#   ./scripts/size-report.sh build/pico > /tmp/before.txt
#   ...make a change, rebuild...
#   ./scripts/size-report.sh build/pico > /tmp/after.txt && diff /tmp/before.txt /tmp/after.txt

set -euo pipefail

cd "$(dirname "$0")/.."

BUILD="${1:-build/pico}"
TOPN="${2:-25}"
FRAMEMIN="${3:-512}"
ELF="$BUILD/picocalc_graphcalc.elf"

if [[ ! -f "$ELF" ]]; then
    echo "error: $ELF not found — build it first (scripts/build-all.sh)" >&2
    exit 1
fi

# Prefer the cross toolchain's binutils; fall back to a PATH copy.
TC="${PICO_TOOLCHAIN_PATH:-/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi}"
pick() {
    for c in "$TC/bin/arm-none-eabi-$1" "$(command -v "arm-none-eabi-$1" || true)"; do
        [[ -x "$c" ]] && { echo "$c"; return; }
    done
    echo "error: arm-none-eabi-$1 not found" >&2
    exit 1
}
SIZE="$(pick size)"
NM="$(pick nm)"
FILT="$(pick c++filt)"
OBJDUMP="$(pick objdump)"

echo "== Static footprint: $ELF =="
"$SIZE" "$ELF"
echo

# RP2040 has 264 KB SRAM; RP2350 has 520 KB. bss+data must fit alongside the
# stacks and any runtime heap. Report headroom against the board's SRAM.
BSS="$("$SIZE" "$ELF" | awk 'NR==2{print $3}')"
DATA="$("$SIZE" "$ELF" | awk 'NR==2{print $2}')"
case "$BUILD" in
    *pico2*) SRAM=$((520 * 1024)); BOARD="RP2350 (520 KB SRAM)" ;;
    *)       SRAM=$((264 * 1024)); BOARD="RP2040 (264 KB SRAM)" ;;
esac
USED=$((BSS + DATA))
echo "board: $BOARD"
printf "static RAM (bss+data): %d bytes (%.1f KB) — ~%d KB nominal headroom\n" \
    "$USED" "$(echo "scale=1; $USED/1024" | bc)" "$(((SRAM - USED) / 1024))"
echo

echo "== Top $TOPN bss symbols (bytes, demangled) =="
"$NM" --print-size --size-sort --radix=d "$ELF" \
    | awk '$3 ~ /^[bB]$/ {print $2, $4}' | sort -rn | head -n "$TOPN" \
    | while read -r sz sym; do printf "%8s  %s\n" "$((10#$sz))" "$("$FILT" "$sym")"; done
echo

echo "== Top $TOPN data symbols (bytes, demangled) =="
"$NM" --print-size --size-sort --radix=d "$ELF" \
    | awk '$3 ~ /^[dD]$/ {print $2, $4}' | sort -rn | head -n "$TOPN" \
    | while read -r sz sym; do printf "%8s  %s\n" "$((10#$sz))" "$("$FILT" "$sym")"; done
echo

# Stack, not static RAM — but the same kind of budget, and until D47 it was
# the one nobody was watching. Core 0 gets 4 KB total before it is writing
# into core 1's stack, and a *single* function was spending 2,232 B of that
# (Engine::compile's tinyexpr binding table). Nothing catches that at build
# time on its own: the MPU stack guard is a 32-byte tripwire a large frame
# can step straight over, and host tests run with an 8 MB stack.
#
# Frames are read out of the prologue: pushed registers, plus whichever
# form the compiler used to move sp (Thumb-1 has no wide immediate, so GCC
# loads a negative literal and adds it). Callee frames stack on top of
# these, so treat the numbers as a floor, not a total.
echo "== Stack frames >= $FRAMEMIN bytes (prologue, demangled) =="
"$OBJDUMP" -d --demangle "$ELF" | python3 -c '
import re, sys

threshold = int(sys.argv[1])
frames = []
name, lines = None, []

def emit(name, lines):
    if name is None:
        return
    words, regs, total = {}, {}, 0
    for l in lines:
        m = re.match(r"\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+\.word\s+0x([0-9a-f]+)", l)
        if m:
            words[int(m.group(1), 16)] = int(m.group(2), 16)
    for l in lines[:40]:
        m = re.search(r"push\s+\{(.+)\}", l)
        if m and total == 0:
            n = 0
            for r in m.group(1).split(", "):
                if "-" in r:
                    a, b = r.split("-")
                    n += int(b[1:]) - int(a[1:]) + 1
                else:
                    n += 1
            total += n * 4
        # Literal-pool load feeding an "add sp, rN" (Thumb-1 large frame).
        m = re.search(r"ldr\s+(r\d+), \[pc, #\d+\]\s+@ \(([0-9a-f]+) ", l)
        if m:
            regs[m.group(1)] = words.get(int(m.group(2), 16))
        m = re.search(r"sub(?:\.w|w)?\s+sp, (?:sp, )?#(\d+)", l)
        if m:
            total += int(m.group(1))
        m = re.search(r"add\s+sp, (r\d+)", l)
        if m and regs.get(m.group(1)) is not None:
            v = regs[m.group(1)]
            if v > 0x80000000:          # negative: sp moves down
                total += (1 << 32) - v
    if total >= threshold:
        frames.append((total, name))

for line in sys.stdin:
    line = line.rstrip("\n")
    m = re.match(r"^[0-9a-f]{8} <(.+)>:$", line)
    if m:
        emit(name, lines)
        name, lines = m.group(1), []
    elif name is not None:
        lines.append(line)
emit(name, lines)

for total, name in sorted(frames, reverse=True):
    print("%8d  %s" % (total, name))
if not frames:
    print("  (none)")
' "$FRAMEMIN"
