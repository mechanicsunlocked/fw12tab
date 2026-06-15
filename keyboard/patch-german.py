#!/usr/bin/env python3
"""Inject a German (QWERTZ) layer into a wvkbd-mobintl source tree.

Adds:
  * an xkb "german" keymap (generated with `xkbcli compile-keymap --layout de`)
  * a "deutsch" layer: QWERTZ with umlauts (ä ö ü ß), a number row, and — so
    Hyprland can be driven from the keyboard — visible Super and Alt keys.

The German keymap does the real character translation, so the y/z swap and the
umlauts are just relabelled keys sitting on the right evdev keycodes
(<AD06>→z, <AB01>→y, <AD11>→ü, <AC10>→ö, <AC11>→ä, <AE11>→ß ...).

Usage: patch-german.py <wvkbd-src-dir> <de.xkb keymap file>
Idempotent: refuses to patch a tree that already contains the layer.
"""
import sys

SRC, KEYMAP = sys.argv[1], sys.argv[2]

# --- the German layer ------------------------------------------------------
KEYS_GERMAN = r'''
static struct key keys_german[] = {
  {"Esc", "Esc", 1.0, Code, KEY_ESC, .scheme = 1},
  {"Ctr", "Ctr", 1.0, Mod, Ctrl, .scheme = 1},
  {"Alt", "Alt", 1.0, Mod, Alt, .scheme = 1},
  {"Sup", "Sup", 1.0, Mod, Super, .scheme = 1},
  {"↑", "↑", 1.0, Code, KEY_UP, .scheme = 1},
  {"↓", "↓", 1.0, Code, KEY_DOWN, .scheme = 1},
  {"←", "←", 1.0, Code, KEY_LEFT, .scheme = 1},
  {"→", "→", 1.0, Code, KEY_RIGHT, .scheme = 1},
  {"Tab", "Tab", 1.0, Code, KEY_TAB, .scheme = 1},
  {"", "", 0.0, EndRow},

  {"1", "!", 1.0, Code, KEY_1},
  {"2", "\"", 1.0, Code, KEY_2},
  {"3", "§", 1.0, Code, KEY_3},
  {"4", "$", 1.0, Code, KEY_4},
  {"5", "%", 1.0, Code, KEY_5},
  {"6", "&", 1.0, Code, KEY_6},
  {"7", "/", 1.0, Code, KEY_7},
  {"8", "(", 1.0, Code, KEY_8},
  {"9", ")", 1.0, Code, KEY_9},
  {"0", "=", 1.0, Code, KEY_0},
  {"ß", "?", 1.0, Code, KEY_MINUS},
  {"´", "`", 1.0, Code, KEY_EQUAL},
  {"", "", 0.0, EndRow},

  {"q", "Q", 1.0, Code, KEY_Q},
  {"w", "W", 1.0, Code, KEY_W},
  {"e", "E", 1.0, Code, KEY_E},
  {"r", "R", 1.0, Code, KEY_R},
  {"t", "T", 1.0, Code, KEY_T},
  {"z", "Z", 1.0, Code, KEY_Y},
  {"u", "U", 1.0, Code, KEY_U},
  {"i", "I", 1.0, Code, KEY_I},
  {"o", "O", 1.0, Code, KEY_O},
  {"p", "P", 1.0, Code, KEY_P},
  {"ü", "Ü", 1.0, Code, KEY_LEFTBRACE},
  {"+", "*", 1.0, Code, KEY_RIGHTBRACE},
  {"", "", 0.0, EndRow},

  {"a", "A", 1.0, Code, KEY_A},
  {"s", "S", 1.0, Code, KEY_S},
  {"d", "D", 1.0, Code, KEY_D},
  {"f", "F", 1.0, Code, KEY_F},
  {"g", "G", 1.0, Code, KEY_G},
  {"h", "H", 1.0, Code, KEY_H},
  {"j", "J", 1.0, Code, KEY_J},
  {"k", "K", 1.0, Code, KEY_K},
  {"l", "L", 1.0, Code, KEY_L},
  {"ö", "Ö", 1.0, Code, KEY_SEMICOLON},
  {"ä", "Ä", 1.0, Code, KEY_APOSTROPHE},
  {"#", "'", 1.0, Code, KEY_BACKSLASH},
  {"", "", 0.0, EndRow},

  {"⇧", "⇫", 1.5, Mod, Shift, .scheme = 1},
  {"y", "Y", 1.0, Code, KEY_Z},
  {"x", "X", 1.0, Code, KEY_X},
  {"c", "C", 1.0, Code, KEY_C},
  {"v", "V", 1.0, Code, KEY_V},
  {"b", "B", 1.0, Code, KEY_B},
  {"n", "N", 1.0, Code, KEY_N},
  {"m", "M", 1.0, Code, KEY_M},
  {",", ";", 1.0, Code, KEY_COMMA},
  {".", ":", 1.0, Code, KEY_DOT},
  {"-", "_", 1.0, Code, KEY_SLASH},
  {"⌫", "⌫", 1.5, Code, KEY_BACKSPACE, .scheme = 1},
  {"", "", 0.0, EndRow},

  {"⌨͕", "⌨͔", 1.5, NextLayer, .scheme = 1},
  {"Cmp", "Cmp", 1.0, Compose, .scheme = 1},
  {"AltGr", "AltGr", 1.0, Mod, AltGr, .scheme = 1},
  {"", "Tab", 4.0, Code, KEY_SPACE},
  {"Sup", "Sup", 1.0, Mod, Super, .scheme = 1},
  {"Enter", "Enter", 1.5, Code, KEY_ENTER, .scheme = 1},

  {"", "", 0.0, Last},
};
'''


