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
]

# Files placed in the fixture storage root, so any screen showing the card
# shows the same card every time.
FIXTURE_FILES: dict[str, str] = {
    "readme.txt": "GraphCalc documentation fixture.\n",
    "notes/todo.txt": "buy milk\n",
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
    """A storage root with fixed contents, rebuilt from scratch each run."""
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
    if "keys" in entry:
        argv += ["--keyscript", str(SCRIPT_DIR / entry["keys"])]
    if "run" in entry:
        argv += ["--run", entry["run"]]
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
    build_fixture()
    SCRATCH.mkdir(parents=True, exist_ok=True)
    tmp_ppm = SCRATCH / "shot.ppm"

    stale: list[str] = []
    for entry in IMAGES:
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
