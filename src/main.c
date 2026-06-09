#include <gtk/gtk.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "config.h"
#include "world.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *overlay;
    GtkWidget *background_picture;
    GtkMediaStream *background_stream;
    GtkWidget *container;
    bool fullscreen;
    World world;
    GtkWidget *sidebar;
    GtkWidget *ctrl_frame;
    GtkWidget *speed_scale;
    GtkWidget *sim_frame;
    GtkWidget *pause_button;
    bool paused;
    bool trails;
    bool bubbles;
    bool debug_info;
    GtkWidget *species_spins[MAX_SPECIES];
    GtkWidget *pop_frame;
    GtkWidget *total_label;
    GtkWidget *species_pop_labels[MAX_SPECIES];
    AquariumConfig cfg;
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
    if (!app->paused) {
        world_tick(&app->world);
    }

    /* Update population counters */
    int sc = species_count();
    int total = 0;
    for (int s = 0; s < sc; s++) {
        total += app->world.alive_counts[s];
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "<b>Total: %d</b>", total);
    gtk_label_set_markup(GTK_LABEL(app->total_label), buf);

    for (int s = 0; s < sc; s++) {
        GtkWidget *lbl = app->species_pop_labels[s];
        if (!lbl) {
            continue;
        }
        const SpeciesConfig *cfg = species_get(s);
        if (!cfg || !cfg->name[0]) {
            continue;
        }
        snprintf(buf, sizeof(buf), "%s: %d/%d", cfg->name,
                 app->world.alive_counts[s], cfg->max_population);
        gtk_label_set_text(GTK_LABEL(lbl), buf);
    }

    return G_SOURCE_CONTINUE;
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
    return FALSE;
}

static void on_speed_changed(GtkRange *range, gpointer user_data) {
    AppState *app = (AppState *) user_data;
    double val = gtk_range_get_value(range);
    world_set_time_scale(&app->world, val);
}

static void on_species_add_clicked(GtkButton *btn, gpointer user_data) {
    (void) btn;
    AppState *app = (AppState *) user_data;
    int si = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "species-index"));
    Vec2 pos = { app->world.width * ((double) rand() / RAND_MAX), app->world.height * ((double) rand() / RAND_MAX) };
    Vec2 vel = { (double) rand() / RAND_MAX * 80.0 - 40.0, (double) rand() / RAND_MAX * 80.0 - 40.0 };
    world_spawn(&app->world, si, pos, vel);
}

static void on_species_remove_clicked(GtkButton *btn, gpointer user_data) {
    (void) btn;
    AppState *app = (AppState *) user_data;
    int si = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "species-index"));
    world_remove_species(&app->world, si, 1);
}

static void on_pause_clicked(GtkButton *btn, gpointer user_data) {
    AppState *app = (AppState *) user_data;
    app->paused = !app->paused;
    if (app->paused) {
        gtk_button_set_label(GTK_BUTTON(btn), "Resume");
    } else {
        gtk_button_set_label(GTK_BUTTON(btn), "Pause");
    }
}

static void rebuild_population_ui(AppState *app) {
    GtkWidget *pop_box = gtk_frame_get_child(GTK_FRAME(app->pop_frame));
    if (!pop_box) {
        return;
    }

    GtkWidget *child = gtk_widget_get_first_child(pop_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(pop_box), child);
        child = next;
    }

    memset(app->species_pop_labels, 0, sizeof(app->species_pop_labels));

    app->total_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(app->total_label), "<b>Total: 0</b>");
    gtk_box_append(GTK_BOX(pop_box), app->total_label);

    int sc = species_count();
    for (int s = 0; s < sc; s++) {
        const SpeciesConfig *cfg = species_get(s);
        if (!cfg || !cfg->name[0]) {
            continue;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%s: 0/%d", cfg->name, cfg->max_population);
        app->species_pop_labels[s] = gtk_label_new(buf);
        gtk_widget_set_halign(app->species_pop_labels[s], GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(pop_box), app->species_pop_labels[s]);
    }
}

static void rebuild_species_ui(AppState *app) {
    GtkWidget *ctrl_box = gtk_frame_get_child(GTK_FRAME(app->ctrl_frame));
    if (ctrl_box) {
        GtkWidget *child = gtk_widget_get_first_child(ctrl_box);
        while (child) {
            GtkWidget *next = gtk_widget_get_next_sibling(child);
            gtk_box_remove(GTK_BOX(ctrl_box), child);
            child = next;
        }
    } else {
        ctrl_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_frame_set_child(GTK_FRAME(app->ctrl_frame), ctrl_box);
    }

    memset(app->species_spins, 0, sizeof(app->species_spins));

    int sc = species_count();
    for (int s = 0; s < sc; s++) {
        const SpeciesConfig *cfg = species_get(s);
        if (!cfg || !cfg->name[0]) continue;

        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

        GtkWidget *lbl = gtk_label_new(cfg->name);
        gtk_widget_set_size_request(lbl, 78, -1);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(row), lbl);

        int max_pop = cfg->max_population > 0 ? cfg->max_population : 100;
        GtkWidget *spin = gtk_spin_button_new_with_range(0, max_pop, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), cfg->flock_size);
        gtk_widget_set_size_request(spin, 54, -1);
        gtk_box_append(GTK_BOX(row), spin);
        app->species_spins[s] = spin;

        GtkWidget *btn_minus = gtk_button_new_with_label("-");
        g_object_set_data(G_OBJECT(btn_minus), "species-index", GINT_TO_POINTER(s));
        g_signal_connect(btn_minus, "clicked", G_CALLBACK(on_species_remove_clicked), app);
        gtk_widget_set_size_request(btn_minus, 26, -1);
        gtk_box_append(GTK_BOX(row), btn_minus);

        GtkWidget *btn_plus = gtk_button_new_with_label("+");
        g_object_set_data(G_OBJECT(btn_plus), "species-index", GINT_TO_POINTER(s));
        g_signal_connect(btn_plus, "clicked", G_CALLBACK(on_species_add_clicked), app);
        gtk_widget_set_size_request(btn_plus, 26, -1);
        gtk_box_append(GTK_BOX(row), btn_plus);

        gtk_box_append(GTK_BOX(ctrl_box), row);
    }
}

