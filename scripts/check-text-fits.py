#!/usr/bin/env python3
"""Static check: no literal string is drawn past the right edge of the panel.

Why this exists: the Y= editor's hint row was 40 characters, and the 8x16
font puts the 40th glyph at x=322 on a 320-wide panel, so the last hint
ran off the screen (testdrive 2026-08-22). Nothing catches
that at build time — the string compiles, the draw clips, and it is only
visible to someone looking at the panel. One character of drift in a hint
row is a whole class of bug, and this is the cheapest place to close it.

WHAT IS AND IS NOT CHECKED. A call is checked when BOTH its x and its
text are literals, which is what hint and label rows are. Calls that draw
a runtime buffer (formatted numbers, filenames, user expressions) cannot
be measured from source and are skipped, and the summary line says how
many — a shrinking coverage number is itself worth noticing.

Usage: check-text-fits.py [--verbose]   (exits 1 on any overflow)
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def panel_width():
    """Read kScreenW rather than hardcode it, so a panel change is picked up."""
    src = open(os.path.join(ROOT, "src/platform/display.hpp")).read()
    m = re.search(r"constexpr int kScreenW = (\d+);", src)
    if not m:
        sys.exit("could not find kScreenW in src/platform/display.hpp")
    return int(m.group(1))


def font_widths():
    """Glyph width of each gfx font, read from the generated header itself."""
    widths = {}
    # The main font is selectable at build time (PICOCALC_FONT) but every
    # candidate is 8x16, so the width is a property of the family, not the
    # choice. Assert that rather than trusting it.
    for header in glob.glob(os.path.join(ROOT, "src/gfx/fonts/*8x16.h")):
        vals = re.findall(r"0x([0-9A-Fa-f]{2})", open(header).read())
        if vals and int(vals[0], 16) != 8:
            sys.exit("%s is not 8 wide; this check assumes the 8x16 family" % header)
    widths["main_font"] = 8
    small = os.path.join(ROOT, "src/gfx/fonts/spleen5x8.h")
    if os.path.exists(small):
        vals = re.findall(r"0x([0-9A-Fa-f]{2})", open(small).read())
        widths["small_font"] = int(vals[0], 16) if vals else 5
    return widths


ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "\\": "\\", '"': '"', "'": "'"}


def decode(literal_body):
    """Escape sequences are one glyph, not two characters. \\x7f is pi."""
    out = []
    i = 0
    while i < len(literal_body):
        c = literal_body[i]
        if c != "\\":
            out.append(c)
            i += 1
            continue
        nxt = literal_body[i + 1]
        if nxt == "x":
            out.append(chr(int(literal_body[i + 2:i + 4], 16)))
            i += 4
        elif nxt in ESCAPES:
            out.append(ESCAPES[nxt])
            i += 2
        else:
            out.append(nxt)
            i += 2
    return "".join(out)


def split_args(text):
    """Split a call's argument list on top-level commas."""
    args, depth, cur, in_str, esc = [], 0, [], False, False
    for c in text:
        if in_str:
            cur.append(c)
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                in_str = False
            continue
        if c == '"':
            in_str = True
            cur.append(c)
        elif c in "([{":
            depth += 1
            cur.append(c)
        elif c in ")]}":
            depth -= 1
            cur.append(c)
        elif c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
    args.append("".join(cur).strip())
    return args


def literal_text(arg):
    """Decoded text of a string literal (or adjacent concatenation), else None."""
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', arg)
    if not parts or re.sub(r'"(?:[^"\\]|\\.)*"', "", arg).strip():
        return None  # not a pure literal (a variable, a call, a ternary)
    return "".join(decode(p) for p in parts)


def literal_int(arg):
    """Value of an integer-literal x, allowing simple `A + B` sums."""
    if re.fullmatch(r"-?\d+", arg):
        return int(arg)
    if re.fullmatch(r"-?\d+(\s*[+-]\s*\d+)+", arg):
        return eval(arg)  # digits and +/- only, checked above
    return None


def calls_in(path):
    """Yield (line_no, font_name, args) for every draw_string call."""
    src = open(path).read()
    lines_at = [0]
    for ch in src:
        lines_at.append(lines_at[-1] + (1 if ch == "\n" else 0))

    # Which font a local `font` refers to, by source offset.
    bindings = [(m.start(), m.group(1))
                for m in re.finditer(r"=\s*gfx::(\w+_font)\(\)", src)]

    for m in re.finditer(r"(\w+)\.draw_string\(", src):
        recv = m.group(1)
        i = m.end()
        depth, in_str, esc, start = 1, False, False, i
        while i < len(src) and depth:
            c = src[i]
            if in_str:
                if esc:
                    esc = False
                elif c == "\\":
                    esc = True
                elif c == '"':
                    in_str = False
            elif c == '"':
                in_str = True
            elif c in "([{":
                depth += 1
            elif c in ")]}":
                depth -= 1
            i += 1
        font = "main_font"
        if recv == "font":
            prior = [n for off, n in bindings if off < m.start()]
            if prior:
                font = prior[-1]
        elif recv.endswith("small_font"):
            font = "small_font"
        yield lines_at[m.start()] + 1, font, split_args(src[start:i - 1])


def main():
    verbose = "--verbose" in sys.argv
    width = panel_width()
    widths = font_widths()
    checked = skipped = 0
    failures = []
    rows = []

    files = sorted(glob.glob(os.path.join(ROOT, "src/**/*.cpp"), recursive=True) +
                   glob.glob(os.path.join(ROOT, "src/**/*.hpp"), recursive=True))
    for path in files:
        rel = os.path.relpath(path, ROOT)

        for line_no, font, args in calls_in(path):
            if len(args) < 4:
                skipped += 1
                continue
            x = literal_int(args[1])
            text = literal_text(args[3])
            if x is None or text is None:
                skipped += 1
                continue
            checked += 1
            end = x + len(text) * widths[font]
            rows.append((end, rel, line_no, x, text))
            if end > width:
                failures.append((rel, line_no, x, text, end))

        # softkey_text() overrides are hint rows too: slot_editor.cpp draws
        # whatever they return at x=2 in the main font.
        src = open(path).read()
        for fn in re.finditer(r"softkey_text\(\)\s*const[^{;]*\{(.*?)\n?\}", src, re.S):
            m = re.search(r"return\s+(\"(?:[^\"\\]|\\.)*\")\s*;", fn.group(1))
            if m is None:
                continue  # returns something computed; not measurable here
            text = literal_text(m.group(1))
            if text is None:
                continue
            checked += 1
            line_no = src.count("\n", 0, fn.start(1) + m.start()) + 1
            end = 2 + len(text) * 8
            rows.append((end, rel, line_no, 2, text))
            if end > width:
                failures.append((rel, line_no, 2, text, end))

    for f in failures:
        rel, line_no, x, text, end = f
        print("%s:%d: text runs off the %dpx panel (x=%d, %d chars, ends at %d)"
              % (rel, line_no, width, x, len(text), end))
        print('    "%s"' % text)

    if verbose:
        print("\nWidest rows that fit:")
        for end, rel, line_no, x, text in sorted(rows, reverse=True)[:8]:
            print("  ends@%-4d %-34s %s:%d" % (end, '"%s"' % text[:30], rel, line_no))

    print("\ntext-fits: %d literal draws checked, %d skipped (runtime text), %d overflowing"
          % (checked, skipped, len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
