# Maintainer: Sven Mathieu <mechanicsunlocked@drotiesel.lu>
pkgname=fw12tab-git
_pkgname=fw12tab
# Revision of ~proycon/wvkbd the German layer patch is built against.
_wvkbd_commit=4fd182a58385b4754756e6dc66860e9ff601b3a1
pkgver=r0.0000000
pkgrel=1
pkgdesc="Framework 12 tablet mode for Omarchy/Hyprland: auto-rotation + on-screen keyboard (German QWERTZ) with a draggable toggle button"
arch=('x86_64')
url="https://github.com/mechanicsunlocked/fw12tab"
license=('MIT')
depends=('bash' 'iio-sensor-proxy' 'jq' 'gtk4' 'gtk4-layer-shell'
         'libnotify' 'wayland' 'libxkbcommon' 'pango' 'cairo' 'glib2' 'harfbuzz')
optdepends=('hyprland: required for intended use')
makedepends=('git' 'gcc' 'pkgconf' 'wayland-protocols' 'xkeyboard-config')
provides=("$_pkgname")
conflicts=("$_pkgname")
source=("git+$url.git"
        "wvkbd::git+https://git.sr.ht/~proycon/wvkbd.git#commit=$_wvkbd_commit")
sha256sums=('SKIP' 'SKIP')
install="$_pkgname.install"

pkgver() {
  cd "$_pkgname"
  printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cd "$_pkgname"
  # On-screen keyboard: generate a layer matching the build host's layout and
  # compile wvkbd (self-contained C generator, no Python / xkbcli subprocess).
  cc -O2 -o "$srcdir/genlayout" keyboard/genlayout.c -lxkbcommon
  _layout="${FW12TAB_KB_LAYOUT:-$(localectl status 2>/dev/null | sed -nE 's/.*X11 Layout:[[:space:]]*([^ ,]+).*/\1/p')}"
  "$srcdir/genlayout" "$srcdir/wvkbd" "${_layout:-us}" "" ""
  make -C "$srcdir/wvkbd" wvkbd-mobintl
  # Toggle button (C / GTK4 layer-shell).
  cc -O2 -o "$srcdir/osk-button" lib/osk-button.c \
     $(pkg-config --cflags --libs gtk4-layer-shell-0 gtk4)
}

package() {
  cd "$_pkgname"
  install -Dm755 bin/fw12tab               "$pkgdir/usr/bin/fw12tab"
  install -Dm755 "$srcdir/osk-button"      "$pkgdir/usr/lib/$_pkgname/osk-button"
  install -Dm755 "$srcdir/wvkbd/wvkbd-mobintl" "$pkgdir/usr/lib/$_pkgname/wvkbd-mobintl"
  install -Dm644 hypr/fw12tab.conf         "$pkgdir/usr/share/$_pkgname/fw12tab.conf"
  install -Dm644 share/framework-logo.svg  "$pkgdir/usr/share/$_pkgname/framework-logo.svg"
  install -Dm644 README.md                 "$pkgdir/usr/share/doc/$_pkgname/README.md"
  install -Dm644 LICENSE                    "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
