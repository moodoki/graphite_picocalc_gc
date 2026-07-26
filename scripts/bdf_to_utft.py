#!/usr/bin/env python3
"""Convert a monospace BDF font to a UTFT-format C header (D9 tooling).

UTFT layout (what gfx::Font reads): data[0]=width, data[1]=height,
data[2]=first char, data[3]=char count, then (w*h/8) bytes per glyph as a
continuous row-major MSB-first bitstream.

Usage:
  bdf_to_utft.py FONT.bdf ARRAY_NAME [--first 32] [--last 126] \
      [--map DEST:CODEPOINT ...] > header.h

--map bakes a non-ASCII glyph into an otherwise-unused slot, e.g.
--last 127 --map 127:960 puts Greek pi (U+03C0) at byte 0x7F (DEL).

--donor FILE names a second BDF consulted for --map codepoints the
primary BDF lacks (e.g. Greek for Spleen 5x8, sourced from the public
domain X11 fixed 5x8). It must share the primary's cell size.

--extra FILE bakes hand-drawn glyphs (for symbols the BDF lacks, e.g.
the angle sign or a slanted imaginary-unit i). The file holds one or
more blocks:

    @ SLOT name
    <cell_h rows of cell_w chars; '#'/'X'/'*'/'@' = on, else off>

Blank lines and lines starting with '#' (outside a glyph body) are
ignored. Hand-drawn glyphs win over --map for the same slot.
"""

import argparse
import sys


def parse_bdf(path):
    """Return (cell_w, cell_h, ascent, {codepoint: glyph}) where glyph is
    (bbx_w, bbx_h, bbx_xoff, bbx_yoff, [row_bitmasks])."""
    cell_w = cell_h = ascent = None
    glyphs = {}
    with open(path, "r", encoding="utf-8") as f:
        lines = iter(f)
        encoding = None
        bbx = None
        for line in lines:
            tok = line.split()
            if not tok:
                continue
            if tok[0] == "FONTBOUNDINGBOX":
                cell_w, cell_h = int(tok[1]), int(tok[2])
            elif tok[0] == "FONT_ASCENT":
                ascent = int(tok[1])
            elif tok[0] == "ENCODING":
                encoding = int(tok[1])
            elif tok[0] == "BBX":
                bbx = (int(tok[1]), int(tok[2]), int(tok[3]), int(tok[4]))
            elif tok[0] == "BITMAP":
                rows = []
                for row in lines:
                    row = row.strip()
                    if row == "ENDCHAR":
                        break
                    rows.append(int(row, 16) if row else 0)
                if encoding is not None and encoding >= 0 and bbx is not None:
                    glyphs[encoding] = (*bbx, rows)
                encoding = None
                bbx = None
    if cell_w is None or cell_h is None or ascent is None:
        sys.exit("error: BDF missing FONTBOUNDINGBOX/FONT_ASCENT")
    return cell_w, cell_h, ascent, glyphs


