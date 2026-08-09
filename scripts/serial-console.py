#!/usr/bin/env python3
"""Interactive/scripted serial console for the PicoCalc (Phase 5.1).

Submits expression lines to the home screen over USB serial and reads back
the firmware's `inject:` echo, plus any `stack:`/`fault:` diagnostics that
land alongside it. Firmware side is the PICOCALC_SERIAL_INJECT block in
src/main.cpp; see docs/phases/phase5.1-spec.md.

Usage:
  serial-console.py 'sqrt(8)' 'det([[1,2][3,4]])'   # submit each, print results
  serial-console.py -f exprs.txt                    # one expression per line
  serial-console.py -                               # read expressions from stdin
  serial-console.py --watch 300                     # just tail diagnostics

Exit status is 1 if any submission errored or timed out, so a script can
gate on it.

Two things this does that scripts/serial-capture.py does not, both learned
the hard way:

  - **Asserts DTR/RTS.** pico stdio_usb only transmits when a terminal is
    connected. serial-capture.py:9-13 documents this for reads; a
    non-interactive *write* needs it just as much.
  - **Reconnects when the device drops.** serial-capture.py spins on a dead
    fd after a reboot and silently reads nothing — that lost a whole test
    pass on 2026-08-08. Reflashing mid-session (picotool load -f -x) is
    normal here, so reconnecting is not optional.
"""

import argparse
import fcntl
import glob
import re
import os
import select
import struct
import sys
import termios
import time

TIOCMBIS = 0x8004746C  # set modem-control bits
TIOCM_DTR = 0x002
TIOCM_RTS = 0x004

# Firmware caps a line at ui::InputLine::kCapacity and rejects anything
# longer rather than truncating. Mirror it here so the failure is reported
# by the sender, with the offending text, instead of arriving as a bare
# "line too long" from the board.
MAX_LINE = 128

# The firmware renders symbols as single high bytes indexing its own font,
# not as UTF-8. Map them back so results are legible and, more importantly,
# distinguishable — they are decoded latin-1, so the raw byte always survives
# underneath. Mirrors the slot map in src/gfx/font.hpp:40-54; keep in sync if
# that map changes (gen-fonts.sh regenerates it).
GLYPHS = {
    "\x7f": "π",
    "\x80": "∠",  # polar phasor angle sign
    "\x81": "θ",
    "\x82": "σ",
    "\x83": "Σ",
    "\x84": "χ",
    "\x85": "µ",
    "\x86": "i",  # slanted imaginary unit
    "\x87": "→",  # store arrow
    "\x88": "λ",
    "\x89": "≠",
    "\x8a": "…",  # horizontal ellipsis (truncation marker)
    "\x8b": "²",
    "\x8c": "√",
    "\x8d": "ₓ",
}


def render(text):
    """Firmware glyph bytes -> printable text. Unknown highs become \\xNN."""
    if text is None:
        return None
    out = []
    for ch in text:
        if ch in GLYPHS:
            out.append(GLYPHS[ch])
        elif ord(ch) < 0x20 or 0x7F <= ord(ch) < 0x100:
            out.append(f"\\x{ord(ch):02x}")
        else:
            out.append(ch)
    return "".join(out)


