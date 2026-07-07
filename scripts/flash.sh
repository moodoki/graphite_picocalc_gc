#!/usr/bin/env bash
# Flash a .uf2 file to a Pico in BOOTSEL mode.
#
# On macOS, the Pico mounts as /Volumes/RPI-RP2 (Pico 1) or
# /Volumes/RP2350 (Pico 2). On Linux, mount path varies.
#
# Usage:
#   ./scripts/flash.sh                              # defaults to build/pico/picocalc_graphcalc.uf2
#   ./scripts/flash.sh build/pico2/picocalc_graphcalc.uf2

set -euo pipefail

cd "$(dirname "$0")/.."

UF2="${1:-build/pico/picocalc_graphcalc.uf2}"

if [[ ! -f "$UF2" ]]; then
  echo "ERROR: $UF2 not found." >&2
  echo "Build first: ./scripts/build-all.sh" >&2
  exit 1
fi

# Detect mount point (macOS)
if [[ "$(uname)" == "Darwin" ]]; then
  for vol in /Volumes/RPI-RP2 /Volumes/RP2350; do
    if [[ -d "$vol" ]]; then
      MOUNT="$vol"
      break
    fi
  done

  if [[ -z "${MOUNT:-}" ]]; then
    echo "ERROR: No Pico in BOOTSEL mode detected." >&2
    echo "  1. Hold BOOTSEL on the PicoCalc / Pico" >&2
    echo "  2. Connect USB-C" >&2
    echo "  3. Wait for /Volumes/RPI-RP2 or /Volumes/RP2350 to mount" >&2
    exit 1
  fi

  echo "Flashing $UF2 → $MOUNT/"
  cp "$UF2" "$MOUNT/"
  echo "Flash complete. The Pico will reboot."
else
  echo "ERROR: This script currently supports macOS only." >&2
  echo "On Linux: find your mount point and: cp $UF2 /path/to/RPI-RP2/" >&2
  exit 1
fi
