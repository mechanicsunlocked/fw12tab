# Maintainer: Sven Mathieu <mechanicsunlocked@drotiesel.lu>
pkgname=fw12tab-git
_pkgname=fw12tab
pkgver=r0.0000000
pkgrel=1
pkgdesc="Framework 12 tablet mode for Omarchy/Hyprland: event-driven auto-rotation + on-screen keyboard with a draggable toggle button"
arch=('any')
url="https://github.com/__GHUSER__/fw12tab"
license=('MIT')
depends=('bash' 'iio-sensor-proxy' 'wvkbd' 'jq' 'python-gobject' 'gtk4' 'libnotify')
optdepends=('hyprland: required for intended use')
makedepends=('git')
provides=("$_pkgname")
conflicts=("$_pkgname")
source=("git+$url.git")
sha256sums=('SKIP')
install="$_pkgname.install"

pkgver() {
  cd "$_pkgname"
  printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

package() {
  cd "$_pkgname"
  install -Dm755 bin/fw12tab       "$pkgdir/usr/bin/fw12tab"
  install -Dm755 lib/osk-button.py "$pkgdir/usr/lib/$_pkgname/osk-button.py"
  install -Dm644 hypr/fw12tab.conf "$pkgdir/usr/share/$_pkgname/fw12tab.conf"
  install -Dm644 README.md         "$pkgdir/usr/share/doc/$_pkgname/README.md"
  install -Dm644 LICENSE           "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
