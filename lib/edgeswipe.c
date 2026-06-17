/* fw12tab edgeswipe — pull down from the top edge to open the touch launcher.
 *
 * A thin, transparent, full-width Wayland layer-shell strip pinned to the TOP
 * edge (the compositor anchors it to the *visual* top in any rotation, so no
 * orientation math is needed). A downward swipe on it runs `fw12tab launch`.
 * Shown only in tablet mode (started/stopped by `fw12tab tablet-watch`), never
 * takes keyboard focus. Sits over the very top of the bar via exclusive_zone=-1.
 *
 * Build: cc -O2 -o edgeswipe edgeswipe.c \
 *          $(pkg-config --cflags --libs gtk4 gtk4-layer-shell-0)
 */
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <stdio.h>

#define STRIP_H 24          /* height of the touch-catching strip, px */
#define PULL_THRESHOLD 60   /* downward travel before it triggers, px */

static gboolean fired;

static void on_drag_begin(GtkGestureDrag *g, double x, double y, gpointer u) {
  (void)g; (void)u;
  fired = FALSE;
  if (g_getenv("FW12TAB_EDGE_DEBUG")) fprintf(stderr, "drag-begin at %.0f,%.0f\n", x, y);
}
static void on_drag_update(GtkGestureDrag *g, double ox, double oy, gpointer u) {
  (void)u;
  double aox = ox < 0 ? -ox : ox;
  if (g_getenv("FW12TAB_EDGE_DEBUG")) fprintf(stderr, "drag-update off %.0f,%.0f\n", ox, oy);
  if (!fired && oy > PULL_THRESHOLD && oy > aox) {   /* a downward pull */
    fired = TRUE;
    g_spawn_command_line_async("fw12tab launch", NULL);
    gtk_gesture_set_state(GTK_GESTURE(g), GTK_EVENT_SEQUENCE_DENIED);
  }
}

static void on_activate(GtkApplication *app, gpointer u) {
  (void)u;
  GtkWidget *win = gtk_application_window_new(app);
  gtk_layer_init_for_window(GTK_WINDOW(win));
  gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
  gtk_layer_set_namespace(GTK_WINDOW(win), "fw12tab-edge");
  gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
  gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);  /* sit at the true top edge */

  gtk_window_set_default_size(GTK_WINDOW(win), 100, STRIP_H);
  GtkWidget *area = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(area, "edge");
  gtk_widget_set_size_request(area, -1, STRIP_H);
  /* a centered drag-handle pill as the visible affordance */
  GtkWidget *handle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(handle, "handle");
  gtk_widget_set_halign(handle, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(handle, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request(handle, 220, 6);
  gtk_widget_set_hexpand(area, TRUE);
  gtk_box_append(GTK_BOX(area), handle);
  gtk_window_set_child(GTK_WINDOW(win), area);

  GtkCssProvider *css = gtk_css_provider_new();
  /* No full-width bar — just a small centered pull handle (which also provides the
   * input region for the swipe). */
  gtk_css_provider_load_from_string(css,
      "window { background: transparent; }"
      ".edge { background: transparent; }"
      ".handle { background: rgba(255,255,255,0.45); border-radius: 9999px; }");
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);

  GtkGesture *drag = gtk_gesture_drag_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), 0);  /* any button/touch */
  g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), NULL);
  g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), NULL);
  gtk_widget_add_controller(win, GTK_EVENT_CONTROLLER(drag));

  gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
  GtkApplication *app =
      gtk_application_new("lu.drotiesel.fw12tab.edge", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  int s = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return s;
}
