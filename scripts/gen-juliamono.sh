#!/usr/bin/env bash
# Regenerate src/gfx/fonts/juliamono8x16.h from the JuliaMono TTF
# (testdrive 2026-07-20 font test-drive; SIL OFL 1.1, see
# drivers/juliamono/LICENSE). The 3.2 MB TTF is NOT vendored — this
# script fetches it to a temp dir so the committed header stays the
# single artifact. Needs freetype-py in the project .venv
# (requirements-dev.txt).
#
# Slot map matches the Spleen 8x16 font (chars 32..134): the high slots
# carry the math/complex glyphs, here pulled straight from JuliaMono's
# real Unicode codepoints instead of hand-drawn (bdf_to_utft --extra).

set -euo pipefail
cd "$(dirname "$0")/.."

TTF_URL="https://github.com/cormullion/juliamono/raw/master/JuliaMono-Regular.ttf"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
TTF="$TMP/JuliaMono-Regular.ttf"

echo "Fetching JuliaMono-Regular.ttf ..."
curl -fsSL --max-time 60 -o "$TTF" "$TTF_URL"

PY=.venv/bin/python
[ -x "$PY" ] || PY=python3

"$PY" scripts/ttf_to_utft.py "$TTF" juliamono8x16 \
    --first 32 --last 140 --width 8 --height 16 --pxsize 14 --baseline 13 \
    --map 127:0x3C0 --map 128:0x2220 --map 129:0x3B8 --map 130:0x3C3 \
    --map 131:0x3A3 --map 132:0x3C7 --map 133:0x3BC --map 134:0x1D456 \
    --map 135:0x21D2 --map 136:0x03BB --map 137:0x2260 --map 138:0x2026 --map 139:0x00B2 --map 140:0x221A \
    > src/gfx/fonts/juliamono8x16.h

if command -v clang-format &>/dev/null; then
    clang-format -i src/gfx/fonts/juliamono8x16.h
fi

echo "Regenerated src/gfx/fonts/juliamono8x16.h"
echo "  slots: 127 pi 128 angle 129 theta 130 sigma 131 Sigma 132 chi 133 mu 134 i"
