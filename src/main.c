#include <gtk/gtk.h>
#include <cairo.h>
#include <stdbool.h>
#include <stdio.h>

#include "app_state.h"
#include "config.h"
#include "entity.h"
#include "sidebar.h"
#include "species.h"
#include "world.h"

static void on_background_error(GObject *obj, GParamSpec *pspec, gpointer user_data) {
    (void) pspec; (void) user_data;
    GtkMediaStream *stream = GTK_MEDIA_STREAM(obj);
    const GError *err = gtk_media_stream_get_error(stream);
    if (err != 0) g_warning("Background video error: %s", err->message);
}

static gboolean tick_cb(gpointer data) {
    AppState *app = (AppState *) data;

    int w = gtk_widget_get_width(app->container);
    int h = gtk_widget_get_height(app->container);
    if (w > 0 && h > 0) {
        app->world.width  = (double) w;
        app->world.height = (double) h;
    }

    world_tick(&app->world);

    char buf[16];
    for (int s = 0; s < species_count(); s++) {
        if (app->dash_labels[s]) {
            snprintf(buf, sizeof(buf), "%d", app->world.alive_counts[s]);
            gtk_label_set_text(GTK_LABEL(app->dash_labels[s]), buf);
        }
    }

    if (app->debug_mode && app->debug_overlay != 0)
        gtk_widget_queue_draw(app->debug_overlay);
    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ debug */
static void debug_draw_cb(GtkDrawingArea *area, cairo_t *cr,
                           int width, int height, gpointer user_data) {
    (void) area; (void) width; (void) height;
    AppState *app = (AppState *) user_data;
    if (app == 0 || !app->debug_mode) return;

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 0.20, 0.92, 0.95, 0.95);
    cairo_rectangle(cr, 0.5, 0.5, app->world.width - 1.0, app->world.height - 1.0);
    cairo_stroke(cr);

    for (int i = 0; i < app->world.entity_count; i++) {
        Entity *e = &app->world.entities[i];
        if (e->state == ENTITY_INACTIVE) continue;

        double bx = 0, by = 0, bw = 0, bh = 0;
        if (!entity_bounds_rect(&app->world, e, &bx, &by, &bw, &bh)) continue;

        cairo_set_source_rgba(cr, 0.98, 0.84, 0.15, 0.88);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, 0.95, 0.95, 0.95, 0.95);
        cairo_arc(cr, e->pos.x, e->pos.y, 2.0, 0.0, 6.28318530718);
        cairo_fill(cr);

        cairo_set_source_rgba(cr, 0.20, 0.95, 0.33, 0.95);
        cairo_move_to(cr, e->pos.x, e->pos.y);
        cairo_line_to(cr, e->pos.x + e->vel.x * 0.28, e->pos.y + e->vel.y * 0.28);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, 1.0, 0.35, 0.35, 0.95);
        cairo_move_to(cr, e->pos.x, e->pos.y);
        cairo_line_to(cr, e->pos.x + e->acc.x * 42.0, e->pos.y + e->acc.y * 42.0);
        cairo_stroke(cr);
    }
}

/* ------------------------------------------------------------------ keys */
static void toggle_fullscreen(AppState *app) {
    if (app->fullscreen) {
        gtk_window_unfullscreen(GTK_WINDOW(app->window));
        app->fullscreen = false;
    } else {
        gtk_window_fullscreen(GTK_WINDOW(app->window));
        app->fullscreen = true;
    }
}

static gboolean on_key_pressed(GtkEventControllerKey *controller,
                                guint keyval, guint keycode,
                                GdkModifierType state, gpointer user_data) {
    (void) controller; (void) keycode; (void) state;
    AppState *app = (AppState *) user_data;

    if (keyval == GDK_KEY_F11) {
        toggle_fullscreen(app); return TRUE;
    }
    if (keyval == GDK_KEY_Escape && app->fullscreen) {
        toggle_fullscreen(app); return TRUE;
    }
    if (keyval == GDK_KEY_F3 || keyval == GDK_KEY_d || keyval == GDK_KEY_D) {
        app->debug_mode = !app->debug_mode;
        app->world.debug_mode = app->debug_mode;
        if (app->debug_overlay) {
            gtk_widget_set_visible(app->debug_overlay, app->debug_mode);
            gtk_widget_queue_draw(app->debug_overlay);
        }
        return TRUE;
    }
    if (keyval == GDK_KEY_Tab || keyval == GDK_KEY_F2) {
        bool now = !gtk_widget_get_visible(app->sidebar);
        gtk_widget_set_visible(app->sidebar, now);
        return TRUE;
    }
    return FALSE;
}

/* ------------------------------------------------------------------ UI Builders */