static void on_reload_clicked(GtkButton *btn, gpointer user_data) {
    (void) btn;
    AppState *app = (AppState *) user_data;

    AquariumConfig new_cfg;
    if (conf_load(&new_cfg, NULL) != 0) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "Failed to load config:\n%s", conf_error(&new_cfg));
        gtk_window_present(GTK_WINDOW(dialog));
        g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
        return;
    }

    app->cfg = new_cfg;
    species_init(app->cfg.species, app->cfg.species_count);

    world_clear(&app->world);
    world_init(&app->world, app->container,
               (double) g_start_width, (double) g_start_height);
    world_set_time_scale(&app->world, gtk_range_get_value(GTK_RANGE(app->speed_scale)));

    rebuild_species_ui(app);

    int sc2 = species_count();
    for (int s = 0; s < sc2; s++) {
        GtkWidget *spin = app->species_spins[s];
        if (!spin) continue;
        int target = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
        for (int i = 0; i < target; i++) {
            Vec2 pos = { app->world.width * ((double) rand() / RAND_MAX), app->world.height * ((double) rand() / RAND_MAX) };
            Vec2 vel = { (double) rand() / RAND_MAX * 80.0 - 40.0, (double) rand() / RAND_MAX * 80.0 - 40.0 };
            world_spawn(&app->world, s, pos, vel);
        }
    }

    /* Show brief info */
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "Config reloaded with %d species. "
        "Adjust target numbers with the spinners and press Initialize pool.",
        sc2);
    gtk_window_present(GTK_WINDOW(dialog));
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
}

static void on_reset_clicked(GtkButton *btn, gpointer user_data) {
    (void) btn;
    AppState *app = (AppState *) user_data;
    int width = gtk_widget_get_width(app->container);
    int height = gtk_widget_get_height(app->container);
    if (width <= 0) width = g_start_width;
    if (height <= 0) height = g_start_height;
    world_clear(&app->world);
    world_init(&app->world, app->container, (double) width, (double) height);
    world_set_time_scale(&app->world, gtk_range_get_value(GTK_RANGE(app->speed_scale)));

    int sc = species_count();
    for (int s = 0; s < sc; s++) {
        GtkWidget *spin = app->species_spins[s];
        if (!spin) continue;
        int target = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
        for (int i = 0; i < target; i++) {
            Vec2 pos = { app->world.width * ((double) rand() / RAND_MAX), app->world.height * ((double) rand() / RAND_MAX) };
            Vec2 vel = { (double) rand() / RAND_MAX * 80.0 - 40.0, (double) rand() / RAND_MAX * 80.0 - 40.0 };
            world_spawn(&app->world, s, pos, vel);
        }
    }
    rebuild_population_ui(app);
}

