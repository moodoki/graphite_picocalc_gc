#!/usr/bin/env python3
"""Generate docs-site/images/*.png by driving the host renderer.

The calculator draws its own documentation screenshots. Before Phase 6.4
the README used a photograph of the device, which is the wrong instrument
for 1-bit glyph work and goes stale silently (issue #33).

Each entry below names a screen and how to reach it. Screens that can be
*constructed* take --eval; screens that must be NAVIGATED to take a key
script from docs-site/images/scripts/ (D97).

Determinism is a requirement, not a nicety (D98): CI regenerates the set
and fails on any difference, so anything time-, RNG- or
uninitialised-memory-dependent in a rendered screen becomes a CI flake.
The storage root is a fixture this script builds from scratch every run,
so the file manager's listing is fixed rather than being whatever happens
to be in ~/.picocalc.

Stdlib only, including the PNG encoder -- the renderer writes PPM
precisely so it needs no image library, and undoing that here by
depending on Pillow would put a package between CI and a docs image.

Usage:
    python3 scripts/gen-doc-images.py            # write the images
    python3 scripts/gen-doc-images.py --check    # fail if any would change
"""

from __future__ import annotations

import argparse
import binascii
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SHOT = ROOT / "build" / "host" / "graphite-shot"
OUT_DIR = ROOT / "docs-site" / "images"
SCRIPT_DIR = OUT_DIR / "scripts"
FIXTURE = ROOT / "build" / "host" / "doc-fixture"
# Scratch lives OUTSIDE the fixture. It did not at first, and the drift
# check caught it on its first run: the file manager photographs this
# directory, so a temporary shot.ppm sitting in it became part of the
# picture -- and --check wrote one more file than a plain run did, so the
# two disagreed forever. Exactly the class of flake D98 warns about.
SCRATCH = ROOT / "build" / "host" / "doc-scratch"

