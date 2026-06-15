#!/bin/bash
# fw12tab — local / non-AUR one-shot installer.
# Installs dependencies and the fw12tab files, then wires up Hyprland.
# For the packaged route instead, see the README (yay -S fw12tab-git).
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-/usr}"

echo "==> Installing dependencies"
sudo pacman -S --needed --noconfirm bash iio-sensor-proxy jq gtk4 gtk4-layer-shell libnotify \
     base-devel git wayland wayland-protocols pango cairo libxkbcommon xkeyboard-config

echo "==> Building the on-screen keyboard (matches your system layout)"
PREFIX="$PREFIX" "$REPO_DIR/keyboard/build.sh"

echo "==> Building the keyboard toggle button (C / GTK4)"
cc -O2 -o "$REPO_DIR/lib/osk-button" "$REPO_DIR/lib/osk-button.c" \
   $(pkg-config --cflags --libs gtk4-layer-shell-0 gtk4)

echo "==> Ensuring iio-sensor-proxy is available"
systemctl is-enabled iio-sensor-proxy >/dev/null 2>&1 || true  # socket/dbus-activated; no enable needed

echo "==> Installing files into $PREFIX"
sudo install -Dm755 "$REPO_DIR/bin/fw12tab"        "$PREFIX/bin/fw12tab"
sudo install -Dm755 "$REPO_DIR/lib/osk-button"     "$PREFIX/lib/fw12tab/osk-button"
sudo install -Dm644 "$REPO_DIR/hypr/fw12tab.conf"  "$PREFIX/share/fw12tab/fw12tab.conf"
sudo install -Dm644 "$REPO_DIR/share/framework-logo.svg" "$PREFIX/share/fw12tab/framework-logo.svg"
sudo install -Dm644 "$REPO_DIR/README.md"          "$PREFIX/share/doc/fw12tab/README.md"
sudo install -Dm644 "$REPO_DIR/LICENSE"            "$PREFIX/share/licenses/fw12tab/LICENSE"

echo "==> Wiring up Hyprland (per-user)"
fw12tab setup

echo
echo "==> Done. Relaunch Hyprland if the daemons aren't running yet:"
echo "    hyprctl dispatch exec fw12tab autorotate"
echo "    hyprctl dispatch exec fw12tab tablet-watch"
