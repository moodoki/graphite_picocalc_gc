#!/usr/bin/env bash
# Regenerate the vendored Spleen font headers from the BDF sources
# (drivers/spleen/, BSD-2-Clause) via bdf_to_utft.py. Run this after
# editing the hand-drawn math glyphs in scripts/mathglyphs-8x16.txt.
#
# Slot map for the 8x16 main font (chars 32..134):
#   32..126  ASCII
#   127 pi    130 sigma  131 Sigma  133 mu   -> --map from the BDF
#   128 angle 129 theta  132 chi    134 i    -> hand-drawn (--extra)
# The 5x8 small font is ASCII plus a hand-drawn pi at slot 127
# (mathglyphs-5x8.txt) for the graph tick labels; the other math glyphs
# stay 8x16-only and render blank in the small font.

set -euo pipefail
cd "$(dirname "$0")/.."

python3 scripts/bdf_to_utft.py drivers/spleen/spleen-8x16.bdf spleen8x16 \
    --first 32 --last 141 \
    --map 127:960 --map 130:963 --map 131:931 --map 133:956 \
    --extra scripts/mathglyphs-8x16.txt > src/gfx/fonts/spleen8x16.h

python3 scripts/bdf_to_utft.py drivers/spleen/spleen-5x8.bdf spleen5x8 \
    --first 32 --last 127 \
    --extra scripts/mathglyphs-5x8.txt > src/gfx/fonts/spleen5x8.h

# Match the committed style (the tool emits one long line per glyph;
# clang-format rewraps and aligns the trailing comments).
if command -v clang-format &>/dev/null; then
    clang-format -i src/gfx/fonts/spleen8x16.h src/gfx/fonts/spleen5x8.h
fi

echo "Regenerated src/gfx/fonts/spleen8x16.h and spleen5x8.h"