class Console:
    def __init__(self, verbose=False):
        self.fd = None
        self.verbose = verbose
        self._pending = ""
        # Firmware-reported timings from the last submit(), or None on a build
        # that does not report them. `last_us` is the whole submit_line
        # (evaluation + formatting + SD history write); `last_eval_us` is the
        # §9 probe window, evaluation + formatting only. See submit().
        self.last_us = None
        self.last_eval_us = None

    # -- connection ----------------------------------------------------
    def _open(self):
        devs = sorted(glob.glob("/dev/cu.usbmodem*"))
        if not devs:
            return False
        try:
            fd = os.open(devs[0], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        except OSError:
            return False
        attrs = termios.tcgetattr(fd)
        attrs[0] = attrs[1] = attrs[3] = 0  # raw
        attrs[2] = termios.CREAD | termios.CLOCAL | termios.CS8
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        for bit in (TIOCM_DTR, TIOCM_RTS):
            fcntl.ioctl(fd, TIOCMBIS, struct.pack("I", bit))
        self.fd = fd
        self._pending = ""
        if self.verbose:
            print(f"# connected {devs[0]}", file=sys.stderr)
        return True

    def connect(self, timeout=15.0):
        end = time.time() + timeout
        while time.time() < end:
            if self._open():
                return True
            time.sleep(0.5)
        return False

    def _drop(self):
        if self.fd is not None:
            try:
                os.close(self.fd)
            except OSError:
                pass
        self.fd = None

    # -- io ------------------------------------------------------------
    def readline(self, timeout):
        """Next complete line, or None on timeout. Reconnects across reboots."""
        end = time.time() + timeout
        while time.time() < end:
            if "\n" in self._pending:
                line, self._pending = self._pending.split("\n", 1)
                return line.rstrip("\r")
            if self.fd is None:
                if not self.connect(timeout=max(0.0, end - time.time())):
                    return None
                continue
            try:
                readable, _, _ = select.select([self.fd], [], [], 0.2)
            except OSError:
                self._drop()
                continue
            if not readable:
                if not glob.glob("/dev/cu.usbmodem*"):
                    self._drop()  # Device went away; wait for it to come back
                continue
            try:
                data = os.read(self.fd, 4096)
            except BlockingIOError:
                continue
            except OSError:
                self._drop()
                continue
            if data:
                # latin-1, not utf-8: the firmware emits custom font glyphs as
                # single high bytes (math/format.hpp), which are not valid
                # UTF-8. errors="replace" would collapse them all to U+FFFD,
                # making "2i" and "2<angle>" compare equal — fatal for a
                # harness that exists to compare results. latin-1 is bijective
                # over bytes, so nothing is lost; GLYPHS renders them below.
                self._pending += data.decode("latin-1")
        return None

    def send(self, line):
        if self.fd is None and not self.connect():
            raise RuntimeError("no device")
        fd = self.fd
        if fd is None:
            raise RuntimeError("no device")
        payload = (line + "\n").encode()
        while payload:
            try:
                n = os.write(fd, payload)
            except (BlockingIOError, InterruptedError):
                time.sleep(0.01)
                continue
            except OSError as exc:
                raise RuntimeError(f"write failed: {exc}") from exc
            payload = payload[n:]

    # -- the primitive the bench actually needs ------------------------
    def submit(self, expr, timeout=10.0):
        """Send `expr`, return (ok, text, kind, diagnostics).

        Firmware evaluation time, when the build reports one, lands in
        `self.last_us` rather than the tuple — builds predating Phase 5.2's
        `us=` field (every release up to and including v0.3.2) leave it None,
        and the A/B pass has to read both. See scripts/ab-measure.py.
        """
        self.last_us = None  # Per-submit, so a stale value can never be read
        self.last_eval_us = None
        if len(expr) >= MAX_LINE:
            return False, f"line too long ({len(expr)} >= {MAX_LINE})", None, []
        self.send(expr)
        diags = []
        end = time.time() + timeout
        while time.time() < end:
            line = self.readline(timeout=max(0.1, end - time.time()))
            if line is None:
                break
            if line.startswith(("stack:", "fault:")):
                diags.append(line)
                continue
            if not line.startswith("inject:"):
                continue  # Unrelated heartbeat (psram-bulk, temp, battery)
            body = line[len("inject:") :].strip()
            if body == "popped to home":
                continue  # Informational, the result echo still follows
            if body.startswith("error "):
                return False, body[len("error ") :], None, diags
            if body.endswith("-> command"):
                return True, None, "command", diags
            # inject: "<expr>" -> "<result>" kind=<kind> [us=<n>] [eval_us=<n>]
            #
            # The trailing fields are optional and were appended over time
            # (5.2.12), so one parser reads them and the released baselines
            # that have none. Peel them off the END one token at a time rather
            # than splitting on a substring: `" us="` also occurs inside
            # `" eval_us="` far too easily, which is exactly how the first cut
            # of this silently returned None for both.
            kind = None
            while True:
                m = re.search(r"\s(kind|us|eval_us)=(\S+)$", body)
                if not m:
                    break
                key, val = m.group(1), m.group(2)
                body = body[: m.start()]
                if key == "kind":
                    kind = val
                elif key == "us":
                    self.last_us = int(val) if val.isdigit() else None
                else:
                    self.last_eval_us = int(val) if val.isdigit() else None
            text = None
            if "-> " in body:
                text = body.split("-> ", 1)[1].strip().strip('"')
            return True, text, kind, diags
        return False, "timeout", None, diags


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exprs", nargs="*", help="expressions to submit ('-' reads stdin)")
    ap.add_argument("-f", "--file", help="file with one expression per line")
    ap.add_argument("-t", "--timeout", type=float, default=10.0,
                    help="per-expression timeout in seconds (default 10)")
    ap.add_argument("--watch", type=float, metavar="SECONDS",
                    help="submit nothing; tail stack:/fault: lines for SECONDS")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="echo connection and diagnostic lines to stderr")
    args = ap.parse_args()

    con = Console(verbose=args.verbose)
    if not con.connect():
        sys.exit("no /dev/cu.usbmodem* device (firmware running? BOOTSEL?)")

    if args.watch is not None:
        end = time.time() + args.watch
        while time.time() < end:
            line = con.readline(timeout=max(0.1, end - time.time()))
            if line and line.startswith(("stack:", "fault:", "inject:")):
                print(line, flush=True)
        return 0

    exprs = []
    if args.file:
        with open(args.file, encoding="utf-8") as fh:
            exprs += [ln.strip() for ln in fh if ln.strip() and not ln.startswith("#")]
    for e in args.exprs:
        if e == "-":
            exprs += [ln.strip() for ln in sys.stdin if ln.strip()]
        else:
            exprs.append(e)
    if not exprs:
        ap.error("nothing to submit (give expressions, -f FILE, or --watch)")

    failures = 0
    for expr in exprs:
        ok, text, kind, diags = con.submit(expr, timeout=args.timeout)
        for d in diags:
            print(f"    {d}", file=sys.stderr if not args.verbose else sys.stdout)
        if not ok:
            failures += 1
            print(f"{expr!r:40} ERROR {text}", flush=True)
        elif kind == "command":
            print(f"{expr!r:40} (command)", flush=True)
        else:
            print(f"{expr!r:40} = {render(text)}   [{kind}]", flush=True)

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
