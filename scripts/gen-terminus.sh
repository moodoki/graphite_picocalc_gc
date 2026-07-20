#!/usr/bin/env bash
# Regenerate src/gfx/fonts/terminus8x16.h from the Terminus ter-u16n BDF,
# with the math/complex high-slot glyphs baked from GNU Unifont
# (testdrive 2026-07-20 font test-drive). Terminus is SIL OFL 1.1 (see
# drivers/terminus/OFL.TXT); Unifont is dual OFL/GPL (drivers/unifont/).
# Neither font file is vendored — both are fetched to a temp dir.
#
# Slot map matches the other 8x16 fonts (chars 32..134): 32..126 come
# from Terminus, and slots 127..134 (pi, angle, theta, sigma, Sigma, chi,
# mu, imaginary-unit i=U+2139) come pixel-exact from Unifont so the math
# set is consistent regardless of Terminus's own Greek.

set -euo pipefail
cd "$(dirname "$0")/.."

TERM_VER="${TERM_VER:-4.39}"
TERM_URL="https://raw.githubusercontent.com/Tecate/bitmap-fonts/master/bitmap/terminus-font-${TERM_VER}/ter-u16n.bdf"
UNIFONT_VER="${UNIFONT_VER:-17.0.05}"
HEX_URL="https://ftp.gnu.org/gnu/unifont/unifont-${UNIFONT_VER}/unifont_all-${UNIFONT_VER}.hex.gz"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Fetching Terminus ter-u16n.bdf (${TERM_VER}) and Unifont ${UNIFONT_VER} .hex ..."
curl -fsSL --max-time 60 -o "$TMP/ter-u16n.bdf" "$TERM_URL"
curl -fsSL --max-time 90 -o "$TMP/unifont_all.hex.gz" "$HEX_URL"
gunzip -f "$TMP/unifont_all.hex.gz"

PY=.venv/bin/python
[ -x "$PY" ] || PY=python3

# --hexshift 2: Unifont's baseline sits 2px lower than Terminus, so lift
# the baked math glyphs to line up with Terminus's characters.
"$PY" scripts/bdf_to_utft.py "$TMP/ter-u16n.bdf" terminus8x16 \
    --first 32 --last 140 --hexfont "$TMP/unifont_all.hex" --hexshift 2 \
    --hexmap 127:0x3C0 --hexmap 128:0x2220 --hexmap 129:0x3B8 --hexmap 130:0x3C3 \
    --hexmap 131:0x3A3 --hexmap 132:0x3C7 --hexmap 133:0x3BC --hexmap 134:0x2139 \
    --hexmap 135:0x21D2 --hexmap 136:0x03BB --hexmap 137:0x2260 --hexmap 138:0x2026 --hexmap 139:0x00B2 --hexmap 140:0x221A \
    > src/gfx/fonts/terminus8x16.h

if command -v clang-format &>/dev/null; then
    clang-format -i src/gfx/fonts/terminus8x16.h
fi

echo "Regenerated src/gfx/fonts/terminus8x16.h (Terminus ${TERM_VER} + Unifont ${UNIFONT_VER} math)"