def c_string(text):
    out = []
    for line in text.split("\n"):
        line = line.replace("\\", "\\\\").replace('"', '\\"')
        out.append('"%s\\n"' % line)
    return "\n".join(out)


# --- patch keymap.mobintl.h -------------------------------------------------
km_path = SRC + "/keymap.mobintl.h"
km = open(km_path, encoding="utf-8").read()
if '"german"' in km:
    print("keymap already patched; skipping")
    sys.exit(0)
km = km.replace("#define NUMKEYMAPS 7", "#define NUMKEYMAPS 8", 1)
km = km.replace('"persian", "hebrew" };', '"persian", "hebrew", "german" };', 1)
german_km = c_string(open(KEYMAP, encoding="utf-8").read())
end = '};};"\n};'                        # end of last element + array close
idx = km.rfind(end)
assert idx != -1, "could not find keymaps[] array terminator"
km = km[:idx] + '};};",\n' + german_km + "\n};\n"
open(km_path, "w", encoding="utf-8").write(km)

# --- patch layout.mobintl.h -------------------------------------------------
lo_path = SRC + "/layout.mobintl.h"
lo = open(lo_path, encoding="utf-8").read()
if "keys_german" in lo:
    print("layout already patched; skipping")
    sys.exit(0)
lo = lo.replace("\tLandscapeSpecial,\n",
                "\tLandscapeSpecial,\n\tDeutsch,\n", 1)
lo = lo.replace(
    '[FullWide] = {keys_full_wide, "latin", "fullwide", true},',
    '[FullWide] = {keys_full_wide, "latin", "fullwide", true},\n'
    '  [Deutsch] = {keys_german, "german", "deutsch", true},', 1)
lo = lo.replace("static struct layout layouts[NumLayouts] = {",
                "static struct key keys_german[];\n"
                "static struct layout layouts[NumLayouts] = {", 1)
lo = lo.replace("static struct key keys_special[] = {",
                KEYS_GERMAN + "\nstatic struct key keys_special[] = {", 1)
open(lo_path, "w", encoding="utf-8").write(lo)

print("patched: added 'german' keymap and 'deutsch' layer")
