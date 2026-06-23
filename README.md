# fw12tab

Tablet-mode support for the **Framework Laptop 12** on **Omarchy** / Hyprland:

- 🔄 **Auto-rotation** — event-driven via `iio-sensor-proxy` (≈0 % idle CPU, no polling).
- 🧭 **Tablet detection** — reads the EC hinge angle (`cros-ec-lid-angle`); the Framework 12
  exposes no tablet-mode switch or sensor interrupt, so a single sysfs read every 2 s is the
  lowest-CPU option available (folding is slow, so 2 s is plenty).
- ⌨️ **On-screen keyboard** — a small self-contained **C / GTK4 layer-shell keyboard** (`oskbd`)
  that mirrors the FW12 physical layout and uploads the real system xkb keymap, so it types
  exactly like the built-in keyboard. It comes with a **floating, draggable, always-on-top
  button** that appears when you fold into tablet mode. Tap it to show/hide the keyboard; drag it
  anywhere. It never steals keyboard focus, so the keyboard types into the app you were using.
- 📲 **Touch app launcher** — pull down from the top edge in tablet mode to open a tile launcher.

Inspired by [FW12Rotate](https://github.com/2disbetter/FW12Rotate); rewritten to be one small
command, fully event-driven for rotation, and to add the tablet keyboard.

## Install

### AUR / one-shot (recommended)
```bash
yay -S fw12tab-git      # pulls deps (iio-sensor-proxy, jq, gtk4, gtk4-layer-shell, libxkbcommon…)
fw12tab setup           # wire up Hyprland (run once, as your user)
```
Then relaunch Hyprland (`Super+Esc → Relaunch`, or `hyprctl reload`).

### From source
```bash
git clone https://github.com/mechanicsunlocked/fw12tab && cd fw12tab
makepkg -si             # builds + installs via pacman
# — or, without packaging —
./install.sh
```

## Usage

| Action | Result |
|---|---|
| `Super + R` | Toggle auto-rotation on/off (notification confirms) |
| Fold past ~200° | Keyboard button appears (top-right by default) |
| Tap the button | Show / hide the on-screen keyboard |
| Drag the button | Move it anywhere on screen |
| Unfold (<170°) | Button and keyboard disappear |

## How it works

| Piece | Command | Runs as |
|---|---|---|
| Rotation daemon | `fw12tab autorotate` | `exec-once` (Hyprland) |
| Tablet watcher | `fw12tab tablet-watch` | `exec-once` (Hyprland) |
| Rotation toggle | `fw12tab rotate-toggle` | `Super+R` |
| Keyboard toggle | `fw12tab osk-toggle` | tap on the button |
| On-screen keyboard | `/usr/lib/fw12tab/oskbd` | spawned by the button / `Super+Shift+K` |
| Floating button | `/usr/lib/fw12tab/osk-button` | spawned in tablet mode |
| Touch launcher | `/usr/lib/fw12tab/touchlaunch` | top-edge pull-down (`edgeswipe`) |

`fw12tab setup` adds one line to `~/.config/hypr/hyprland.conf`:
`source = /usr/share/fw12tab/fw12tab.conf` (binds, `exec-once`s, and the window rules).

## Configuration

All optional, via environment variables (e.g. export in `~/.config/hypr/fw12tab.conf` host or your shell):

| Variable | Default | Meaning |
|---|---|---|
| `FW12TAB_ENTER` | `200` | Hinge angle (°) to enter tablet mode |
| `FW12TAB_EXIT` | `170` | Hinge angle (°) to leave tablet mode (hysteresis) |
| `FW12TAB_POLL` | `2` | Hinge poll interval (s) |
| `FW12TAB_OSK_HEIGHT` | `300` | Keyboard height (px) |
| `FW12TAB_DEBUG` | `0` | `1` = log to stderr |
| `FW12TAB_ANGLE_FILE` | auto | Override hinge sysfs path (testing) |

**Rotation comes out mirrored?** Swap the `1` and `3` in `_transform()` in `/usr/bin/fw12tab`
(panel mounting differs between units).

## Debugging

```bash
fw12tab doctor                          # deps, sensor, daemons, wiring — all in one
FW12TAB_DEBUG=1 fw12tab autorotate       # watch orientation events live
FW12TAB_DEBUG=1 fw12tab tablet-watch     # watch tablet enter/leave
FW12TAB_ANGLE_FILE=/tmp/a fw12tab tablet-watch &   # echo 250>/tmp/a to fake a fold
cat /sys/bus/iio/devices/iio:device*/in_angl_raw   # current hinge angle
monitor-sensor                           # raw accelerometer orientation stream
hyprctl clients | grep -A3 OskButton     # confirm the button is floating/pinned
```

## Uninstall
```bash
sudo pacman -R fw12tab-git    # or: fw12tab-git via your AUR helper
# then remove the `source = /usr/share/fw12tab/fw12tab.conf` line from ~/.config/hypr/hyprland.conf
```

## Requirements
Framework Laptop 12 (cros-ec hinge sensor + touchscreen), Omarchy / Hyprland, Wayland.
Runtime deps: `bash iio-sensor-proxy jq gtk4 gtk4-layer-shell libnotify libxkbcommon xkeyboard-config pango cairo`.
Build deps: `base-devel wayland wayland-protocols` (provides `cc`, `pkg-config`, `wayland-scanner`).
The one-shot `./install.sh` installs all of these via `pacman --needed`.

## License
MIT © 2026 Sven Mathieu
