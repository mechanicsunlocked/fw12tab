#!/usr/bin/env python3
"""fw12tab — floating, draggable on-screen-keyboard toggle button.

A small always-on-top button shown in tablet mode. Tap it to show/hide the
wvkbd on-screen keyboard; drag it to move it anywhere on screen.

It is a Wayland *layer-shell* surface (gtk4-layer-shell): that guarantees it
receives touch/pointer input while never taking keyboard focus, so the
keyboard types into whatever app you were actually using. Dragging is done by
moving the layer margins (compositor window-move doesn't apply to layers).
"""
import os
import subprocess
import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Gdk", "4.0")
gi.require_version("Gtk4LayerShell", "1.0")
from gi.repository import Gtk, Gdk, Gio, Gtk4LayerShell as LS  # noqa: E402

APP_ID = "org.fw12.OskButton"
SIZE = 60
MARGIN = 24             # default gap from screen edges
MOVE_THRESHOLD_SQ = 49  # ~7px before a press becomes a drag instead of a tap
ICON = os.environ.get("FW12TAB_OSK_ICON", "/usr/share/fw12tab/framework-logo.svg")
POS_FILE = os.path.join(os.environ.get("XDG_RUNTIME_DIR", "/tmp"),
                        "fw12tab-osk-pos")  # remember where it was dragged

# Round badge: black outer/inner, white logo. A thin grey ring keeps it
# visible on dark wallpapers.
CSS = """
window { background: transparent; }
.fw12-osk {
  background: #000000;
  border-radius: 9999px;
  border: 2px solid rgba(255, 255, 255, 0.18);
  box-shadow: 0 2px 6px rgba(0, 0, 0, 0.5);
}
.fw12-osk label { font-size: 28px; color: #ffffff; }
"""


class App(Gtk.Application):
    def __init__(self):
        super().__init__(application_id=APP_ID,
                         flags=Gio.ApplicationFlags.DEFAULT_FLAGS)

    def do_activate(self):
        win = Gtk.ApplicationWindow(application=self)
        win.set_decorated(False)
        win.set_resizable(False)
        win.set_default_size(SIZE, SIZE)

        # --- layer-shell: top layer, never grab the keyboard ---
        LS.init_for_window(win)
        # OVERLAY so the button always sits above the on-screen keyboard
        # (wvkbd is also on the overlay layer); never grab the keyboard.
        LS.set_layer(win, LS.Layer.OVERLAY)
        LS.set_keyboard_mode(win, LS.KeyboardMode.NONE)
        LS.set_namespace(win, "fw12tab-osk")
        # Anchor top-left so margins act as absolute x/y from that corner.
        LS.set_anchor(win, LS.Edge.TOP, True)
        LS.set_anchor(win, LS.Edge.LEFT, True)

        # Monitor size (logical px) for initial top-right placement + clamping.
        mw, mh = self._monitor_size()
        pos = self._load_pos(mw, mh)
        LS.set_margin(win, LS.Edge.LEFT, pos["x"])
        LS.set_margin(win, LS.Edge.TOP, pos["y"])

        provider = Gtk.CssProvider()
        provider.load_from_string(CSS)
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(), provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

        box = Gtk.Box()
        box.add_css_class("fw12-osk")
        box.set_hexpand(True)
        box.set_vexpand(True)
        if os.path.exists(ICON):  # white Framework logo centered on the badge
            child = Gtk.Picture.new_for_filename(ICON)
            child.set_content_fit(Gtk.ContentFit.CONTAIN)
            child.set_size_request(34, 34)
            child.set_halign(Gtk.Align.CENTER)
            child.set_valign(Gtk.Align.CENTER)
        else:  # fallback so the button is never invisible
            child = Gtk.Label(label="⌨")
        child.set_hexpand(True)
        child.set_vexpand(True)
        box.append(child)
        win.set_child(box)

        # --- drag to move (updates layer margins), tap to toggle ---
        start = {"x": 0, "y": 0}
        drag = Gtk.GestureDrag()
        drag.set_button(0)  # any button / touch

        def on_begin(_g, _x, _y):
            start["x"], start["y"] = pos["x"], pos["y"]

        def on_update(_g, off_x, off_y):
            if off_x * off_x + off_y * off_y < MOVE_THRESHOLD_SQ:
                return
            pos["x"] = min(max(0, int(start["x"] + off_x)), max(0, mw - SIZE))
            pos["y"] = min(max(0, int(start["y"] + off_y)), max(0, mh - SIZE))
            LS.set_margin(win, LS.Edge.LEFT, pos["x"])
            LS.set_margin(win, LS.Edge.TOP, pos["y"])

        def on_end(_g, _ox, _oy):
            try:  # remember position so a respawn lands where the user left it
                with open(POS_FILE, "w") as f:
                    f.write("%d %d" % (pos["x"], pos["y"]))
            except OSError:
                pass

        drag.connect("drag-begin", on_begin)
        drag.connect("drag-update", on_update)
        drag.connect("drag-end", on_end)
        win.add_controller(drag)

        # GestureClick reliably fires for a touch tap; it is denied (so it
        # won't toggle) when GestureDrag claims the sequence as a real move.
        click = Gtk.GestureClick()
        click.set_button(0)

        def on_released(_g, _n, _x, _y):
            subprocess.Popen(["fw12tab", "osk-toggle"])

        click.connect("released", on_released)
        win.add_controller(click)

        win.present()

    def _load_pos(self, mw, mh):
        x, y = max(0, mw - SIZE - MARGIN), MARGIN * 2  # default: top-right
        try:
            sx, sy = open(POS_FILE).read().split()
            x = min(max(0, int(sx)), max(0, mw - SIZE))
            y = min(max(0, int(sy)), max(0, mh - SIZE))
        except (OSError, ValueError):
            pass
        return {"x": x, "y": y}

    def _monitor_size(self):
        try:
            mons = Gdk.Display.get_default().get_monitors()
            geo = mons.get_item(0).get_geometry()
            return geo.width, geo.height
        except Exception:
            return 1280, 800  # safe fallback


if __name__ == "__main__":
    App().run(None)
