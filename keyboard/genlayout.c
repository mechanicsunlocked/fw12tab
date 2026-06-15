/* fw12tab — patch a wvkbd-mobintl source tree with a "system" layer that
 * matches the user's ACTUAL keyboard layout. Self-contained C (libxkbcommon):
 * no Python, no xkbcli subprocess.
 *
 * One fixed keycode grid works for every layout because the labels are resolved
 * from the compiled xkb keymap: US prints QWERTY, German QWERTZ + umlauts,
 * French AZERTY — no per-layout profiles. Frame keys (Esc, Ctrl, Alt, Super,
 * arrows, Shift, Backspace, Enter, ...) are fixed so Hyprland stays drivable
 * from the keyboard (Super+1..0, Super+W).
 *
 * Usage: genlayout <wvkbd-src-dir> <layout> [variant] [options]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

/* Physical ISO 105-key main block (positions; labels resolved from the keymap).
 * R1: ^ 1..0 ß ´     R2: q..p ü +     R3: a..l ö ä #     R4: < y..m , . - */
static const int row_num[]  = {KEY_GRAVE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,
                               KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL};
static const int row_top[]  = {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U,
                               KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE};
static const int row_home[] = {KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J,
                               KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_BACKSLASH};
static const int row_bot[]  = {KEY_102ND, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B,
                               KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH};

static struct xkb_keymap *KM;

/* ---- small string helpers (build dynamic strings) ---- */
static char *xstrdup(const char *s) { char *p = malloc(strlen(s) + 1); strcpy(p, s); return p; }
static char *appendf(char *buf, const char *fmt, ...) {
  char tmp[512]; va_list ap; va_start(ap, fmt); vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
  size_t a = buf ? strlen(buf) : 0, b = strlen(tmp);
  buf = realloc(buf, a + b + 1); memcpy(buf + a, tmp, b + 1); return buf;
}

static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) { perror(path); exit(1); }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  char *b = malloc(n + 1); if (fread(b, 1, n, f) != (size_t)n) { perror(path); exit(1); }
  b[n] = '\0'; fclose(f); return b;
}
static void write_file(const char *path, const char *data) {
  FILE *f = fopen(path, "wb"); if (!f) { perror(path); exit(1); }
  fputs(data, f); fclose(f);
}

/* Replace the first occurrence of needle with repl; abort if missing. */
static char *replace_once(char *hay, const char *needle, const char *repl) {
  char *at = strstr(hay, needle);
  if (!at) { fprintf(stderr, "genlayout: anchor not found: %.40s\n", needle); exit(1); }
  size_t pre = at - hay, nl = strlen(needle), rl = strlen(repl), tail = strlen(at + nl);
  char *out = malloc(pre + rl + tail + 1);
  memcpy(out, hay, pre); memcpy(out + pre, repl, rl); memcpy(out + pre + rl, at + nl, tail + 1);
  free(hay); return out;
}
/* Find the LAST occurrence of needle. */
static char *rstrstr(char *hay, const char *needle) {
  char *r = NULL, *p = hay;
  while ((p = strstr(p, needle))) { r = p; p++; }
  return r;
}

/* Stream text to f as adjacent C string literals "line\n" (one per line).
 * Streaming (not building one giant string) keeps this O(n) and avoids any
 * fixed-buffer truncation on the large keymap. */
static void fput_escaped(FILE *f, const char *t) {
  fputc('"', f);
  for (; *t; t++) {
    unsigned char c = *t;
    if (c == '\n') { fputs("\\n\"\n\"", f); continue; }
    if (c == '"' || c == '\\') fputc('\\', f);
    fputc(c, f);
  }
  fputs("\\n\"", f);
}

