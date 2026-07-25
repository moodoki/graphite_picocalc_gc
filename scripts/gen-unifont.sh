#!/usr/bin/env bash
# Regenerate src/gfx/fonts/unifont8x16.h from the GNU Unifont .hex
# (testdrive 2026-07-20 font test-drive; dual OFL 1.1 / GPL v2+ with font
# embedding exception, see drivers/unifont/COPYING). Unifont is a native
# 8x16 bitmap font, so its 8-wide glyphs are copied byte-for-byte — no
# rasterization, pixel-exact. The ~8 MB unifont_all .hex is NOT vendored;
# this fetches it to a temp dir. Pin UNIFONT_VER to reproduce byte-for-byte.
#
# Slot map matches the other 8x16 fonts (chars 32..134): high slots carry
# Unifont's real Greek/math glyphs. Slot 134 (imaginary unit) uses
# Unifont's own U+2139 (a serif i, distinct from the plain variable i) —
# Unifont's true math-italic i (U+1D456) is 16 wide and won't fit.

set -euo pipefail
cd "$(dirname "$0")/.."

UNIFONT_VER="${UNIFONT_VER:-17.0.05}"
HEX_URL="https://ftp.gnu.org/gnu/unifont/unifont-${UNIFONT_VER}/unifont_all-${UNIFONT_VER}.hex.gz"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Fetching unifont_all-${UNIFONT_VER}.hex.gz ..."
curl -fsSL --max-time 90 -o "$TMP/unifont_all.hex.gz" "$HEX_URL"
gunzip -f "$TMP/unifont_all.hex.gz"

PY=.venv/bin/python
[ -x "$PY" ] || PY=python3

# --shift 2: Unifont's baseline sits 2px lower than Spleen/Terminus,
# which left it looking low/off-centered (e.g. in the status bar); lift
# the whole font to match. (Clips only the grave accent's top pixel.)
"$PY" scripts/hex_to_utft.py "$TMP/unifont_all.hex" unifont8x16 \
    --first 32 --last 141 --shift 2 \
    --map 127:0x3C0 --map 128:0x2220 --map 129:0x3B8 --map 130:0x3C3 \
    --map 131:0x3A3 --map 132:0x3C7 --map 133:0x3BC --map 134:0x2139 \
    --map 135:0x21D2 --map 136:0x03BB --map 137:0x2260 --map 138:0x2026 --map 139:0x00B2 --map 140:0x221A --map 141:0x2093 \
    > src/gfx/fonts/unifont8x16.h

if command -v clang-format &>/dev/null; then
    clang-format -i src/gfx/fonts/unifont8x16.h
fi

echo "Regenerated src/gfx/fonts/unifont8x16.h (Unifont ${UNIFONT_VER})"
