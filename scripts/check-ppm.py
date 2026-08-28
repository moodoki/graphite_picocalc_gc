#!/usr/bin/env python3
"""Sanity-check a screen dump from graphite-shot.

Exists because the interesting failure of a host renderer is not a crash,
it is a *plausible* image. The D94 amendment describes the specific one:
`framebuffer.cpp` gates its render body on a preprocessor macro, so a
build that gets the macro wrong compiles, links, runs, and writes a
perfectly well-formed picture of nothing. Checking the file opened is not
enough; the pixels have to disagree with each other.
"""

import sys

EXPECT_W = 320
EXPECT_H = 320
HEADER = b"P6\n%d %d\n255\n" % (EXPECT_W, EXPECT_H)


def check(path: str) -> list[str]:
    with open(path, "rb") as f:
        data = f.read()

    problems = []
    if not data.startswith(HEADER):
        problems.append(f"header is {data[:16]!r}, expected {HEADER!r}")
        return problems  # Nothing below can be trusted without the header.

    px = data[len(HEADER):]
    expected = EXPECT_W * EXPECT_H * 3
    if len(px) != expected:
        problems.append(f"{len(px)} pixel bytes, expected {expected}")

    colours = {px[i:i + 3] for i in range(0, len(px) - 2, 3)}
    plural = "colour" if len(colours) == 1 else "colours"
    print(f"{path}: {EXPECT_W}x{EXPECT_H}, {len(colours)} distinct {plural}", flush=True)
    if len(colours) < 2:
        problems.append("image is a single flat colour -- the render path "
                        "produced nothing (see the D94 amendment)")
    return problems


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: check-ppm.py <file.ppm>", file=sys.stderr)
        return 2
    problems = check(argv[1])
    for p in problems:
        print(f"ERROR: {p}", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
