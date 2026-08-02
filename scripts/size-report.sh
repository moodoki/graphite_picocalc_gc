#!/usr/bin/env bash
# Static-footprint report for a built firmware ELF: total text/data/bss plus
# the largest bss/data symbols (demangled). Turns "headroom is shrinking"
# (size-optimization-ideas.md) into a repeatable, diffable metric.
#
# Usage:
#   ./scripts/size-report.sh [build/pico|build/pico2] [top-N]
#   ./scripts/size-report.sh                 # default: build/pico, top 25
#
# Compare two builds by saving the output and diffing, e.g.:
#   ./scripts/size-report.sh build/pico > /tmp/before.txt
#   ...make a change, rebuild...
#   ./scripts/size-report.sh build/pico > /tmp/after.txt && diff /tmp/before.txt /tmp/after.txt

set -euo pipefail

cd "$(dirname "$0")/.."

BUILD="${1:-build/pico}"
TOPN="${2:-25}"
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
