#!/usr/bin/env bash
# Open a USB serial console to a connected Pico/PicoCalc.
#
# Auto-detects the device (waits up to 10 seconds).
# Exit screen with: Ctrl-A then K, then Y.

set -euo pipefail

if [[ "$(uname)" != "Darwin" ]]; then
  echo "ERROR: This script currently supports macOS only." >&2
  echo "On Linux: ls /dev/ttyACM* and use minicom or screen." >&2
  exit 1
fi

BAUD=115200

echo "Waiting for USB serial device..."
for i in {1..10}; do
  DEV=$(ls /dev/tty.usbmodem* 2>/dev/null | head -n1 || true)
  if [[ -n "$DEV" ]]; then
    break
  fi
  sleep 1
done

if [[ -z "${DEV:-}" ]]; then
  echo "ERROR: No USB serial device found after 10 seconds." >&2
  echo "  - Is the PicoCalc connected via USB-C?" >&2
  echo "  - Is the firmware actually running (not in BOOTSEL mode)?" >&2
  exit 1
fi

echo "Connecting to $DEV at $BAUD baud."
echo "Exit with: Ctrl-A K Y"
echo
exec screen "$DEV" "$BAUD"
