#!/bin/bash
# Build wvkbd-mobintl with an on-screen layer that matches the user's ACTUAL
# keyboard layout, and install it to $PREFIX/lib/fw12tab/wvkbd-mobintl.
#
# No per-layout profiles and no Python: a single C tool (genlayout, using
# libxkbcommon) reads the compiled xkb keymap and resolves each key's label, so
# US→QWERTY, DE→QWERTZ+umlauts, FR→AZERTY, then patches the wvkbd source.
#
# Layout detection: $FW12TAB_KB_LAYOUT, else Hyprland kb_layout, else localectl,
# else "us". Re-run after changing your layout or to track a wvkbd upgrade.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-/usr}"
WVKBD_URL="${WVKBD_URL:-https://git.sr.ht/~proycon/wvkbd}"
WVKBD_REF="${WVKBD_REF:-4fd182a58385b4754756e6dc66860e9ff601b3a1}"

# --- detect the system layout / variant / options --------------------------
hypr_kv() { { grep -E "^[[:space:]]*$1[[:space:]]*=" "$HOME/.config/hypr/input.conf" 2>/dev/null \
              | head -1 | sed -E "s/.*=[[:space:]]*//; s/[[:space:]]*(#.*)?$//"; } || true; }
LAYOUT="${FW12TAB_KB_LAYOUT:-$(hypr_kv kb_layout)}"
VARIANT="${FW12TAB_KB_VARIANT:-$(hypr_kv kb_variant)}"
OPTIONS="${FW12TAB_KB_OPTIONS:-$(hypr_kv kb_options)}"
[ -n "$LAYOUT" ] || LAYOUT=$(localectl status 2>/dev/null | sed -nE 's/.*X11 Layout:[[:space:]]*([^ ]+).*/\1/p' | head -1)
LAYOUT="${LAYOUT:-us}"; LAYOUT="${LAYOUT%%,*}"; VARIANT="${VARIANT%%,*}"
echo "==> Detected keyboard layout: ${LAYOUT}${VARIANT:+ ($VARIANT)}${OPTIONS:+ [$OPTIONS]}"

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
echo "==> Fetching wvkbd ($WVKBD_REF)"
git clone -q "$WVKBD_URL" "$tmp/wvkbd"
git -C "$tmp/wvkbd" checkout -q "$WVKBD_REF"

echo "==> Generating + injecting the 'system' layer (C / libxkbcommon)"
cc -O2 -o "$tmp/genlayout" "$HERE/genlayout.c" -lxkbcommon
"$tmp/genlayout" "$tmp/wvkbd" "$LAYOUT" "$VARIANT" "$OPTIONS"

echo "==> Building"
make -C "$tmp/wvkbd" wvkbd-mobintl >/dev/null

echo "==> Installing to $PREFIX/lib/fw12tab/wvkbd-mobintl"
${SUDO:-sudo} install -Dm755 "$tmp/wvkbd/wvkbd-mobintl" "$PREFIX/lib/fw12tab/wvkbd-mobintl"
echo "==> Done. Layer 'system' matches your $LAYOUT keyboard (Super/Alt keys included)."