/* Resolve UTF-8 for a scancode at a shift level into out (label). */
static void label_at(int scancode, int level, char *out, size_t n) {
  const xkb_keysym_t *syms; out[0] = '\0';
  int ns = xkb_keymap_key_get_syms_by_level(KM, scancode + 8, 0, level, &syms);
  if (ns < 1) return;
  if (xkb_keysym_to_utf8(syms[0], out, n) > 0 && (unsigned char)out[0] >= 0x20) return;
  switch (syms[0]) {                                  /* dead keys -> accent glyph */
    case XKB_KEY_dead_acute:      snprintf(out, n, "´"); return;
    case XKB_KEY_dead_grave:      snprintf(out, n, "`");      return;
    case XKB_KEY_dead_circumflex: snprintf(out, n, "^");      return;
    case XKB_KEY_dead_tilde:      snprintf(out, n, "~");      return;
    case XKB_KEY_dead_diaeresis:  snprintf(out, n, "¨"); return;
    case XKB_KEY_dead_cedilla:    snprintf(out, n, "¸"); return;
    case XKB_KEY_dead_caron:      snprintf(out, n, "ˇ"); return;
    case XKB_KEY_dead_breve:      snprintf(out, n, "˘"); return;
    case XKB_KEY_dead_abovering:  snprintf(out, n, "°"); return;
  }
  out[0] = '\0';
}

static char *emit_key(char *buf, int scancode) {
  char base[16], shift[16];
  label_at(scancode, 0, base, sizeof base);
  label_at(scancode, 1, shift, sizeof shift);
  if (!base[0]) return buf;                            /* no symbol (e.g. ANSI 102nd) */
  if (!shift[0]) strcpy(shift, base);
  char eb[40] = "", es[40] = "";                       /* escape for C literal */
  for (char *s = base, *d = eb; *s; s++) { if (*s=='"'||*s=='\\') *d++='\\'; *d++=*s; *d=0; }
  for (char *s = shift, *d = es; *s; s++) { if (*s=='"'||*s=='\\') *d++='\\'; *d++=*s; *d=0; }
  return appendf(buf, "  {\"%s\", \"%s\", 1.0, Code, %d},\n", eb, es, scancode);
}
static char *emit_row(char *buf, const int *keys, int n) {
  for (int i = 0; i < n; i++) buf = emit_key(buf, keys[i]);
  return appendf(buf, "  {\"\", \"\", 0.0, EndRow},\n");
}

/* Build a physical-ISO-faithful layer. Modifier keys (Ctrl/Super/Alt/AltGr)
 * sit in the BOTTOM row exactly like a real keyboard — Super is NOT in row 1.
 * A slim top row adds Esc / layer-switch / arrows (touch convenience). */