# The image set. `eval` lines are submitted to the home screen in order;
# `keys` names a script in docs-site/images/scripts/; `run` executes a
# Python file. Phase 6.4.8 extends this with one entry per softkey set and
# status-bar variant -- the sweep is a list of manifest entries, not new
# machinery.
IMAGES: list[dict] = [
    {
        "name": "home",
        "caption": "The home screen at startup.",
    },
    {
        "name": "natural-math",
        "caption": "Natural math display: radicals and stacked fractions.",
        "eval": ["sqrt(2)", "1/3", "2^10"],
    },
    {
        "name": "files-softkeys",
        "caption": "The file manager. Note the truncated softkey label (#52).",
        "keys": "files-softkeys.keys",
    },

    # ---- The chrome sweep (6.4.8) ----
    #
    # One entry per draw_softkeys / draw_status_bar call site in src/, in
    # its modal and flag variants. These are NOT documentation images --
    # they are a regression gate. D98's drift check turns the set into a
    # permanent one: a chrome change that moves any of these bars fails CI
    # rather than being noticed later, or not at all.
    #
    # Keep them even where they show nothing wrong. The value is the diff
    # on the day something does.

    # -- softkey bars --
    {
        "name": "chrome-yeq",
        "caption": "The Y= editor's softkey bar (F1 from home).",
        "key": "f1",
    },
    {
        "name": "chrome-window",
        "caption": "The WINDOW screen (F2 from home).",
        "key": "f2",
    },
    {
        "name": "chrome-mode",
        "caption": "The MODE screen's softkey bar (F3 from home).",
        "key": "f3",
    },
    {
        "name": "chrome-graph",
        "caption": "The graph screen's softkey bar (F5 from home).",
        "key": "f5",
    },
    {
        "name": "chrome-graph-param",
        "caption": "Parametric mode: F1 reads PAR, and the empty-graph message "
                   "is the longer variant.",
        "keys": "graph-param.keys",
    },
    {
        "name": "chrome-table",
        "caption": "The table screen's softkey bar (F5, F5 from home).",
        "keys": "table.keys",
    },
    {
        "name": "chrome-launcher",
        "caption": "The app launcher (F6 from home), including a tier-2 SD app.",
        "key": "f6",
    },
    {
        "name": "chrome-notepad",
        "caption": "The text editor widget's softkey bar, via Notepad.",
        "keys": "notepad.keys",
    },
    {
        "name": "chrome-python",
        "caption": "The Python editor's softkey bar (EDIT/BACK modal variant).",
        "keys": "python.keys",
    },
    {
        "name": "chrome-files-move",
        "caption": "The file manager with a cut armed: the MOVE label appears in F3.",
        "keys": "files-move.keys",
    },
    {
        "name": "chrome-settings",
        "caption": "The SETTINGS screen's softkey bar.",
        "eval": ["settings"],
    },

    # -- status-bar titles --
    #
    # draw_status_bar puts the title at x=4 and the right-aligned block at
    # a computed rx, with NOTHING clamping one against the other. Every
    # static title in src/ is here so that the day one of them grows, the
    # diff says so.
    {
        "name": "chrome-stats",
        "caption": "Status bar: STATS.",
        "eval": ["stats"],
    },
    {
        "name": "chrome-lists",
        "caption": "Status bar: LISTS.",
        "eval": ["lists"],
    },
    {
        "name": "chrome-matrix",
        "caption": "Status bar: MATRIX.",
        "eval": ["matrix"],
    },
    {
        "name": "chrome-const",
        "caption": "Status bar: CONSTANTS.",
        "eval": ["const"],
    },
    {
        "name": "chrome-dist",
        "caption": "Status bar: DIST.",
        "eval": ["dist"],
    },
    {
        "name": "chrome-test",
        "caption": "Status bar: TEST.",
        "eval": ["test"],
    },
    {
        "name": "chrome-plots",
        "caption": "Status bar: STAT PLOTS -- the longest static title, at 10.",
        "eval": ["plot"],
    },
    {
        "name": "chrome-cas",
        "caption": "Status bar: CAS.",
        "eval": ["cas"],
    },
    {
        "name": "chrome-solver",
        "caption": "Status bar: SOLVER.",
        "eval": ["solve"],
    },
    {
        "name": "chrome-analyze",
        "caption": "Status bar: ANALYZE, over the graph screen.",
        "eval": ["analyze"],
    },

    {
        "name": "chrome-help",
        "caption": "The HELP screen, whose title bar is hand-rolled rather than shared.",
        "eval": ["help"],
    },
    {
        "name": "chrome-table-setup",
        "caption": "Table setup (F5, F5, F2) -- another hand-rolled title bar.",
        "keys": "table-setup.keys",
    },

    # -- the states a user cannot reach on purpose --
    {
        "name": "chrome-unhealthy",
        "caption": "D26: SD and PSRAM reported down, on the home screen.",
        "args": ["--unhealthy", "sd,psram"],
    },
    {
        "name": "chrome-unhealthy-plots",
        "caption": "D26 indicators after the longest static title (STAT PLOTS).",
        "eval": ["plot"],
        "args": ["--unhealthy", "sd,psram"],
    },
    # #61's evidence. This was originally the SD app screen, whose status
    # bar shows a 23-character app name -- and CI rejected it, correctly:
    # the program screen also prints the MicroPython heap figure, which is
    # not the same on Linux and macOS. Nothing in the UI is wrong there;
    # the number is a property of the build. So the rule the sweep learned
    # is that a screen showing an interpreter's heap cannot be in a
    # byte-identical drift set, and the rule generalises -- anything the
    # host and the board can legitimately disagree about does not belong
    # in these images.
    #
    # The file manager makes the same point better anyway. It needs no
    # interpreter, the title is longer (char[40] against char[24]), and it
    # takes nothing but a folder a user made.
    {
        # The proof for #62. Before the fix this screen hand-rolled its
        # title bar and could not show these at all, which is the whole
        # of that bug: a failing card was invisible on the screen a user
        # was most likely to be sitting on.
        "name": "chrome-yeq-unhealthy",
        "caption": "The Y= editor with SD and PSRAM down -- indicators it "
                   "could not show before #62.",
        "key": "f1",
        "args": ["--unhealthy", "sd,psram"],
    },
    {
        "name": "chrome-files-deep",
        "caption": "A deep directory: the file manager's title runs straight "
                   "into the right-aligned block (#61).",
        "keys": "files-deep.keys",
    },
    {
        "name": "chrome-files-deep-unhealthy",
        "caption": "The same title with D26's SD and PSRAM indicators, which "
                   "it pushes into the block as well.",
        "keys": "files-deep.keys",
        "args": ["--unhealthy", "sd,psram"],
    },
]

# Files placed in the fixture storage root, so any screen showing the card
# shows the same card every time.
FIXTURE_FILES: dict[str, str] = {
    "readme.txt": "GraphCalc documentation fixture.\n",
    "notes/todo.txt": "buy milk\n",
    # A tier-2 SD app, so the launcher has a row that is not built in.
    # Its name is the longest SdAppManifest::name holds (char[24]), which
    # is also the shape of #61 -- but the image that DEMONSTRATES #61 is
    # the file manager below, not this app. See the note on
    # chrome-files-deep for why.
    "apps/longname/app.txt": "name=Mortgage Amortizer 2026\n",
    "apps/longname/main.py": "print('fixture app')\n",
    # A deep directory. The file manager's title is "FILES <cur_dir_>" in
    # a char[40] and nothing clamps it against the right-aligned block, so
    # walking down here is all it takes to make the status bar unreadable.
    "documents/2026/statements/jan.txt": "fixture\n",
}


