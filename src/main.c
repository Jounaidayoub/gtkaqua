#include <gtk/gtk.h>

#include <stdbool.h>

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
    GtkWidget *species_combo;
    GtkWidget *speed_scale;
    GtkWidget *add_count_spin;
    GtkWidget *remove_count_spin;
    GtkWidget *pause_button;
    bool paused;
    bool trails;
    bool bubbles;
    bool debug_info;
    GtkWidget *species_checks[SPECIES_COUNT];
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

    g_printerr("TICK: app=%p container=%p world.container=%p\n", (void*)app, (void*)app->container, (void*)app->world.container);

    int w = gtk_widget_get_width(app->container);
    int h = gtk_widget_get_height(app->container);
    if (w > 0 && h > 0) {
        app->world.width = (double) w;
        app->world.height = (double) h;
    }

    if (!app->paused) {
        world_tick(&app->world);
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

static int get_active_species(AppState *app) {
    gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(app->species_combo));
    if (active < 0 || active >= SPECIES_COUNT) {
        return -1;
    }
    return active;
}

static void sync_species_limits(AppState *app) {
    int active = get_active_species(app);
    if (active < 0) {
        return;
    }
    const SpeciesConfig *cfg = species_get((SpeciesKind) active);
    if (!cfg) {
        return;
    }
    if (app->add_count_spin) {
        int max_add = cfg->max_population > 0 ? cfg->max_population : 100;
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(app->add_count_spin), 1, max_add);
        if (gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->add_count_spin)) > max_add) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->add_count_spin), max_add);
        }
    }
    if (app->remove_count_spin) {
        int max_remove = cfg->max_population > 0 ? cfg->max_population : 100;
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(app->remove_count_spin), 1, max_remove);
    }
}

static void on_species_changed(GtkComboBox *combo, gpointer user_data) {
    (void) combo;
    sync_species_limits((AppState *) user_data);
}

static void on_remove_fish_clicked(GtkButton *btn, gpointer user_data) {
    (void) btn;
    AppState *app = (AppState *) user_data;
    int active = get_active_species(app);
    if (active < 0) {
        return;
    }
    int count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->remove_count_spin));
    world_remove_species(&app->world, (SpeciesKind) active, count);
}

static void on_precise_add_clicked(GtkButton *btn, gpointer user_data) {
    (void) btn;
    AppState *app = (AppState *) user_data;
    int active = get_active_species(app);
    if (active < 0 || active >= SPECIES_COUNT) return;
    int count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->add_count_spin));
    for (int i = 0; i < count; i++) {
        Vec2 pos = { app->world.width * ((double) rand() / RAND_MAX), app->world.height * ((double) rand() / RAND_MAX) };
        Vec2 vel = { (double) rand() / RAND_MAX * 80.0 - 40.0, (double) rand() / RAND_MAX * 80.0 - 40.0 };
        world_spawn(&app->world, (SpeciesKind) active, pos, vel);
    }
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

static void on_reset_clicked(GtkButton *btn, gpointer user_data) {
    (void) btn;
    AppState *app = (AppState *) user_data;
    int width = gtk_widget_get_width(app->container);
    int height = gtk_widget_get_height(app->container);
    if (width <= 0) width = START_WIDTH;
    if (height <= 0) height = START_HEIGHT;
    gboolean enabled[SPECIES_COUNT];
    for (int s = 0; s < SPECIES_COUNT; s++) {
        enabled[s] = app->species_checks[s] != 0 && gtk_check_button_get_active(GTK_CHECK_BUTTON(app->species_checks[s]));
    }
    world_clear(&app->world);
    world_init(&app->world, app->container, (double) width, (double) height);
    world_set_time_scale(&app->world, gtk_range_get_value(GTK_RANGE(app->speed_scale)));
    world_populate_enabled(&app->world, enabled);
    sync_species_limits(app);
}