def render_cell(cell_w, cell_h, ascent, glyph):
    """Rasterize one glyph into a cell_w x cell_h boolean grid."""
    bw, bh, xoff, yoff, rows = glyph
    grid = [[False] * cell_w for _ in range(cell_h)]
    top = ascent - (bh + yoff)  # y of the glyph's first bitmap row in the cell
    hex_bits = ((bw + 7) // 8) * 8  # BDF rows are padded to whole bytes
    for ry, mask in enumerate(rows):
        y = top + ry
        if not 0 <= y < cell_h:
            continue
        for rx in range(bw):
            if (mask >> (hex_bits - 1 - rx)) & 1:
                x = xoff + rx
                if 0 <= x < cell_w:
                    grid[y][x] = True
    return grid


def parse_extra(path, cell_w, cell_h):
    """Parse a hand-drawn glyph file into {slot: grid} (grid is a
    cell_h x cell_w boolean matrix). '#','X','*','@' mean pixel-on."""
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
                rows.append([ch in on for ch in line[:cell_w].ljust(cell_w)])
                if len(rows) == cell_h:
                    hand[slot] = (name, rows)
                    slot = None
    if slot is not None:
        sys.exit(f"error: extra {path}: glyph {slot} has fewer than {cell_h} rows")
    return hand


def parse_hex(path, cell_h):
    """Parse a GNU Unifont .hex into {codepoint: grid} for 8-wide glyphs
    (16 bytes). Wider glyphs are skipped — they can't share an 8px cell."""
    glyphs = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or ":" not in line:
                continue
            cp_s, bits = line.split(":", 1)
            if len(bits) != cell_h * 2:  # 8-wide == cell_h bytes == cell_h*2 hex digits
                continue
            rows = [int(bits[i:i + 2], 16) for i in range(0, len(bits), 2)]
            glyphs[int(cp_s, 16)] = [[(b >> (7 - c)) & 1 for c in range(8)] for b in rows]
    return glyphs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bdf")
    ap.add_argument("array_name")
    ap.add_argument("--first", type=int, default=32)
    ap.add_argument("--last", type=int, default=126)
    ap.add_argument("--map", action="append", default=[],
                    metavar="DEST:CODEPOINT",
                    help="source slot DEST from another codepoint's glyph")
    ap.add_argument("--extra", action="append", default=[], metavar="FILE",
                    help="hand-drawn glyph file (wins over --map for a slot)")
    ap.add_argument("--donor", metavar="FILE",
                    help="fallback BDF for --map codepoints the primary lacks")
    ap.add_argument("--hexfont", metavar="FILE",
                    help="GNU Unifont .hex to source --hexmap glyphs from")
    ap.add_argument("--hexmap", action="append", default=[], metavar="DEST:CODEPOINT",
                    help="bake an 8-wide Unifont glyph into slot DEST (needs --hexfont)")
    ap.add_argument("--hexshift", type=int, default=0, metavar="N",
                    help="translate --hexmap glyphs up N pixels (Unifont sits ~2px "
                         "lower than most BDF fonts; N>0 lifts them to match)")
    args = ap.parse_args()

    remap = {}
    for spec in args.map:
        dest, cp = (int(x) for x in spec.split(":"))
        if not args.first <= dest <= args.last:
            sys.exit(f"error: --map dest {dest} outside {args.first}..{args.last}")
        remap[dest] = cp

    cell_w, cell_h, ascent, glyphs = parse_bdf(args.bdf)
    if (cell_w * cell_h) % 8 != 0:
        sys.exit(f"error: cell {cell_w}x{cell_h} is not byte-divisible")
    count = args.last - args.first + 1

    donor_ascent = None
    donor_glyphs = {}
    if args.donor:
        d_w, d_h, donor_ascent, donor_glyphs = parse_bdf(args.donor)
        if (d_w, d_h) != (cell_w, cell_h):
            sys.exit(f"error: --donor cell {d_w}x{d_h} != primary {cell_w}x{cell_h}")

    hexmap = {}
    hexglyphs = {}
    if args.hexmap:
        if not args.hexfont:
            sys.exit("error: --hexmap needs --hexfont")
        if cell_w != 8:
            sys.exit(f"error: --hexmap needs an 8-wide cell (font is {cell_w} wide)")
        hexglyphs = parse_hex(args.hexfont, cell_h)
        if args.hexshift:
            blank = [False] * cell_w
            for cp, grid in hexglyphs.items():
                # Shift up N rows: content moves toward row 0, blanks pad
                # the bottom. (A negative N shifts down.)
                n = args.hexshift
                hexglyphs[cp] = (grid[n:] + [blank] * n) if n >= 0 else ([blank] * -n + grid[:n])
        for spec in args.hexmap:
            dest, cp = (int(x, 0) for x in spec.split(":"))
            if not args.first <= dest <= args.last:
                sys.exit(f"error: --hexmap dest {dest} outside {args.first}..{args.last}")
            if cp not in hexglyphs:
                sys.exit(f"error: --hexmap U+{cp:04X} not an 8-wide glyph in {args.hexfont}")
            hexmap[dest] = cp

    hand = {}
    hand_name = {}
    for path in args.extra:
        for slot, (name, grid) in parse_extra(path, cell_w, cell_h).items():
            if not args.first <= slot <= args.last:
                sys.exit(f"error: --extra slot {slot} outside {args.first}..{args.last}")
            hand[slot] = grid
            hand_name[slot] = name

    out = [cell_w, cell_h, args.first, count]
    for slot in range(args.first, args.last + 1):
        cp = remap.get(slot, slot)
        if slot in hand:
            grid = hand[slot]
        elif slot in hexmap:
            grid = hexglyphs[hexmap[slot]]
        elif cp in glyphs:
            grid = render_cell(cell_w, cell_h, ascent, glyphs[cp])
        elif cp in donor_glyphs:
            grid = render_cell(cell_w, cell_h, donor_ascent, donor_glyphs[cp])
        else:
            grid = [[False] * cell_w for _ in range(cell_h)]
        bits = [px for row in grid for px in row]
        for i in range(0, len(bits), 8):
            byte = 0
            for b in bits[i:i + 8]:
                byte = (byte << 1) | int(b)
            out.append(byte)

    per_glyph = (cell_w * cell_h) // 8
    print(f"// Generated by scripts/bdf_to_utft.py from {args.bdf.split('/')[-1]}")
    print(f"// {cell_w}x{cell_h}, chars {args.first}..{args.last}, "
          f"{len(out)} bytes. Do not edit by hand.")
    print(f"\nconst unsigned char {args.array_name}[] = {{")
    print(f"    0x{out[0]:02X}, 0x{out[1]:02X}, 0x{out[2]:02X}, 0x{out[3]:02X},")
    for g in range(count):
        base = 4 + g * per_glyph
        row = ", ".join(f"0x{b:02X}" for b in out[base:base + per_glyph])
        slot = args.first + g
        if slot in hand_name:
            label = hand_name[slot]
        elif slot in hexmap:
            label = f"U+{hexmap[slot]:04X} unifont"
        elif slot in remap:
            cp = remap[slot]
            label = f"U+{cp:04X}" if cp in glyphs else f"U+{cp:04X} donor"
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
