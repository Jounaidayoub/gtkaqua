#include <gtk/gtk.h>

#include <cairo.h>
#include <math.h>
#include <stdbool.h>

#include "config.h"
#include "entity.h"
#include "species.h"
#include "world.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *overlay;
    GtkWidget *background_picture;
    GtkMediaStream *background_stream;
    GtkWidget *container;
    GtkWidget *debug_overlay;
    bool fullscreen;
    bool debug_mode;
    World world;
} AppState;

static void on_background_error(GObject *obj, GParamSpec *pspec, gpointer user_data) {
    (void) pspec;
    (void) user_data;

    GtkMediaStream *stream = GTK_MEDIA_STREAM(obj);
    const GError *err = gtk_media_stream_get_error(stream);
    if (err != 0) {
        g_warning("Background video error: %s", err->message);
    }
}

static gboolean tick_cb(gpointer data) {
    AppState *app = (AppState *) data;

    int w = gtk_widget_get_width(app->container);
    int h = gtk_widget_get_height(app->container);
    if (w > 0 && h > 0) {
        app->world.width = (double) w;
        app->world.height = (double) h;
    }

    world_tick(&app->world);
    if (app->debug_mode && app->debug_overlay != 0) {
        gtk_widget_queue_draw(app->debug_overlay);
    }
    return G_SOURCE_CONTINUE;
}

static void draw_arrow(cairo_t *cr, Vec2 start, Vec2 vec, double scale, double r, double g, double b) {
    Vec2 end = {start.x + (vec.x * scale), start.y + (vec.y * scale)};
    double dx = end.x - start.x;
    double dy = end.y - start.y;
    double len = sqrt((dx * dx) + (dy * dy));
    if (len < 1e-3) {
        return;
    }

    double ux = dx / len;
    double uy = dy / len;
    double head_len = 8.0;
    double wing = 4.0;
    Vec2 left = {end.x - (ux * head_len) + (-uy * wing), end.y - (uy * head_len) + (ux * wing)};
    Vec2 right = {end.x - (ux * head_len) - (-uy * wing), end.y - (uy * head_len) - (ux * wing)};

    cairo_set_source_rgba(cr, r, g, b, 0.96);
    cairo_move_to(cr, start.x, start.y);
    cairo_line_to(cr, end.x, end.y);
    cairo_stroke(cr);

    cairo_move_to(cr, end.x, end.y);
    cairo_line_to(cr, left.x, left.y);
    cairo_line_to(cr, right.x, right.y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

static void debug_draw_cb(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    (void) area;
    (void) width;
    (void) height;

    AppState *app = (AppState *) user_data;
    if (app == 0 || !app->debug_mode) {
        return;
    }

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 0.20, 0.92, 0.95, 0.95);
    cairo_rectangle(cr, 0.5, 0.5, app->world.width - 1.0, app->world.height - 1.0);
    cairo_stroke(cr);

    for (int i = 0; i < app->world.entity_count; i++) {
        Entity *e = &app->world.entities[i];
        if (e->state == ENTITY_INACTIVE) {
            continue;
        }

        double bx = 0.0;
        double by = 0.0;
        double bw = 0.0;
        double bh = 0.0;
        if (!entity_bounds_rect(&app->world, e, &bx, &by, &bw, &bh)) {
            continue;
        }

        cairo_set_source_rgba(cr, 0.98, 0.84, 0.15, 0.88);
        cairo_rectangle(cr, bx, by, bw, bh);
        cairo_stroke(cr);

        const SpeciesConfig *cfg = species_get(e->species);
        if (cfg != 0) {
            if (cfg->fear_radius > 0.0) {
                cairo_set_source_rgba(cr, 1.0, 0.32, 0.32, 0.26);
                cairo_arc(cr, e->pos.x, e->pos.y, cfg->fear_radius, 0.0, 6.28318530718);
                cairo_stroke(cr);
            }
            if (cfg->hunt_radius > 0.0) {
                cairo_set_source_rgba(cr, 0.77, 0.47, 1.0, 0.20);
                cairo_arc(cr, e->pos.x, e->pos.y, cfg->hunt_radius, 0.0, 6.28318530718);
                cairo_stroke(cr);
            }
        }

        draw_arrow(cr, e->pos, e->dbg_force_sep, 46.0, 1.0, 0.42, 0.16);
        draw_arrow(cr, e->pos, e->dbg_force_ali, 46.0, 0.17, 0.78, 1.0);
        draw_arrow(cr, e->pos, e->dbg_force_coh, 46.0, 0.36, 1.0, 0.35);
        draw_arrow(cr, e->pos, e->dbg_force_flee, 46.0, 1.0, 0.28, 0.80);
        draw_arrow(cr, e->pos, e->dbg_force_sum, 36.0, 1.0, 1.0, 1.0);
    }

    cairo_set_source_rgba(cr, 0.10, 0.12, 0.16, 0.82);
    cairo_rectangle(cr, 10.0, 10.0, 290.0, 64.0);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1.0, 0.4, 0.4);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);
    cairo_move_to(cr, 20.0, 34.0);
    cairo_show_text(cr, "DEBUG MODE");
    cairo_set_source_rgb(cr, 0.86, 0.94, 1.0);
    cairo_set_font_size(cr, 12.0);
    cairo_move_to(cr, 20.0, 54.0);
    cairo_show_text(cr, "Orange=sep, Blue=align, Green=coh, Pink=flee, White=sum");
}

