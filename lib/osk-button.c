/* fw12tab — floating, draggable on-screen-keyboard toggle button (C/GTK4).
 *
 * A small always-on-top badge shown in tablet mode. Short tap shows/hides the
 * wvkbd keyboard; drag moves it anywhere. It is a Wayland layer-shell surface
 * on the OVERLAY layer (above the keyboard) that never takes keyboard focus, so
 * the keyboard types into whatever app you were using.
 *
 * Linking gtk4-layer-shell before libwayland (pkg-config order) makes the layer
 * surface initialise correctly with no LD_PRELOAD — unlike the GI/Python path.
 */
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>

#define SIZE 60
#define MARGIN 24
#define MOVE_THRESHOLD_SQ 49      /* ~7px before a press counts as a drag */

static const char *CSS =
  "window { background: transparent; }"
  ".fw12-osk {"
  "  background: #000000;"
  "  border-radius: 9999px;"
  "  border: 2px solid rgba(255,255,255,0.18);"
  "  box-shadow: 0 2px 6px rgba(0,0,0,0.5); }"
  ".fw12-osk label { font-size: 28px; color: #ffffff; }";

typedef struct {
  GtkWindow *win;
  GdkMonitor *mon;
  int x, y, w, h;        /* position + monitor logical size */
  int start_x, start_y;  /* drag anchor */
  gboolean moved;
  char pos_file[512];
} Btn;

static const char *icon_path(void) {
  const char *e = g_getenv("FW12TAB_OSK_ICON");
  return (e && *e) ? e : "/usr/share/fw12tab/framework-logo.svg";
}

static void clamp_and_place(Btn *b) {
  if (b->x < 0) b->x = 0;
  if (b->y < 0) b->y = 0;
  if (b->x > b->w - SIZE) b->x = b->w - SIZE > 0 ? b->w - SIZE : 0;
  if (b->y > b->h - SIZE) b->y = b->h - SIZE > 0 ? b->h - SIZE : 0;
  gtk_layer_set_margin(b->win, GTK_LAYER_SHELL_EDGE_LEFT, b->x);
  gtk_layer_set_margin(b->win, GTK_LAYER_SHELL_EDGE_TOP, b->y);
}

/* Re-read monitor geometry (rotation swaps width/height) and re-clamp. */
static void reflow(Btn *b) {
  if (b->mon) {
    GdkRectangle g;
    gdk_monitor_get_geometry(b->mon, &g);
    if (g.width > 0 && g.height > 0) { b->w = g.width; b->h = g.height; }
  }
  clamp_and_place(b);
}
static void on_geometry(GObject *o, GParamSpec *p, gpointer u) {
  (void)o; (void)p; reflow((Btn *)u);
}

static void load_pos(Btn *b) {
  b->x = b->w - SIZE - MARGIN;          /* default: top-right */
  b->y = MARGIN * 2;
  FILE *f = fopen(b->pos_file, "r");
  if (f) { int x, y; if (fscanf(f, "%d %d", &x, &y) == 2) { b->x = x; b->y = y; } fclose(f); }
}
static void save_pos(Btn *b) {
  FILE *f = fopen(b->pos_file, "w");
  if (f) { fprintf(f, "%d %d", b->x, b->y); fclose(f); }
}

static void on_drag_begin(GtkGestureDrag *g, double sx, double sy, gpointer u) {
  (void)g; (void)sx; (void)sy;
  Btn *b = u; b->start_x = b->x; b->start_y = b->y; b->moved = FALSE;
}
static void on_drag_update(GtkGestureDrag *g, double ox, double oy, gpointer u) {
  (void)g; Btn *b = u;
  if (!b->moved && ox * ox + oy * oy < MOVE_THRESHOLD_SQ) return;
  b->moved = TRUE;
  b->x = b->start_x + (int)ox;
  b->y = b->start_y + (int)oy;
  clamp_and_place(b);
}
static void on_drag_end(GtkGestureDrag *g, double ox, double oy, gpointer u) {
  (void)g; (void)ox; (void)oy; Btn *b = u;
  if (b->moved) save_pos(b);
}
static void on_released(GtkGestureClick *g, int n, double x, double y, gpointer u) {
  (void)g; (void)n; (void)x; (void)y; Btn *b = u;
  if (!b->moved) g_spawn_command_line_async("fw12tab osk-toggle", NULL);
}

static void on_activate(GtkApplication *app, gpointer u) {
  (void)u;
  Btn *b = g_new0(Btn, 1);
  const char *rt = g_getenv("XDG_RUNTIME_DIR");
  g_snprintf(b->pos_file, sizeof b->pos_file, "%s/fw12tab-osk-pos", rt ? rt : "/tmp");

  GtkWidget *win = gtk_application_window_new(app);
  b->win = GTK_WINDOW(win);
  gtk_window_set_decorated(b->win, FALSE);
  gtk_window_set_resizable(b->win, FALSE);
  gtk_window_set_default_size(b->win, SIZE, SIZE);

  gtk_layer_init_for_window(b->win);
  gtk_layer_set_layer(b->win, GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_keyboard_mode(b->win, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
  gtk_layer_set_namespace(b->win, "fw12tab-osk");
  gtk_layer_set_anchor(b->win, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(b->win, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);

  b->w = 1280; b->h = 800;
  GdkDisplay *disp = gdk_display_get_default();
  GListModel *mons = disp ? gdk_display_get_monitors(disp) : NULL;
  b->mon = mons ? g_list_model_get_item(mons, 0) : NULL;
  reflow(b);                         /* seed w/h from geometry */
  load_pos(b);
  reflow(b);                         /* apply loaded/default position */
  if (b->mon) {
    g_signal_connect(b->mon, "notify::geometry", G_CALLBACK(on_geometry), b);
    g_object_unref(b->mon);   /* display owns the monitor; drop our get_item ref. b->mon
                                 stays valid (borrowed) for the internal panel's lifetime. */
  }

  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_string(css, CSS);
  gtk_style_context_add_provider_for_display(
    gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(box, "fw12-osk");
  gtk_widget_set_hexpand(box, TRUE);
  gtk_widget_set_vexpand(box, TRUE);

  GtkWidget *child;
  if (g_file_test(icon_path(), G_FILE_TEST_EXISTS)) {
    child = gtk_picture_new_for_filename(icon_path());
    gtk_picture_set_content_fit(GTK_PICTURE(child), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_size_request(child, 34, 34);
    gtk_widget_set_halign(child, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(child, GTK_ALIGN_CENTER);
  } else {
    child = gtk_label_new("⌨");   /* keyboard glyph fallback */
  }
  gtk_widget_set_hexpand(child, TRUE);
  gtk_widget_set_vexpand(child, TRUE);
  gtk_box_append(GTK_BOX(box), child);
  gtk_window_set_child(b->win, box);

  GtkGesture *drag = gtk_gesture_drag_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), 0);
  g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), b);
  g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), b);
  g_signal_connect(drag, "drag-end", G_CALLBACK(on_drag_end), b);
  gtk_widget_add_controller(win, GTK_EVENT_CONTROLLER(drag));

  GtkGesture *click = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
  g_signal_connect(click, "released", G_CALLBACK(on_released), b);
  gtk_widget_add_controller(win, GTK_EVENT_CONTROLLER(click));

  gtk_window_present(b->win);
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("org.fw12.OskButton",
                                            G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