/* --- Bottom Dashboard --- */
static void build_dashboard(AppState *app) {
    app->dashboard = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(app->dashboard, "dashboard");
    gtk_widget_set_margin_top   (app->dashboard, 8);
    gtk_widget_set_margin_bottom(app->dashboard, 8);
    gtk_widget_set_margin_start (app->dashboard, 16);
    gtk_widget_set_margin_end   (app->dashboard, 16);

    for (int s = 0; s < species_count(); s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (!cfg) continue;

        GtkWidget *item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_valign(item, GTK_ALIGN_CENTER);

        GtkWidget *img = gtk_picture_new_for_filename(cfg->asset_path);
        gtk_widget_add_css_class(img, "dash-icon");
        gtk_picture_set_can_shrink(GTK_PICTURE(img), TRUE);
        gtk_widget_set_size_request(img, 48, 32);
        gtk_box_append(GTK_BOX(item), img);

        app->dash_labels[s] = gtk_label_new("0");
        gtk_widget_add_css_class(app->dash_labels[s], "dash-count");
        gtk_box_append(GTK_BOX(item), app->dash_labels[s]);

        gtk_box_append(GTK_BOX(app->dashboard), item);
    }
}

/* ------------------------------------------------------------------ activate */

static void on_activate(GtkApplication *gtk_app, gpointer user_data) {
    (void) user_data;

    AppState *app = g_new0(AppState, 1);
    app->fullscreen = START_FULLSCREEN != 0;

    species_load_defaults();
    if (!species_load_toml("config/aquarium.toml")) {
        g_warning("Could not load config/aquarium.toml; using built-in species defaults");
    }

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "GTK Aquarium");
    gtk_window_set_default_size(GTK_WINDOW(app->window), START_WIDTH, START_HEIGHT);

    app->root_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(app->window), app->root_box);

    app->main_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(app->main_area, TRUE);
    gtk_widget_set_vexpand(app->main_area, TRUE);

    app->overlay = gtk_overlay_new();
    gtk_widget_set_hexpand(app->overlay, TRUE);
    gtk_widget_set_vexpand(app->overlay, TRUE);
    gtk_box_append(GTK_BOX(app->main_area), app->overlay);
    app->debug_mode = false;

    /* Initialize World BEFORE building UI so target_counts are populated */
    app->container = gtk_fixed_new();
    gtk_widget_add_css_class(app->container, "fish-layer");
    world_init(&app->world, app->container, START_WIDTH, START_HEIGHT);

    sidebar_build(app);
    gtk_box_append(GTK_BOX(app->root_box), app->main_area);
    gtk_box_append(GTK_BOX(app->root_box), app->sidebar);

    build_dashboard(app);
    gtk_box_append(GTK_BOX(app->main_area), app->dashboard);

    /* Background video */
    app->background_stream = gtk_media_file_new_for_filename("assets/background-underwater.webm");
    gtk_media_stream_set_loop   (GTK_MEDIA_STREAM(app->background_stream), TRUE);
    gtk_media_stream_set_muted  (GTK_MEDIA_STREAM(app->background_stream), TRUE);
    gtk_media_stream_set_playing(GTK_MEDIA_STREAM(app->background_stream), TRUE);
    g_signal_connect(app->background_stream, "notify::error", G_CALLBACK(on_background_error), 0);

    app->background_picture = gtk_picture_new_for_paintable(GDK_PAINTABLE(app->background_stream));
    gtk_picture_set_can_shrink  (GTK_PICTURE(app->background_picture), TRUE);
    gtk_picture_set_content_fit (GTK_PICTURE(app->background_picture), GTK_CONTENT_FIT_COVER);
    gtk_widget_set_hexpand(app->background_picture, TRUE);
    gtk_widget_set_vexpand(app->background_picture, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(app->overlay), app->background_picture);

    /* Container */
    gtk_widget_set_hexpand(app->container, TRUE);
    gtk_widget_set_vexpand(app->container, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(app->overlay), app->container);
    gtk_widget_set_can_target(app->container, FALSE);

    /* Debug overlay */
    app->debug_overlay = gtk_drawing_area_new();
    gtk_widget_set_hexpand(app->debug_overlay, TRUE);
    gtk_widget_set_vexpand(app->debug_overlay, TRUE);
    gtk_widget_set_can_target(app->debug_overlay, FALSE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(app->debug_overlay), debug_draw_cb, app, 0);
    gtk_overlay_add_overlay(GTK_OVERLAY(app->overlay), app->debug_overlay);
    gtk_widget_set_visible(app->debug_overlay, FALSE);

    /* CSS */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        "window { background-color: #021b33; }"
        ".fish-layer { background-color: transparent; }"
        ".sidebar { background-color: alpha(#0a1628, 0.96); border-left: 1px solid #224466; }"
        ".dashboard { background-color: alpha(#0a1628, 0.96); border-top: 1px solid #224466; }"
        ".dash-icon { border: 2px solid #5599ff; border-radius: 4px; padding: 2px; background: alpha(black, 0.4); }"
        ".dash-count { color: #88ccff; font-weight: bold; font-size: 16px; margin-left: 4px; }"
        "label { color: #ddeeff; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    /* Key controller */
    GtkEventController *key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_pressed), app);
    gtk_widget_add_controller(app->window, key);

    gtk_window_present(GTK_WINDOW(app->window));
    gtk_media_stream_play(GTK_MEDIA_STREAM(app->background_stream));
    if (app->fullscreen) gtk_window_fullscreen(GTK_WINDOW(app->window));

    world_populate(&app->world);
    g_timeout_add(TICK_MS, tick_cb, app);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.gtkaqua.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), 0);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
