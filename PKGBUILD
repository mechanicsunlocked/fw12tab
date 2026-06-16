# Maintainer: Sven Mathieu <mechanicsunlocked@drotiesel.lu>
pkgname=fw12tab-git
_pkgname=fw12tab
pkgver=r0.0000000
pkgrel=1
pkgdesc="Framework 12 tablet mode for Omarchy/Hyprland: auto-rotation + an on-screen keyboard that mirrors the FW12 layout, with a draggable toggle button"
arch=('x86_64')
url="https://github.com/mechanicsunlocked/fw12tab"
license=('MIT')
depends=('bash' 'iio-sensor-proxy' 'jq' 'gtk4' 'gtk4-layer-shell' 'libnotify'
         'wayland' 'libxkbcommon' 'xkeyboard-config' 'pango' 'cairo' 'glib2' 'harfbuzz')
optdepends=('hyprland: required for intended use')
makedepends=('git' 'gcc' 'pkgconf' 'wayland')
provides=("$_pkgname")
conflicts=("$_pkgname")
source=("git+$url.git")
sha256sums=('SKIP')
install="$_pkgname.install"

pkgver() {
  cd "$_pkgname"
  printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cd "$_pkgname"
  # On-screen keyboard (oskbd): C/GTK4 layer-shell keyboard that mirrors the FW12
  # layout and drives a Wayland virtual keyboard with the real system xkb keymap.
  wayland-scanner client-header proto/virtual-keyboard-unstable-v1.xml \
                  "$srcdir/virtual-keyboard-unstable-v1-client-protocol.h"
  wayland-scanner private-code  proto/virtual-keyboard-unstable-v1.xml \
                  "$srcdir/virtual-keyboard-unstable-v1-protocol.c"
  cc -O2 -I"$srcdir" -o "$srcdir/oskbd" lib/oskbd.c \
     "$srcdir/virtual-keyboard-unstable-v1-protocol.c" \
     $(pkg-config --cflags --libs gtk4 gtk4-layer-shell-0 xkbcommon wayland-client) -lm
  # Toggle button (C / GTK4 layer-shell).
  cc -O2 -o "$srcdir/osk-button" lib/osk-button.c \
     $(pkg-config --cflags --libs gtk4-layer-shell-0 gtk4)
  # Tablet-mode switch reader (C, kernel headers only).
  cc -O2 -o "$srcdir/tabletmode" lib/tabletmode.c
}

package() {
  cd "$_pkgname"
  install -Dm755 bin/fw12tab               "$pkgdir/usr/bin/fw12tab"
  install -Dm755 "$srcdir/oskbd"           "$pkgdir/usr/lib/$_pkgname/oskbd"
  install -Dm755 "$srcdir/osk-button"      "$pkgdir/usr/lib/$_pkgname/osk-button"
  install -Dm755 "$srcdir/tabletmode"      "$pkgdir/usr/lib/$_pkgname/tabletmode"
  # Tablet-switch bind service + resume hook (soc_button_array probe-race fix).
  install -Dm755 system/fw12tab-bind-tablet-switch "$pkgdir/usr/lib/$_pkgname/bind-tablet-switch"
  install -Dm644 system/fw12tab-tablet-switch.service "$pkgdir/usr/lib/systemd/system/fw12tab-tablet-switch.service"
  install -Dm755 system/fw12tab-tablet-switch-sleep "$pkgdir/usr/lib/systemd/system-sleep/fw12tab-tablet-switch"
  install -Dm644 hypr/fw12tab.conf         "$pkgdir/usr/share/$_pkgname/fw12tab.conf"
  install -Dm644 share/framework-logo.svg  "$pkgdir/usr/share/$_pkgname/framework-logo.svg"
  install -Dm644 README.md                 "$pkgdir/usr/share/doc/$_pkgname/README.md"
  install -Dm644 LICENSE                    "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
