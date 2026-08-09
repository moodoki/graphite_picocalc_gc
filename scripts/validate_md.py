#!/usr/bin/env python3
"""Validate markdown documents.

Checks:
  - Math mode: unmatched $ signs, unicode math symbols outside math mode
  - Links: well-formed, no obviously suspicious targets
  - Code blocks: balanced (every ``` has a closing ```)
  - Tables: consistent column counts within each table

Usage:
    python3 scripts/validate_md.py [path]...
    python3 scripts/validate_md.py docs/**/*.md
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


UNICODE_MATH = {
    "\u00d7": "times",
    "\u00f7": "div",
    "\u00b1": "pm",
    "\u2264": "leq",
    "\u2265": "geq",
    "\u2260": "neq",
    "\u221e": "infty",
    "\u221a": "sqrt",
    "\u2211": "sum",
    "\u222b": "int",
    "\u2202": "partial",
    "\u03b1": "alpha",
    "\u03b2": "beta",
    "\u03b3": "gamma",
    "\u03b4": "delta",
    "\u03b5": "epsilon",
    "\u03bb": "lambda",
    "\u03bc": "mu",
    "\u03c3": "sigma",
    "\u03c6": "phi",
    "\u03c9": "omega",
    "\u03c0": "pi",
    "\u03b8": "theta",
}


def validate(path: Path) -> int:
    try:
        content = path.read_text()
    except OSError as e:
        print(f"  ERROR: cannot read {path}: {e}")
        return 1

    lines = content.split("\n")
    issues: list[str] = []

    # 1. Math mode: unmatched $ (skipping content inside backtick code)
    in_code = False
    for i, line in enumerate(lines, 1):
        if line.strip().startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            continue

        # Strip inline code spans (`...`) before counting $
        stripped_line = re.sub(r"`[^`]*`", "", line)

        # Count unescaped, non-$$ dollar signs
        count = 0
        j = 0
        while j < len(stripped_line):
            if stripped_line[j] == "$":
                # Skip $$
                if j + 1 < len(stripped_line) and stripped_line[j + 1] == "$":
                    j += 2
                    continue
                # Skip \$
                if j > 0 and stripped_line[j - 1] == "\\":
                    j += 1
                    continue
                count += 1
            j += 1
        if count % 2 != 0:
            issues.append(
                f"  Line {i}: unmatched $ ({count}): {line.strip()[:80]}"
            )

    # 2. Unicode math outside math mode
    in_code = False
    for i, line in enumerate(lines, 1):
        if line.strip().startswith("```"):
            in_code = not in_code
            continue
        if in_code or line.strip().startswith("$$"):
            continue

        # Strip inline code spans (`...`) — a unicode char quoted verbatim
        # in code (e.g. a literal glyph reference) isn't loose math prose.
        check_line = re.sub(r"`[^`]*`", "", line)

        for sym, name in UNICODE_MATH.items():
            if sym in check_line:
                pos = line.find(sym)
                before = line[:pos]
                # Check if inside $...$ on this line
                # (count unescaped $ before the symbol)
                d = 0
                j = 0
                while j < len(before):
                    if before[j] == "$" and (j == 0 or before[j - 1] != "\\"):
                        if j + 1 < len(before) and before[j + 1] == "$":
                            j += 2
                            continue
                        d += 1
                    j += 1
                if d % 2 == 0:
                    issues.append(
                        f"  Line {i}: unicode '{sym}' (\\{name}) "
                        f"outside math mode: {line.strip()[:80]}"
                    )

    # 3. Code blocks
    in_code = False
    code_start = 0
    for i, line in enumerate(lines, 1):
        if line.strip().startswith("```"):
            if in_code:
                in_code = False
            else:
                in_code = True
                code_start = i
    if in_code:
        issues.append(f"  Unclosed code block starting at line {code_start}")

    # 4. Tables
    in_table = False
    table_cols = 0
    table_start = 0
    in_code = False
    for i, line in enumerate(lines, 1):
        if line.strip().startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            in_table = False
            continue

        s = line.strip()
        if s.startswith("|") and s.endswith("|"):
            cols = s.count("|") - 1
            if not in_table:
                in_table = True
                table_cols = cols
                table_start = i
            elif cols != table_cols:
                issues.append(
                    f"  Line {i}: table has {cols} cols, expected "
                    f"{table_cols} (table started line {table_start})"
                )
        else:
            in_table = False

    # 5. Links (basic well-formedness check)
    link_re = re.compile(r"\[([^\]]*)\]\(([^)]*)\)")
    in_code = False
    for i, line in enumerate(lines, 1):
        if line.strip().startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            continue
        for m in link_re.finditer(line):
            text, url = m.group(1), m.group(2)
            if not url and not text:
                issues.append(f"  Line {i}: empty link")
            if url and not url.startswith(
                ("http://", "https://", "#", "/", "mailto:", "./", "../")
            ):
                # Heuristic: if the "URL" is just bare letters/numbers/digits,
                # it's likely an inline expression like [A](2,3). Skip.
                # A trailing "#anchor" on a relative path is a real link
                # (README.md#quick-start), so allow "#" here — inline math
                # never contains one.
                if not re.match(r"^[A-Za-z0-9_\-./,# ]+$", url):
                    issues.append(
                        f"  Line {i}: suspicious link target: {url}"
                    )

    if issues:
        print(f"\n{path}: {len(issues)} issue(s)")
        for issue in issues:
            print(issue)
        return 1
    print(f"  {path}: OK")
    return 0


def main(argv: list[str]) -> int:
    if len(argv) <= 1:
        print(__doc__)
        return 0

    paths: list[Path] = []
    for arg in argv[1:]:
        p = Path(arg)
        if p.is_dir():
            paths.extend(p.rglob("*.md"))
        elif p.is_file():
            paths.append(p)
        else:
            print(f"WARN: {arg} not found")

    if not paths:
        print("No markdown files to validate.")
        return 0

    failures = 0
    for path in sorted(set(paths)):
        failures += validate(path)

    print(f"\nValidated {len(paths)} file(s), {failures} with issues.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