static char *build_keys_array(void) {
  char *b = xstrdup("static struct key keys_system[] = {\n");
  /* top utility strip */
  b = appendf(b, "  {\"Esc\", \"Esc\", 1.0, Code, KEY_ESC, .scheme = 1},\n");
  b = appendf(b, "  {\"⌨͕\", \"⌨͔\", 1.0, NextLayer, .scheme = 1},\n");
  b = appendf(b, "  {\"Cmp\", \"Cmp\", 1.0, Compose, .scheme = 1},\n");
  b = appendf(b, "  {\"←\", \"←\", 1.0, Code, KEY_LEFT, .scheme = 1},\n");
  b = appendf(b, "  {\"↓\", \"↓\", 1.0, Code, KEY_DOWN, .scheme = 1},\n");
  b = appendf(b, "  {\"↑\", \"↑\", 1.0, Code, KEY_UP, .scheme = 1},\n");
  b = appendf(b, "  {\"→\", \"→\", 1.0, Code, KEY_RIGHT, .scheme = 1},\n");
  b = appendf(b, "  {\"\", \"\", 0.0, EndRow},\n");
  /* R1 number row + Backspace */
  for (unsigned i = 0; i < sizeof row_num / sizeof *row_num; i++) b = emit_key(b, row_num[i]);
  b = appendf(b, "  {\"⌫\", \"⌫\", 2.0, Code, KEY_BACKSPACE, .scheme = 1},\n");
  b = appendf(b, "  {\"\", \"\", 0.0, EndRow},\n");
  /* R2 Tab + qwertz */
  b = appendf(b, "  {\"Tab\", \"Tab\", 1.5, Code, KEY_TAB, .scheme = 1},\n");
  for (unsigned i = 0; i < sizeof row_top / sizeof *row_top; i++) b = emit_key(b, row_top[i]);
  b = appendf(b, "  {\"\", \"\", 0.0, EndRow},\n");
  /* R3 Caps + home row + Enter */
  b = appendf(b, "  {\"⇪\", \"⇪\", 1.75, Code, KEY_CAPSLOCK, .scheme = 1},\n");
  for (unsigned i = 0; i < sizeof row_home / sizeof *row_home; i++) b = emit_key(b, row_home[i]);
  b = appendf(b, "  {\"⏎\", \"⏎\", 1.75, Code, KEY_ENTER, .scheme = 1},\n");
  b = appendf(b, "  {\"\", \"\", 0.0, EndRow},\n");
  /* R4 Shift + < + zxc... + Shift */
  b = appendf(b, "  {\"⇧\", \"⇫\", 1.25, Mod, Shift, .scheme = 1},\n");
  for (unsigned i = 0; i < sizeof row_bot / sizeof *row_bot; i++) b = emit_key(b, row_bot[i]);
  b = appendf(b, "  {\"⇧\", \"⇫\", 1.75, Mod, Shift, .scheme = 1},\n");
  b = appendf(b, "  {\"\", \"\", 0.0, EndRow},\n");
  /* R5 modifier row — physical ISO order: Ctrl Super Alt Space AltGr Ctrl */
  b = appendf(b, "  {\"Ctrl\", \"Ctrl\", 1.5, Mod, Ctrl, .scheme = 1},\n");
  b = appendf(b, "  {\"Super\", \"Super\", 1.25, Mod, Super, .scheme = 1},\n");
  b = appendf(b, "  {\"Alt\", \"Alt\", 1.25, Mod, Alt, .scheme = 1},\n");
  b = appendf(b, "  {\"\", \"\", 5.0, Code, KEY_SPACE},\n");
  b = appendf(b, "  {\"AltGr\", \"AltGr\", 1.25, Mod, AltGr, .scheme = 1},\n");
  b = appendf(b, "  {\"Ctrl\", \"Ctrl\", 1.5, Mod, Ctrl, .scheme = 1},\n");
  b = appendf(b, "  {\"\", \"\", 0.0, Last},\n};\n");
  return b;
}

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: %s <wvkbd-src> <layout> [variant] [options]\n", argv[0]); return 2; }
  const char *src = argv[1];
  struct xkb_rule_names names = {
    .rules = NULL, .model = "pc105", .layout = argv[2],
    .variant = (argc > 3 && argv[3][0]) ? argv[3] : NULL,
    .options = (argc > 4 && argv[4][0]) ? argv[4] : NULL,
  };
  struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  KM = ctx ? xkb_keymap_new_from_names(ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
  if (!KM) { fprintf(stderr, "genlayout: cannot compile keymap for '%s'\n", argv[2]); return 1; }

  char *keymap_text = xkb_keymap_get_as_string(KM, XKB_KEYMAP_FORMAT_TEXT_V1);
  char *keys = build_keys_array();

  /* ---- patch keymap.mobintl.h: add the "system" keymap ---- */
  char p1[512]; snprintf(p1, sizeof p1, "%s/keymap.mobintl.h", src);
  char *km = read_file(p1);
  if (!strstr(km, "\"system\"")) {
    km = replace_once(km, "#define NUMKEYMAPS 7", "#define NUMKEYMAPS 8");
    km = replace_once(km, "\"persian\", \"hebrew\" };", "\"persian\", \"hebrew\", \"system\" };");
    char *end = rstrstr(km, "};};\"\n};");
    if (!end) { fprintf(stderr, "genlayout: keymaps[] terminator not found\n"); return 1; }
    *end = '\0';                                /* km now = everything before the array close */
    FILE *f = fopen(p1, "wb"); if (!f) { perror(p1); return 1; }
    fputs(km, f);
    fputs("};};\",\n", f);                       /* close the previous keymap element (+comma) */
    fput_escaped(f, keymap_text);                /* stream the new "system" keymap */
    fputs("\n};\n", f);                          /* close the keymaps[] array */
    fclose(f);
  }

  /* ---- patch layout.mobintl.h: add the "system" layer ---- */
  char p2[512]; snprintf(p2, sizeof p2, "%s/layout.mobintl.h", src);
  char *lo = read_file(p2);
  if (!strstr(lo, "keys_system")) {
    lo = replace_once(lo, "\tLandscapeSpecial,\n", "\tLandscapeSpecial,\n\tSystem,\n");
    lo = replace_once(lo,
      "[FullWide] = {keys_full_wide, \"latin\", \"fullwide\", true},",
      "[FullWide] = {keys_full_wide, \"latin\", \"fullwide\", true},\n"
      "  [System] = {keys_system, \"system\", \"system\", true},");
    lo = replace_once(lo, "static struct layout layouts[NumLayouts] = {",
      "static struct key keys_system[];\nstatic struct layout layouts[NumLayouts] = {");
    char *ins = xstrdup(keys);
    ins = appendf(ins, "\nstatic struct key keys_special[] = {");
    lo = replace_once(lo, "static struct key keys_special[] = {", ins);
    free(ins);
    write_file(p2, lo);
  }

  /* ---- patch main.c: release latched modifiers on SIGTERM/SIGINT ----
   * Otherwise `pkill wvkbd` while e.g. Super is latched leaves it stuck in the
   * compositor, turning mouse clicks into window-move (Super+drag). Linux key
   * codes: LSHIFT 42, LCTRL 29, LALT 56, LMETA 125, RALT 100. */
  char p3[512]; snprintf(p3, sizeof p3, "%s/main.c", src);
  char *mc = read_file(p3);
  if (!strstr(mc, "ssi_signo == SIGTERM")) {
    mc = replace_once(mc, "sigaddset(&signal_mask, SIGPIPE);",
      "sigaddset(&signal_mask, SIGPIPE);\n"
      "    sigaddset(&signal_mask, SIGTERM);\n"
      "    sigaddset(&signal_mask, SIGINT);");
    mc = replace_once(mc,
      "else if (si.ssi_signo == SIGPIPE)\n                pipewarn();",
      "else if (si.ssi_signo == SIGPIPE)\n                pipewarn();\n"
      "            else if (si.ssi_signo == SIGTERM || si.ssi_signo == SIGINT) {\n"
      "                if (keyboard.mods & Shift) zwp_virtual_keyboard_v1_key(keyboard.vkbd, 0, 42, WL_KEYBOARD_KEY_STATE_RELEASED);\n"
      "                if (keyboard.mods & Ctrl)  zwp_virtual_keyboard_v1_key(keyboard.vkbd, 0, 29, WL_KEYBOARD_KEY_STATE_RELEASED);\n"
      "                if (keyboard.mods & Alt)   zwp_virtual_keyboard_v1_key(keyboard.vkbd, 0, 56, WL_KEYBOARD_KEY_STATE_RELEASED);\n"
      "                if (keyboard.mods & Super) zwp_virtual_keyboard_v1_key(keyboard.vkbd, 0, 125, WL_KEYBOARD_KEY_STATE_RELEASED);\n"
      "                if (keyboard.mods & AltGr) zwp_virtual_keyboard_v1_key(keyboard.vkbd, 0, 100, WL_KEYBOARD_KEY_STATE_RELEASED);\n"
      "                keyboard.mods = 0; wl_display_flush(display); run_display = false;\n"
      "            }");
    write_file(p3, mc);
  }

  fprintf(stderr, "genlayout: patched wvkbd with 'system' layer for layout '%s'\n", argv[2]);
  return 0;
}
