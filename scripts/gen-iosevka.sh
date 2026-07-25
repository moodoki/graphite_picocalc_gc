#!/usr/bin/env bash
# Regenerate src/gfx/fonts/iosevka8x16.h from the Iosevka Fixed TTF
# (testdrive 2026-07-20 font test-drive; SIL OFL 1.1, see
# drivers/iosevka/LICENSE.md). The upstream PkgTTF zip is ~140 MB and is
# NOT vendored — this fetches it to a temp dir, extracts the Regular
# face, and rasterizes. Needs freetype-py in the project .venv
# (requirements-dev.txt).
#
# Slot map matches the Spleen/JuliaMono 8x16 fonts (chars 32..134): the
# high slots carry the math/complex glyphs from Iosevka's real Unicode
# codepoints. Pin IOSEVKA_VER to reproduce a byte-identical header.

set -euo pipefail
cd "$(dirname "$0")/.."

IOSEVKA_VER="${IOSEVKA_VER:-34.7.0}"
ZIP_URL="https://github.com/be5invis/Iosevka/releases/download/v${IOSEVKA_VER}/PkgTTF-IosevkaFixed-${IOSEVKA_VER}.zip"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Fetching PkgTTF-IosevkaFixed-${IOSEVKA_VER}.zip ..."
curl -fsSL --max-time 180 -o "$TMP/iosevka.zip" "$ZIP_URL"
unzip -o -q "$TMP/iosevka.zip" IosevkaFixed-Regular.ttf -d "$TMP"

PY=.venv/bin/python
[ -x "$PY" ] || PY=python3

"$PY" scripts/ttf_to_utft.py "$TMP/IosevkaFixed-Regular.ttf" iosevka8x16 \
    --first 32 --last 141 --width 8 --height 16 --pxsize 15 --baseline 13 \
    --map 127:0x3C0 --map 128:0x2220 --map 129:0x3B8 --map 130:0x3C3 \
    --map 131:0x3A3 --map 132:0x3C7 --map 133:0x3BC --map 134:0x1D456 \
    --map 135:0x21D2 --map 136:0x03BB --map 137:0x2260 --map 138:0x2026 --map 139:0x00B2 --map 140:0x221A --map 141:0x2093 \
    > src/gfx/fonts/iosevka8x16.h

if command -v clang-format &>/dev/null; then
    clang-format -i src/gfx/fonts/iosevka8x16.h
fi

echo "Regenerated src/gfx/fonts/iosevka8x16.h (Iosevka Fixed ${IOSEVKA_VER})"
