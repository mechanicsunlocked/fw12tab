#!/bin/bash
# Build wvkbd-mobintl with fw12tab's German (QWERTZ) layer and install it to
# $PREFIX/lib/fw12tab/wvkbd-mobintl. fw12tab prefers this binary over a stock
# wvkbd. Re-run after a wvkbd upgrade you want to track.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-/usr}"
# Pinned to the revision the layer patch was authored against; override to track
# upstream (the patch uses stable anchors but may need updating across releases).
WVKBD_URL="${WVKBD_URL:-https://git.sr.ht/~proycon/wvkbd}"
WVKBD_REF="${WVKBD_REF:-4fd182a58385b4754756e6dc66860e9ff601b3a1}"

command -v xkbcli >/dev/null || { echo "!! need xkbcli (libxkbcommon)" >&2; exit 1; }

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
echo "==> Fetching wvkbd ($WVKBD_REF)"
git clone -q "$WVKBD_URL" "$tmp/wvkbd"
git -C "$tmp/wvkbd" checkout -q "$WVKBD_REF"

echo "==> Generating German keymap + applying layer patch"
xkbcli compile-keymap --layout de > "$tmp/de.xkb"
python3 "$HERE/patch-german.py" "$tmp/wvkbd" "$tmp/de.xkb"

echo "==> Building"
make -C "$tmp/wvkbd" wvkbd-mobintl >/dev/null

echo "==> Installing to $PREFIX/lib/fw12tab/wvkbd-mobintl"
${SUDO:-sudo} install -Dm755 "$tmp/wvkbd/wvkbd-mobintl" "$PREFIX/lib/fw12tab/wvkbd-mobintl"
echo "==> Done. Layers: deutsch,special,nav (Super/Alt keys included)."
