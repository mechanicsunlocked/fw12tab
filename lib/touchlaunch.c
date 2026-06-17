/* fw12tab touchlaunch — a finger-friendly application launcher for tablet mode.
 *
 * Omarchy's walker selects on pointer hover + click and ignores raw touch taps
 * (a GTK4/Wayland behaviour we can't fix from outside walker). This is a tiny
 * self-contained replacement built the same way as the fw12tab OSK: big GtkButton
 * tiles whose "clicked" signal fires on a real touch tap — no pointer emulation,
 * no device grab, native touch everywhere else is untouched. A fullscreen scrolled
 * grid lets you flick through apps and tap one to launch it.
 *
 * Build: cc -O2 -o touchlaunch touchlaunch.c \
 *          $(pkg-config --cflags --libs gtk4 gtk4-layer-shell-0)
 */
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>

static GtkWindow *win;

static void quit_launcher(void) { if (win) gtk_window_close(win); }

/* Launch the selected app (detached) and close the launcher. */
static void on_tile_clicked(GtkButton *btn, gpointer data) {
  GAppInfo *info = G_APP_INFO(data);
  GdkAppLaunchContext *ctx =
      gdk_display_get_app_launch_context(gtk_widget_get_display(GTK_WIDGET(btn)));
  GError *err = NULL;
  if (!g_app_info_launch(info, NULL, G_APP_LAUNCH_CONTEXT(ctx), &err)) {
    g_warning("fw12tab touchlaunch: launch failed: %s", err ? err->message : "?");
    g_clear_error(&err);
  }
  g_object_unref(ctx);
  quit_launcher();
}

/* GClosureNotify-typed wrapper so the appinfo ref is dropped with the button. */
static void drop_appinfo(gpointer info, GClosure *c) { (void)c; g_object_unref(info); }

static gint cmp_appinfo(gconstpointer a, gconstpointer b) {
  return g_utf8_collate(g_app_info_get_display_name(G_APP_INFO(a)),
                        g_app_info_get_display_name(G_APP_INFO(b)));
}

static gboolean on_key(GtkEventControllerKey *c, guint keyval, guint kc,
                       GdkModifierType st, gpointer d) {
  (void)c; (void)kc; (void)st; (void)d;
  if (keyval == GDK_KEY_Escape) { quit_launcher(); return TRUE; }
  return FALSE;
}

static GtkWidget *make_tile(GAppInfo *info) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  GIcon *icon = g_app_info_get_icon(info);
  GtkWidget *img = icon ? gtk_image_new_from_gicon(icon)
                        : gtk_image_new_from_icon_name("application-x-executable");
  gtk_image_set_pixel_size(GTK_IMAGE(img), 56);
  GtkWidget *lbl = gtk_label_new(g_app_info_get_display_name(info));
  gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars(GTK_LABEL(lbl), 12);
  gtk_label_set_justify(GTK_LABEL(lbl), GTK_JUSTIFY_CENTER);
  gtk_box_append(GTK_BOX(box), img);
  gtk_box_append(GTK_BOX(box), lbl);

  GtkWidget *btn = gtk_button_new();
  gtk_button_set_child(GTK_BUTTON(btn), box);
  gtk_widget_add_css_class(btn, "tile");
  g_signal_connect_data(btn, "clicked", G_CALLBACK(on_tile_clicked),
                        g_object_ref(info), drop_appinfo, 0);
  return btn;
}

static void activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;
  win = GTK_WINDOW(gtk_application_window_new(app));
  gtk_layer_init_for_window(win);
  gtk_layer_set_layer(win, GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_keyboard_mode(win, GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
  gtk_layer_set_namespace(win, "fw12tab-launch");
  for (int e = 0; e < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; e++)
    gtk_layer_set_anchor(win, e, TRUE);          /* fullscreen */
  gtk_layer_set_exclusive_zone(win, -1);         /* cover the bar too */

  GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(root, "root");

  GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_add_css_class(top, "topbar");
  GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(spacer, TRUE);
  GtkWidget *close = gtk_button_new_with_label("\xe2\x9c\x95"); /* ✕ */
  gtk_widget_add_css_class(close, "close");
  g_signal_connect_swapped(close, "clicked", G_CALLBACK(quit_launcher), NULL);
  gtk_box_append(GTK_BOX(top), spacer);
  gtk_box_append(GTK_BOX(top), close);

  GtkWidget *grid = gtk_flow_box_new();
  gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(grid), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(grid), TRUE);
  gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(grid), 3);
  gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(grid), 8);
  gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(grid), 4);
  gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(grid), 4);

  GList *apps = g_app_info_get_all();
  apps = g_list_sort(apps, cmp_appinfo);
  for (GList *l = apps; l; l = l->next) {
    GAppInfo *info = G_APP_INFO(l->data);
    if (!g_app_info_should_show(info)) continue;
    gtk_flow_box_append(GTK_FLOW_BOX(grid), make_tile(info));
  }
  g_list_free_full(apps, g_object_unref);

  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scroll, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), grid);

  gtk_box_append(GTK_BOX(root), top);
  gtk_box_append(GTK_BOX(root), scroll);
  gtk_window_set_child(win, root);

  GtkEventController *kc = gtk_event_controller_key_new();
  g_signal_connect(kc, "key-pressed", G_CALLBACK(on_key), NULL);
  gtk_widget_add_controller(GTK_WIDGET(win), kc);

  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_string(css,
    ".root { background: rgba(20,20,28,0.96); padding: 16px; }"
    ".topbar { margin-bottom: 12px; }"
    ".close { min-width: 44px; min-height: 44px; font-size: 18px; }"
    ".tile { padding: 12px 6px; border-radius: 12px; background: transparent; }"
    ".tile:hover, .tile:active { background: rgba(255,255,255,0.12); }"
    ".tile label { font-size: 12px; }");
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);

  gtk_window_present(win);
  gtk_window_set_focus(win, NULL);   /* no pre-highlighted tile */
}

int main(int argc, char **argv) {
  GtkApplication *app =
      gtk_application_new("lu.drotiesel.fw12tab.launch", G_APPLICATION_NON_UNIQUE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int s = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return s;
}
