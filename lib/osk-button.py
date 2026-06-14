#!/usr/bin/env python3
"""fw12tab — floating, draggable on-screen-keyboard toggle button.

A small always-on-top button that appears in tablet mode. Tap it to show/hide
the wvkbd on-screen keyboard; drag it (plain pointer drag) to move it anywhere.
It deliberately never takes keyboard focus (Hyprland `nofocus` rule) so the
keyboard types into whatever app you were using.
"""
import subprocess
import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Gdk", "4.0")
from gi.repository import Gtk, Gdk, Gio  # noqa: E402

APP_ID = "org.fw12.OskButton"  # = Wayland app-id = Hyprland window class

CSS = """
window { background: transparent; }
.fw12-osk {
  background: rgba(30, 30, 46, 0.88);
  border-radius: 18px;
  border: 1px solid rgba(205, 214, 244, 0.25);
}
.fw12-osk label { font-size: 28px; color: #cdd6f4; }
"""

MOVE_THRESHOLD_SQ = 49  # ~7px before a press becomes a drag instead of a tap


class App(Gtk.Application):
    def __init__(self):
        super().__init__(application_id=APP_ID,
                         flags=Gio.ApplicationFlags.DEFAULT_FLAGS)

    def do_activate(self):
        win = Gtk.ApplicationWindow(application=self)
        win.set_decorated(False)
        win.set_resizable(False)
        win.set_default_size(60, 60)
        win.set_title("fw12tab-osk")

        provider = Gtk.CssProvider()
        provider.load_from_string(CSS)
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(), provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

        box = Gtk.Box()
        box.add_css_class("fw12-osk")
        box.set_halign(Gtk.Align.FILL)
        box.set_valign(Gtk.Align.FILL)
        box.set_hexpand(True)
        box.set_vexpand(True)
        label = Gtk.Label(label="⌨")  # keyboard glyph
        label.set_hexpand(True)
        label.set_vexpand(True)
        box.append(label)
        win.set_child(box)

        state = {"moving": False}
        drag = Gtk.GestureDrag()
        drag.set_button(1)

        def on_begin(_g, _x, _y):
            state["moving"] = False

        def on_update(g, off_x, off_y):
            if state["moving"]:
                return
            if off_x * off_x + off_y * off_y < MOVE_THRESHOLD_SQ:
                return
            state["moving"] = True
            surface = win.get_surface()
            ev = g.get_current_event()
            dev = ev.get_device() if ev else None
            ts = ev.get_time() if ev else 0
            _ok, sx, sy = g.get_start_point()
            # Hand the move to the compositor (xdg_toplevel.move) so it can land
            # anywhere on screen. Falls back silently if unsupported.
            if surface is not None and dev is not None and hasattr(surface, "begin_move"):
                try:
                    surface.begin_move(dev, 1, sx, sy, ts)
                except Exception:
                    pass

        def on_end(_g, _ox, _oy):
            if not state["moving"]:  # a tap, not a drag -> toggle the keyboard
                subprocess.Popen(["fw12tab", "osk-toggle"])

        drag.connect("drag-begin", on_begin)
        drag.connect("drag-update", on_update)
        drag.connect("drag-end", on_end)
        win.add_controller(drag)
        win.present()


if __name__ == "__main__":
    App().run(None)
