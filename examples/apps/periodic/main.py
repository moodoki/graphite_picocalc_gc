# Periodic table — the app phase6-spec.md §4.6 entry 1 has been holding
# open as the pressure test for the `calc` module.
#
# TI-83 Periodic Table app in shape: an 18-column grid you walk with the
# arrow keys, and a detail panel for whatever is under the cursor. What
# is different here is colour — every cell is painted by its element
# series, which is what the grid is actually *for* once you can see it.
#
# Data lives next door in elements.csv and is re-read at every start, so
# correcting a mass or renaming a series needs a text editor, not a
# firmware build. That is the whole point of /picocalc/apps.

import calc
import gc

DATA = "/picocalc/apps/periodic/elements.csv"


def heap(stage):
    """Report the Python heap over USB serial.

    This app is the first real answer to "how much of the 40 KB does a
    modest reference dataset actually cost" (phase6-spec.md §4.4), so it
    says. A drawing script owns the panel, so print() reaches serial and
    nothing else — the display is never touched by this.
    """
    print("periodic: %s, heap %d free" % (stage, gc.mem_free()))

# ---- Layout ----------------------------------------------------------
#
# The panel is 320x320 and the font is 8x16, so a 16px cell holds exactly
# a two-character symbol. Nothing here is a magic number; they all fall
# out of those three.
#
# PITCH is one pixel wider than CELL, and that pixel is the whole of
# issue #43. The canvas has no transparent text — a glyph is drawn as a
# filled cell, because there is no framebuffer to read back into
# (calc_canvas.hpp) — so a symbol paints its 16x16 edge to edge, and at
# a 16px pitch two neighbours in the same series fused into one slab you
# could not read. Drawing the outline OVER the glyph afterwards was the
# obvious fix and is wrong: across the five 8x16 fonts the firmware can
# be built with, every edge of the glyph box is lit by some letter
# (Terminus lights the left column for M/T/W/Y, Spleen for almost every
# capital, Iosevka the bottom row for descenders), so an outline drawn
# on top shaves real pixels off real symbols. A 17px pitch instead
# leaves a gutter column and row that no glyph can ever reach, and the
# outline is drawn in it.
#
# 18 columns at 17 come to 305, so the margins are 8 and 7.

CELL = 16   # painted cell, and exactly two characters of the 8x16 font
PITCH = 17  # cell + the 1px gutter the outline lives in
GRID_X = 8
GRID_Y = 22
FGAP = 8  # blank strip between the main block and the lanthanides
DETAIL_Y = 184
HINT_Y = 300

BG = (0, 0, 0)
WHITE = (255, 255, 255)
DIM = (150, 150, 150)
GRID_INK = (70, 70, 70)  # cell outline, drawn in the gutter

SERIES_NAME = (
    "Alkali metal",
    "Alkaline earth",
    "Transition metal",
    "Post-transition",
    "Metalloid",
    "Nonmetal",
    "Halogen",
    "Noble gas",
    "Lanthanide",
    "Actinide",
)

SERIES_COLOR = (
    (212, 68, 60),
    (228, 132, 48),
    (214, 176, 72),
    (104, 158, 112),
    (66, 170, 164),
    (66, 132, 220),
    (140, 108, 212),
    (198, 88, 176),
    (88, 164, 96),
    (172, 120, 84),
)


def ink_for(rgb):
    """Black or white, whichever the cell colour can actually carry.

    Ten hand-picked colours would otherwise need ten hand-picked text
    colours kept in step with them; this cannot drift.
    """
    lum = (rgb[0] * 299 + rgb[1] * 587 + rgb[2] * 114) // 1000
    return (0, 0, 0) if lum > 140 else WHITE


SERIES_INK = [ink_for(c) for c in SERIES_COLOR]

# ---- Data ------------------------------------------------------------
#
# Parallel lists indexed by atomic number rather than a list of dicts:
# 118 dicts would cost more of the Pico 1's 40 KB Python heap than the
# whole rest of the app, and Z is a perfect index already. Slot 0 is
# unused so that SYM[26] is iron.

SYM = [""] * 119
NAME = [""] * 119
MASS = [""] * 119
SERIES = [0] * 119
COL = [0] * 119
ROW = [0] * 119

# Reverse lookup for navigation: which element sits at (column, row).
# A flat list of small ints, not a dict keyed by tuples — 180 slots cost
# a few hundred bytes, where 118 tuple keys would cost several KB.
GRID = [0] * (18 * 10)


def slot(col, row):
    return (row - 1) * 18 + (col - 1)


def load():
    text = calc.read_file(DATA)
    count = 0
    for line in text.split("\n"):
        if not line or line[0] == "#":
            continue
        if line[-1] == "\r":  # tolerate a file edited on Windows
            line = line[:-1]
        parts = line.split(",")
        if len(parts) != 7:
            continue
        z = int(parts[0])
        SYM[z] = parts[1]
        NAME[z] = parts[2]
        MASS[z] = parts[3]
        SERIES[z] = int(parts[4])
        COL[z] = int(parts[5])
        ROW[z] = int(parts[6])
        GRID[slot(COL[z], ROW[z])] = z
        count += 1
    return count


# ---- Drawing ---------------------------------------------------------


def cell_xy(z):
    x = GRID_X + (COL[z] - 1) * PITCH
    row = ROW[z]
    if row <= 7:
        return x, GRID_Y + (row - 1) * PITCH
    # Rows 9 and 10 are the f-block strips, drawn below the main table
    # with a gap. Row 8 does not exist and is what the gap is made of.
    return x, GRID_Y + 7 * PITCH + FGAP + (row - 9) * PITCH