def write_png(path: Path, width: int, height: int, rgb: bytes) -> None:
    """Encode 8-bit RGB as a PNG. Deterministic for identical input."""

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", binascii.crc32(tag + data) & 0xFFFFFFFF)
        )

    # Filter type 0 (None) on every scanline. Filtering would compress
    # better; it would also make the encoder something worth testing, and
    # these images are a few KB either way.
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw += rgb[y * stride:(y + 1) * stride]

    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(b"P6\n"):
        raise ValueError(f"{path}: not a binary PPM")
    # The renderer writes exactly "P6\n<w> <h>\n255\n" -- parse that
    # rather than the whole of PPM, and complain if it ever changes.
    header, _, rest = data[3:].partition(b"\n255\n")
    width, height = (int(v) for v in header.split())
    expected = width * height * 3
    if len(rest) != expected:
        raise ValueError(f"{path}: {len(rest)} pixel bytes, expected {expected}")
    return width, height, rest


def build_fixture() -> None:
    """A storage root with fixed contents, rebuilt before EVERY image.

    Per-image, not per-run, and that distinction was found by looking at
    the pictures. The calculator persists as it goes -- history.txt,
    variables.dat, graphstate.dat -- into the same root, so with one
    fixture per run each image inherited whatever the images before it had
    typed. The home screen in chrome-unhealthy.png was showing
    natural-math.png's radicals. The set stayed reproducible only as long
    as nobody reordered IMAGES, which is a trap rather than a property:
    every entry now renders from the same starting state, so an entry
    means what it says on its own.
    """
    if FIXTURE.exists():
        shutil.rmtree(FIXTURE)
    for rel, text in FIXTURE_FILES.items():
        target = FIXTURE / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
    FIXTURE.mkdir(parents=True, exist_ok=True)


def render(entry: dict, ppm_path: Path) -> None:
    argv = [str(SHOT)]
    for line in entry.get("eval", []):
        argv += ["--eval", line]
    if "key" in entry:
        argv += ["--key", entry["key"]]
    if "keys" in entry:
        argv += ["--keyscript", str(SCRIPT_DIR / entry["keys"])]
    if "run" in entry:
        argv += ["--run", entry["run"]]
    # Escape hatch for state the UI cannot be driven into: --unhealthy is
    # the only user so far (6.4.8). Deliberately raw argv rather than a
    # per-flag key, so a lever added to graphite-shot needs no change here.
    argv += entry.get("args", [])
    argv += ["--shot", str(ppm_path)]

    result = subprocess.run(
        argv,
        capture_output=True,
        text=True,
        # HOME as well as PICOCALC_HOME: if the storage backend ever stops
        # honouring the explicit root, this makes it fail loudly instead of
        # quietly rendering the developer's real ~/.picocalc into a
        # committed image.
        env={"PICOCALC_HOME": str(FIXTURE), "HOME": str(FIXTURE), "PATH": "/usr/bin:/bin"},
        check=False,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f"gen-doc-images: {entry['name']} failed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="regenerate into memory and fail if any committed image differs",
    )
    args = parser.parse_args()

    if not SHOT.exists():
        sys.stderr.write(
            f"gen-doc-images: {SHOT} not found.\n"
            "  Build it first: cmake -B build/host -S host && cmake --build build/host\n"
        )
        return 1

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    SCRATCH.mkdir(parents=True, exist_ok=True)
    tmp_ppm = SCRATCH / "shot.ppm"

    stale: list[str] = []
    for entry in IMAGES:
        build_fixture()
        render(entry, tmp_ppm)
        width, height, rgb = read_ppm(tmp_ppm)
        target = OUT_DIR / f"{entry['name']}.png"

        if args.check:
            before = target.read_bytes() if target.exists() else b""
            write_png(tmp_ppm.with_suffix(".png"), width, height, rgb)
            after = tmp_ppm.with_suffix(".png").read_bytes()
            if before != after:
                stale.append(entry["name"])
            continue

        write_png(target, width, height, rgb)
        print(f"  {target.relative_to(ROOT)}  ({width}x{height})")

    if args.check:
        if stale:
            sys.stderr.write(
                "gen-doc-images: these images no longer match the firmware:\n"
                + "".join(f"  {n}\n" for n in stale)
                + "Regenerate with: python3 scripts/gen-doc-images.py\n"
            )
            return 1
        print(f"gen-doc-images: {len(IMAGES)} images match.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
