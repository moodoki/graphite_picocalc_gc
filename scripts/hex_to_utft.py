#!/usr/bin/env python3
"""Convert a GNU Unifont .hex to a UTFT-format C header (testdrive
2026-07-20 font test-drive). Unifont is already a native 8x16 (and 16x16)
bitmap font, so its 8-wide glyphs map byte-for-byte onto the layout
gfx::Font reads — no rasterization, pixel-exact.

  hex_to_utft.py FONT.hex ARRAY_NAME [--first 32] [--last 134] \
      [--map SLOT:CODEPOINT ...] [--extra FILE ...]

.hex lines are "CODEPOINT:BITMAP"; an 8-wide glyph is 16 bytes (32 hex
digits, one byte per row), a 16-wide glyph 32 bytes. Only 8-wide glyphs
fit the cell; --map a wider glyph and it is rejected. --map places a
glyph from another codepoint into a slot (Greek/math into the high slots
the ASCII range leaves free); --extra bakes an 8-wide hand-drawn glyph
(same format as bdf_to_utft.py --extra) for symbols Unifont only has at
16 wide, e.g. the slanted imaginary-unit i.
"""

import argparse
import sys

CELL_W = 8
CELL_H = 16


def parse_hex(path):
    """Return {codepoint: [16 row-bytes]} for 8-wide glyphs only."""
    glyphs = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or ":" not in line:
                continue
            cp_s, bits = line.split(":", 1)
            if len(bits) != CELL_H * 2:  # 8-wide == 16 bytes == 32 hex digits
                continue
            rows = [int(bits[i:i + 2], 16) for i in range(0, len(bits), 2)]
            glyphs[int(cp_s, 16)] = rows
    return glyphs


def parse_extra(path):
    """Hand-drawn glyphs: {slot: (name, [16 row-bytes])}. '#'/'X'/'*'/'@'
    mean pixel-on; 8 columns, 16 rows per glyph (same as bdf_to_utft)."""
    on = set("#X*@")
    hand = {}
    slot = None
    name = ""
    rows = []
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\n")
            stripped = line.strip()
            if slot is None:
                if not stripped or stripped.startswith("#"):
                    continue
                if not stripped.startswith("@"):
                    sys.exit(f"error: extra {path}: expected '@ SLOT name', got {line!r}")
                parts = stripped.split(maxsplit=2)
                slot = int(parts[1])
                name = parts[2] if len(parts) > 2 else f"0x{slot:02X}"
                rows = []
            else:
                byte = 0
                for c in range(CELL_W):
                    ch = line[c] if c < len(line) else "."
                    byte = (byte << 1) | (1 if ch in on else 0)
                rows.append(byte)
                if len(rows) == CELL_H:
                    hand[slot] = (name, rows)
                    slot = None
    if slot is not None:
        sys.exit(f"error: extra {path}: glyph {slot} has fewer than {CELL_H} rows")
    return hand


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("hex")
    ap.add_argument("array_name")
    ap.add_argument("--first", type=int, default=32)
    ap.add_argument("--last", type=int, default=134)
    ap.add_argument("--map", action="append", default=[], metavar="SLOT:CODEPOINT")
    ap.add_argument("--extra", action="append", default=[], metavar="FILE")
    ap.add_argument("--shift", type=int, default=0, metavar="N",
                    help="translate every glyph up N pixels (Unifont's baseline "
                         "sits ~2px lower than Spleen/Terminus; N>0 lifts to match)")
    args = ap.parse_args()

    remap = {}
    for spec in args.map:
        slot, cp = (int(x, 0) for x in spec.split(":"))
        if not args.first <= slot <= args.last:
            sys.exit(f"error: --map slot {slot} outside {args.first}..{args.last}")
        remap[slot] = cp

    hand = {}
    hand_name = {}
    for path in args.extra:
        for slot, (name, rows) in parse_extra(path).items():
            if not args.first <= slot <= args.last:
                sys.exit(f"error: --extra slot {slot} outside {args.first}..{args.last}")
            hand[slot] = rows
            hand_name[slot] = name

    glyphs = parse_hex(args.hex)
    count = args.last - args.first + 1
    out = [CELL_W, CELL_H, args.first, count]
    clipped = []
    for slot in range(args.first, args.last + 1):
        if slot in hand:
            rows = hand[slot]
        else:
            cp = remap.get(slot, slot)
            if cp in remap.values() and cp not in glyphs:
                sys.exit(f"error: --map codepoint U+{cp:04X} not an 8-wide glyph in {args.hex}")
            rows = glyphs.get(cp, [0] * CELL_H)
        if args.shift > 0:
            n = args.shift
            if any(rows[:n]):
                clipped.append(slot)
            rows = rows[n:] + [0] * n
        elif args.shift < 0:
            n = -args.shift
            rows = [0] * n + rows[:CELL_H - n]
        out.extend(rows)
    if clipped:
        sys.stderr.write(
            f"note: --shift {args.shift} clipped top pixels of slot(s) "
            f"{', '.join(str(s) for s in clipped)}\n")

    per_glyph = (CELL_W * CELL_H) // 8
    print(f"// Generated by scripts/hex_to_utft.py from {args.hex.split('/')[-1]}")
    print(f"// {CELL_W}x{CELL_H}, chars {args.first}..{args.last}, "
          f"{len(out)} bytes. Do not edit by hand.")
    print(f"\nconst unsigned char {args.array_name}[] = {{")
    print(f"    0x{out[0]:02X}, 0x{out[1]:02X}, 0x{out[2]:02X}, 0x{out[3]:02X},")
    for g in range(count):
        base = 4 + g * per_glyph
        row = ", ".join(f"0x{b:02X}" for b in out[base:base + per_glyph])
        slot = args.first + g
        if slot in hand_name:
            label = hand_name[slot]
        elif slot in remap:
            label = f"U+{remap[slot]:04X}"
        elif chr(slot) == "\\":
            label = "backslash"
        elif chr(slot).isprintable():
            label = chr(slot)
        else:
            label = f"0x{slot:02X}"
        print(f"    {row},  // {slot} '{label}'")
    print("};")


if __name__ == "__main__":
    main()
