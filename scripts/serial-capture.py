#!/usr/bin/env python3
"""Non-interactive USB-serial capture for the PicoCalc (macOS).

Usage: serial-capture.py [SECONDS] [MATCH]

Reads /dev/cu.usbmodem* for SECONDS (default 30) and prints everything
received; exits early once MATCH (a substring) has been seen, if given.

Why not `cat`: pico stdio_usb only transmits when a terminal is
connected, i.e. DTR is asserted — `screen` (scripts/monitor.sh) does
that, plain `cat` does not, and silently reads nothing (learned the
hard way, 2026-07-18). This script opens the port raw and asserts
DTR/RTS explicitly, so it works from non-interactive agent sessions.
"""

import fcntl
import glob
import os
import select
import struct
import sys
import termios
import time

TIOCMBIS = 0x8004746C  # set modem-control bits
TIOCM_DTR = 0x002
TIOCM_RTS = 0x004


def main():
    seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
    match = sys.argv[2].encode() if len(sys.argv) > 2 else None

    devs = glob.glob("/dev/cu.usbmodem*")
    if not devs:
        sys.exit("no /dev/cu.usbmodem* device (firmware running? BOOTSEL?)")
    fd = os.open(devs[0], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = attrs[1] = attrs[3] = 0  # raw: no iflag/oflag/lflag
    attrs[2] = termios.CREAD | termios.CLOCAL | termios.CS8
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    for bit in (TIOCM_DTR, TIOCM_RTS):
        fcntl.ioctl(fd, TIOCMBIS, struct.pack("I", bit))

    end = time.time() + seconds
    buf = b""
    while time.time() < end:
        readable, _, _ = select.select([fd], [], [], 0.5)
        if readable:
            try:
                data = os.read(fd, 4096)
            except (BlockingIOError, OSError):
                continue
            if data:
                buf += data
                sys.stdout.write(data.decode(errors="replace"))
                sys.stdout.flush()
        if match and match in buf:
            time.sleep(0.2)  # drain the tail of the line
            try:
                tail = os.read(fd, 4096)
                if tail:
                    sys.stdout.write(tail.decode(errors="replace"))
            except (BlockingIOError, OSError):
                pass
            break
    os.close(fd)


if __name__ == "__main__":
    main()
