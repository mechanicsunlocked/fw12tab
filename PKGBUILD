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
depends=('bash' 'iio-sensor-proxy' 'jq' 'python-gobject' 'gtk4' 'gtk4-layer-shell'
         'libnotify' 'wayland' 'libxkbcommon' 'pango' 'cairo' 'glib2' 'harfbuzz')
optdepends=('hyprland: required for intended use')
makedepends=('git' 'wayland-protocols' 'xkeyboard-config')
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
  # German (QWERTZ) on-screen keyboard: inject the layer + de keymap, compile.
  xkbcli compile-keymap --layout de > "$srcdir/de.xkb"
  python3 "$srcdir/$_pkgname/keyboard/patch-german.py" "$srcdir/wvkbd" "$srcdir/de.xkb"
  make -C "$srcdir/wvkbd" wvkbd-mobintl
}

package() {
  cd "$_pkgname"
  install -Dm755 bin/fw12tab               "$pkgdir/usr/bin/fw12tab"
  install -Dm755 lib/osk-button.py         "$pkgdir/usr/lib/$_pkgname/osk-button.py"
  install -Dm755 "$srcdir/wvkbd/wvkbd-mobintl" "$pkgdir/usr/lib/$_pkgname/wvkbd-mobintl"
  install -Dm644 hypr/fw12tab.conf         "$pkgdir/usr/share/$_pkgname/fw12tab.conf"
  install -Dm644 share/framework-logo.svg  "$pkgdir/usr/share/$_pkgname/framework-logo.svg"
  install -Dm644 README.md                 "$pkgdir/usr/share/doc/$_pkgname/README.md"
  install -Dm644 LICENSE                    "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