def draw_cell(z, selected):
    x, y = cell_xy(z)
    sym = SYM[z]
    if len(sym) < 2:
        sym = sym + " "  # pad, so the background fills the whole cell
    s = SERIES[z]
    if selected:
        calc.draw_text(x, y, sym, SERIES_COLOR[s], WHITE)
    else:
        calc.draw_text(x, y, sym, SERIES_INK[s], SERIES_COLOR[s])
    # The ring sits entirely in the gutter around the painted cell, so
    # it never touches a glyph pixel (see PITCH above). Adjacent cells
    # share the gutter between them and so draw the same line twice,
    # which costs a redraw and keeps the arithmetic honest.
    calc.draw_rect(x - 1, y - 1, CELL + 2, CELL + 2, WHITE if selected else GRID_INK)


def draw_detail(z):
    calc.draw_rect(0, DETAIL_Y, 320, HINT_Y - DETAIL_Y, BG, True)
    s = SERIES[z]
    calc.draw_rect(8, DETAIL_Y, CELL * 2, CELL, SERIES_COLOR[s], True)
    sym = SYM[z]
    calc.draw_text(8 + (CELL * 2 - len(sym) * 8) // 2, DETAIL_Y, sym, SERIES_INK[s],
                   SERIES_COLOR[s])
    calc.draw_text(56, DETAIL_Y, NAME[z], WHITE, BG)

    calc.draw_text(8, DETAIL_Y + 22, "Z %d" % z, WHITE, BG)
    calc.draw_text(136, DETAIL_Y + 22, "Mass %s" % MASS[z], WHITE, BG)

    row = ROW[z]
    if row <= 7:
        group = "Group %d" % COL[z]
        period = row
    else:
        # An f-block element has no group in an 18-column table; its
        # period is the row it was pulled out of, not the strip it is
        # drawn in.
        group = "Group -"
        period = 6 if row == 9 else 7
    calc.draw_text(8, DETAIL_Y + 42, group, WHITE, BG)
    calc.draw_text(136, DETAIL_Y + 42, "Period %d" % period, WHITE, BG)

    calc.draw_text(8, DETAIL_Y + 62, SERIES_NAME[s], SERIES_COLOR[s], BG)


def draw_all(z):
    calc.clear_screen(BG)
    calc.draw_text(8, 2, "PERIODIC TABLE", WHITE, BG)
    for i in range(1, 119):
        if SYM[i]:
            draw_cell(i, i == z)
    draw_detail(z)
    calc.draw_text(8, HINT_Y, "ARROWS move  F1 key  ESC exit", DIM, BG)


def show_legend():
    calc.clear_screen(BG)
    calc.draw_text(8, 8, "ELEMENT SERIES", WHITE, BG)
    for i in range(10):
        y = 40 + i * 24
        calc.draw_rect(16, y, 24, CELL, SERIES_COLOR[i], True)
        calc.draw_text(52, y, SERIES_NAME[i], SERIES_COLOR[i], BG)
    calc.draw_text(8, HINT_Y, "Any key returns", DIM, BG)
    calc.wait_key()


# ---- Navigation ------------------------------------------------------


def step_h(z, dc):
    """Nearest element left or right in the same row, or stay put.

    Walking rather than stepping one column is what carries the cursor
    over the ten-column gap in periods 2 and 3, and over the notch the
    f-block leaves in periods 6 and 7.
    """
    c = COL[z] + dc
    while 1 <= c <= 18:
        t = GRID[slot(c, ROW[z])]
        if t:
            return t
        c += dc
    return z


def step_v(z, dr):
    """Nearest element up or down.

    Straight up from a lanthanide is column 3, which is empty in every
    row of the main block — that is the column the f-block was pulled
    out of. So when the column is empty, take the closest occupied cell
    in that row instead of falling off the table.
    """
    col = COL[z]
    r = ROW[z] + dr
    while 1 <= r <= 10:
        t = GRID[slot(col, r)]
        if t:
            return t
        best = 0
        best_d = 99
        for c in range(1, 19):
            t = GRID[slot(c, r)]
            if t and abs(c - col) < best_d:
                best_d = abs(c - col)
                best = t
        if best:
            return best
        r += dr
    return z


def main():
    gc.collect()
    heap("start")
    n = load()
    gc.collect()
    heap("%d elements loaded" % n)
    if n == 0:
        calc.clear_screen(BG)
        calc.draw_text(8, 8, "No data: " + DATA, (255, 80, 80), BG)
        calc.wait_key()
        return

    z = 1
    draw_all(z)
    while True:
        ev = calc.wait_key()
        key = ev["name"]
        ch = ev["ch"]
        moved = z
        if key == "left":
            moved = step_h(z, -1)
        elif key == "right":
            moved = step_h(z, 1)
        elif key == "up":
            moved = step_v(z, -1)
        elif key == "down":
            moved = step_v(z, 1)
        elif key == "f1":
            show_legend()
            draw_all(z)
            continue
        elif ch == "q":
            return

        if moved != z:
            # Only the two cells that changed, plus the panel. Redrawing
            # all 118 for every arrow press would make the cursor crawl.
            draw_cell(z, False)
            draw_cell(moved, True)
            z = moved
            draw_detail(z)


try:
    main()
except KeyboardInterrupt:
    # ESC. The drawing stays on the panel; a second ESC leaves for the
    # launcher, which is the app convention everywhere else.
    pass