static void on_activate(GtkApplication *gtk_app, gpointer user_data) {
    (void) user_data;

    AppState *app = g_new0(AppState, 1);
    app->fullscreen = START_FULLSCREEN != 0;

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "GTK Aquarium");
    gtk_window_set_default_size(GTK_WINDOW(app->window), START_WIDTH, START_HEIGHT);

    /* Root horizontal box: scrollable sidebar + main overlay */
    GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(app->window), root_box);

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

    /* --- Pool species checkboxes --- */
    GtkWidget *pool_frame = gtk_frame_new(NULL);
    gtk_widget_set_margin_bottom(pool_frame, 6);
    GtkWidget *pool_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_frame_set_child(GTK_FRAME(pool_frame), pool_box);
    gtk_box_append(GTK_BOX(app->sidebar), pool_frame);

    GtkWidget *pool_title = gtk_label_new("Pool species");
    gtk_box_append(GTK_BOX(pool_box), pool_title);

    const SpeciesKind pool_order[] = {
        SPECIES_SHARK,
        SPECIES_TUNA,
        SPECIES_BARRACUDA,
        SPECIES_BASS,
        SPECIES_TROUT,
        SPECIES_ANGELFISH,
        SPECIES_CLOWNFISH,
        SPECIES_GOLDFISH,
    };

    for (int i = 0; i < SPECIES_COUNT; i++) {
        int s = (int) pool_order[i];
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (!cfg || !cfg->name) {
            continue;
        }
        char label[96];
        if (cfg->kind == SPECIES_SHARK) {
            snprintf(label, sizeof(label), "%s (apex)  (init %d / max %d)", cfg->name, cfg->flock_size, cfg->max_population);
        } else {
            snprintf(label, sizeof(label), "%s  (init %d / max %d)", cfg->name, cfg->flock_size, cfg->max_population);
        }
        app->species_checks[s] = gtk_check_button_new_with_label(label);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(app->species_checks[s]), TRUE);
        gtk_box_append(GTK_BOX(pool_box), app->species_checks[s]);
    }

    /* --- Precise add section --- */
    GtkWidget *precise_frame = gtk_frame_new(NULL);
    gtk_widget_set_margin_bottom(precise_frame, 6);
    GtkWidget *precise_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_frame_set_child(GTK_FRAME(precise_frame), precise_box);
    gtk_box_append(GTK_BOX(app->sidebar), precise_frame);

    GtkWidget *lbl_precise = gtk_label_new("Add precise");
    gtk_box_append(GTK_BOX(precise_box), lbl_precise);

    GtkWidget *h1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(precise_box), h1);
    GtkWidget *lbl_e = gtk_label_new("Species:");
    gtk_box_append(GTK_BOX(h1), lbl_e);

    app->species_combo = gtk_combo_box_text_new();
    for (int s = 0; s < SPECIES_COUNT; s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (cfg != 0 && cfg->name != 0) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->species_combo), cfg->name);
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->species_combo), 0);
    g_signal_connect(app->species_combo, "changed", G_CALLBACK(on_species_changed), app);
    gtk_box_append(GTK_BOX(h1), app->species_combo);

    GtkWidget *h2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(precise_box), h2);
    GtkWidget *lbl_count = gtk_label_new("Number:");
    gtk_box_append(GTK_BOX(h2), lbl_count);
    app->add_count_spin = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->add_count_spin), 2);
    gtk_box_append(GTK_BOX(h2), app->add_count_spin);

    GtkWidget *btn_precise_add = gtk_button_new_with_label("+ Add");
    g_signal_connect(btn_precise_add, "clicked", G_CALLBACK(on_precise_add_clicked), app);
    gtk_box_append(GTK_BOX(precise_box), btn_precise_add);

    GtkWidget *remove_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(precise_box), remove_row);
    GtkWidget *lbl_remove = gtk_label_new("Remove:");
    gtk_box_append(GTK_BOX(remove_row), lbl_remove);
    app->remove_count_spin = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->remove_count_spin), 1);
    gtk_box_append(GTK_BOX(remove_row), app->remove_count_spin);

    GtkWidget *btn_remove_spec = gtk_button_new_with_label("- Remove");
    g_signal_connect(btn_remove_spec, "clicked", G_CALLBACK(on_remove_fish_clicked), app);
    gtk_box_append(GTK_BOX(precise_box), btn_remove_spec);

    /* --- Simulation controls --- */
    GtkWidget *sim_frame = gtk_frame_new("Simulation");
    gtk_widget_set_margin_bottom(sim_frame, 6);
    GtkWidget *sim_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_frame_set_child(GTK_FRAME(sim_frame), sim_box);
    gtk_box_append(GTK_BOX(app->sidebar), sim_frame);

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
    g_signal_connect(chk_trails, "toggled", G_CALLBACK(gtk_widget_set_sensitive), app); /* placeholder */
    gtk_box_append(GTK_BOX(h_toggles), chk_trails);
    GtkWidget *chk_bubbles = gtk_check_button_new_with_label("Bubbles");
    g_signal_connect(chk_bubbles, "toggled", G_CALLBACK(gtk_widget_set_sensitive), app); /* placeholder */
    gtk_box_append(GTK_BOX(h_toggles), chk_bubbles);
    GtkWidget *chk_debug = gtk_check_button_new_with_label("Debug info");
    gtk_box_append(GTK_BOX(sim_box), chk_debug);

    app->pause_button = gtk_button_new_with_label("Pause");
    g_signal_connect(app->pause_button, "clicked", G_CALLBACK(on_pause_clicked), app);
    gtk_box_append(GTK_BOX(sim_box), app->pause_button);

    GtkWidget *btn_reset = gtk_button_new_with_label("Initialize pool");
    g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_reset_clicked), app);
    gtk_box_append(GTK_BOX(sim_box), btn_reset);

    /* --- Selected fish info --- */
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
    if (width <= 0) {
        width = START_WIDTH;
    }
    if (height <= 0) {
        height = START_HEIGHT;
    }

    world_init(&app->world, app->container, (double) width, (double) height);
    /* default speed */
    world_set_time_scale(&app->world, 1.0);
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