static void toggle_fullscreen(AppState *app) {
    if (app->fullscreen) {
        gtk_window_unfullscreen(GTK_WINDOW(app->window));
        app->fullscreen = false;
    } else {
        gtk_window_fullscreen(GTK_WINDOW(app->window));
        app->fullscreen = true;
    }
}

static gboolean on_key_pressed(
    GtkEventControllerKey *controller,
    guint keyval,
    guint keycode,
    GdkModifierType state,
    gpointer user_data
) {
    (void) controller;
    (void) keycode;
    (void) state;

    AppState *app = (AppState *) user_data;

    if (keyval == GDK_KEY_F11) {
        toggle_fullscreen(app);
        return TRUE;
    }

    if (keyval == GDK_KEY_Escape && app->fullscreen) {
        toggle_fullscreen(app);
        return TRUE;
    }

    if (keyval == GDK_KEY_F3 || keyval == GDK_KEY_d || keyval == GDK_KEY_D) {
        app->debug_mode = !app->debug_mode;
        app->world.debug_mode = app->debug_mode;
        if (app->debug_overlay != 0) {
            gtk_widget_set_visible(app->debug_overlay, app->debug_mode);
            gtk_widget_queue_draw(app->debug_overlay);
        }
        return TRUE;
    }

    return FALSE;
}

static void on_activate(GtkApplication *gtk_app, gpointer user_data) {
    (void) user_data;

    AppState *app = g_new0(AppState, 1);
    app->fullscreen = START_FULLSCREEN != 0;

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "GTK Aquarium");
    gtk_window_set_default_size(GTK_WINDOW(app->window), START_WIDTH, START_HEIGHT);

    app->overlay = gtk_overlay_new();
    gtk_window_set_child(GTK_WINDOW(app->window), app->overlay);
    app->debug_mode = false;

    app->background_stream = gtk_media_file_new_for_filename("assets/background-underwater.webm");
    gtk_media_stream_set_loop(GTK_MEDIA_STREAM(app->background_stream), TRUE);
    gtk_media_stream_set_muted(GTK_MEDIA_STREAM(app->background_stream), TRUE);
    gtk_media_stream_set_playing(GTK_MEDIA_STREAM(app->background_stream), TRUE);
    g_signal_connect(app->background_stream, "notify::error", G_CALLBACK(on_background_error), 0);

    app->background_picture = gtk_picture_new_for_paintable(GDK_PAINTABLE(app->background_stream));
    gtk_picture_set_can_shrink(GTK_PICTURE(app->background_picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(app->background_picture), GTK_CONTENT_FIT_COVER);
    gtk_widget_set_hexpand(app->background_picture, TRUE);
    gtk_widget_set_vexpand(app->background_picture, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(app->overlay), app->background_picture);

    app->container = gtk_fixed_new();
    gtk_widget_add_css_class(app->container, "fish-layer");
    gtk_widget_set_hexpand(app->container, TRUE);
    gtk_widget_set_vexpand(app->container, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(app->overlay), app->container);
    gtk_widget_set_can_target(app->container, FALSE);

    app->debug_overlay = gtk_drawing_area_new();
    gtk_widget_set_hexpand(app->debug_overlay, TRUE);
    gtk_widget_set_vexpand(app->debug_overlay, TRUE);
    gtk_widget_set_can_target(app->debug_overlay, FALSE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(app->debug_overlay), debug_draw_cb, app, 0);
    gtk_overlay_add_overlay(GTK_OVERLAY(app->overlay), app->debug_overlay);
    gtk_widget_set_visible(app->debug_overlay, FALSE);

    GtkCssProvider *bg_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
        bg_provider,
        "window { background-image: linear-gradient(180deg, #021b33 0%, #02294a 40%, #013054 100%); }"
        ".fish-layer { background-color: transparent; background-image: none; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(bg_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(bg_provider);

    GtkEventController *key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_pressed), app);
    gtk_widget_add_controller(app->window, key);


    gtk_window_present(GTK_WINDOW(app->window));
    gtk_media_stream_play(GTK_MEDIA_STREAM(app->background_stream));
    if (app->fullscreen) {
        gtk_window_fullscreen(GTK_WINDOW(app->window));
    }

    int width = gtk_widget_get_width(app->container);
    int height = gtk_widget_get_height(app->container);
    if (width <= 0) {
        width = START_WIDTH;
    }
    if (height <= 0) {
        height = START_HEIGHT;
    }

    world_init(&app->world, app->container, (double) width, (double) height);
    app->world.debug_mode = app->debug_mode;
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