static void on_activate(GtkApplication *gtk_app, gpointer user_data) {
    (void) user_data;

    AppState *app = g_new0(AppState, 1);
    app->paused = false;
    app->trails = false;
    app->bubbles = false;
    app->debug_info = false;

    /* Load config */
    if (conf_load(&app->cfg, NULL) != 0) {
        g_printerr("WARNING: %s\n", conf_error(&app->cfg));
        g_printerr("Using default values.\n");
    }
    species_init(app->cfg.species, app->cfg.species_count);

    app->fullscreen = g_start_fullscreen != 0;

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "GTK Aquarium");
    gtk_window_set_default_size(GTK_WINDOW(app->window), g_start_width, g_start_height);

    GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(app->window), root_box);

    /* Sidebar */
    GtkWidget *sidebar_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(sidebar_scroll, TRUE);
    gtk_box_append(GTK_BOX(root_box), sidebar_scroll);

    app->sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(app->sidebar, 6);
    gtk_widget_set_margin_top(app->sidebar, 6);
    gtk_widget_set_margin_bottom(app->sidebar, 6);
    gtk_widget_set_size_request(app->sidebar, 220, -1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebar_scroll), app->sidebar);

    /* ---- Population counter ---- */
    app->pop_frame = gtk_frame_new("Population");
    gtk_widget_set_margin_bottom(app->pop_frame, 6);
    gtk_box_append(GTK_BOX(app->sidebar), app->pop_frame);

    GtkWidget *pop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_frame_set_child(GTK_FRAME(app->pop_frame), pop_box);

    app->total_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(app->total_label), "<b>Total: 0</b>");
    gtk_box_append(GTK_BOX(pop_box), app->total_label);

    int sc = species_count();
    for (int s = 0; s < sc; s++) {
        const SpeciesConfig *cfg = species_get(s);
        if (!cfg || !cfg->name[0]) {
            continue;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "%s: 0/%d", cfg->name, cfg->max_population);
        app->species_pop_labels[s] = gtk_label_new(buf);
        gtk_widget_set_halign(app->species_pop_labels[s], GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(pop_box), app->species_pop_labels[s]);
    }

    /* Aquarium area */
    app->overlay = gtk_overlay_new();
    gtk_box_append(GTK_BOX(root_box), app->overlay);

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

    /* ---- Population control ---- */
    app->ctrl_frame = gtk_frame_new("Population Control");
    gtk_widget_set_margin_bottom(app->ctrl_frame, 6);
    gtk_box_append(GTK_BOX(app->sidebar), app->ctrl_frame);
    rebuild_species_ui(app);

    /* ---- Simulation controls ---- */
    app->sim_frame = gtk_frame_new("Simulation");
    gtk_widget_set_margin_bottom(app->sim_frame, 6);
    GtkWidget *sim_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_frame_set_child(GTK_FRAME(app->sim_frame), sim_box);
    gtk_box_append(GTK_BOX(app->sidebar), app->sim_frame);

    GtkWidget *lbl_speed = gtk_label_new("Global speed:");
    gtk_box_append(GTK_BOX(sim_box), lbl_speed);

    app->speed_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 4.0, 0.1);
    gtk_range_set_value(GTK_RANGE(app->speed_scale), 1.0);
    gtk_widget_set_halign(app->speed_scale, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(app->speed_scale, TRUE);
    g_signal_connect(app->speed_scale, "value-changed", G_CALLBACK(on_speed_changed), app);
    gtk_box_append(GTK_BOX(sim_box), app->speed_scale);

    GtkWidget *h_toggles = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(sim_box), h_toggles);
    GtkWidget *chk_trails = gtk_check_button_new_with_label("Trails");
    g_signal_connect(chk_trails, "toggled", G_CALLBACK(gtk_widget_set_sensitive), app);
    gtk_box_append(GTK_BOX(h_toggles), chk_trails);
    GtkWidget *chk_bubbles = gtk_check_button_new_with_label("Bubbles");
    g_signal_connect(chk_bubbles, "toggled", G_CALLBACK(gtk_widget_set_sensitive), app);
    gtk_box_append(GTK_BOX(h_toggles), chk_bubbles);
    GtkWidget *chk_debug = gtk_check_button_new_with_label("Debug info");
    gtk_box_append(GTK_BOX(sim_box), chk_debug);

    app->pause_button = gtk_button_new_with_label("Pause");
    g_signal_connect(app->pause_button, "clicked", G_CALLBACK(on_pause_clicked), app);
    gtk_box_append(GTK_BOX(sim_box), app->pause_button);

    GtkWidget *btn_reset = gtk_button_new_with_label("Initialize pool");
    g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_reset_clicked), app);
    gtk_box_append(GTK_BOX(sim_box), btn_reset);

    /* ---- Reload config button ---- */
    GtkWidget *btn_reload = gtk_button_new_with_label("Reload config");
    g_signal_connect(btn_reload, "clicked", G_CALLBACK(on_reload_clicked), app);
    gtk_box_append(GTK_BOX(app->sidebar), btn_reload);

    /* ---- Selected fish info ---- */
    GtkWidget *sel_frame = gtk_frame_new("Selected fish");
    GtkWidget *sel_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_frame_set_child(GTK_FRAME(sel_frame), sel_box);
    GtkWidget *lbl_sel_hint = gtk_label_new("Click on a fish");
    gtk_box_append(GTK_BOX(sel_box), lbl_sel_hint);
    gtk_box_append(GTK_BOX(app->sidebar), sel_frame);

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
    if (width <= 0) width = g_start_width;
    if (height <= 0) height = g_start_height;

    world_init(&app->world, app->container, (double) width, (double) height);
    world_set_time_scale(&app->world, 1.0);

    int sc3 = species_count();
    for (int s = 0; s < sc3; s++) {
        GtkWidget *spin = app->species_spins[s];
        if (!spin) continue;
        int target = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
        for (int i = 0; i < target; i++) {
            Vec2 pos = { app->world.width * ((double) rand() / RAND_MAX), app->world.height * ((double) rand() / RAND_MAX) };
            Vec2 vel = { (double) rand() / RAND_MAX * 80.0 - 40.0, (double) rand() / RAND_MAX * 80.0 - 40.0 };
            world_spawn(&app->world, s, pos, vel);
        }
    }
    rebuild_population_ui(app);

    g_timeout_add(g_tick_ms, tick_cb, app);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.gtkaqua.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), 0);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
