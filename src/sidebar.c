#include "sidebar.h"

#include <stdio.h>

#include "species.h"
#include "world.h"

typedef struct { AppState *app; int species; double *field; } SpinCtx;

static void on_target_count_changed(GtkSpinButton *spin, gpointer ud) {
    SpinCtx *ctx = (SpinCtx *) ud;
    ctx->app->world.target_counts[ctx->species] = (int) gtk_spin_button_get_value(spin);
}

static void on_adv_changed(GtkSpinButton *spin, gpointer ud) {
    SpinCtx *ctx = (SpinCtx *) ud;
    *ctx->field = gtk_spin_button_get_value(spin);
}

static void on_reload_config(AppState *app) {
    species_load_defaults();
    if (!species_load_toml("config/aquarium.toml")) {
        g_warning("Could not load config/aquarium.toml; using built-in species defaults");
    }
    for (int s = 0; s < species_count(); s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (!cfg) continue;
        app->world.target_counts[s] = cfg->flock_size;
        app->world.effective[s].max_speed         = cfg->max_speed;
        app->world.effective[s].min_speed         = cfg->min_speed;
        app->world.effective[s].max_force         = cfg->max_force;
        app->world.effective[s].separation_radius = cfg->separation_radius;
        app->world.effective[s].separation_weight = cfg->separation_weight;
        app->world.effective[s].alignment_radius  = cfg->alignment_radius;
        app->world.effective[s].alignment_weight  = cfg->alignment_weight;
        app->world.effective[s].cohesion_radius   = cfg->cohesion_radius;
        app->world.effective[s].cohesion_weight   = cfg->cohesion_weight;
        app->world.effective[s].fear_radius       = cfg->fear_radius;
        app->world.effective[s].fear_weight       = cfg->fear_weight;
        app->world.effective[s].hunt_radius       = cfg->hunt_radius;
        app->world.effective[s].hunt_weight       = cfg->hunt_weight;
        app->world.effective[s].avoid_same_radius = cfg->avoid_same_radius;
        app->world.effective[s].avoid_same_weight = cfg->avoid_same_weight;
        app->world.effective[s].lifespan          = cfg->lifespan;
        app->world.effective[s].respawn_delay     = cfg->respawn_delay;
    }
    sidebar_refresh(app);
}

static void on_save_write(GFile *file, AppState *app) {
    char *path = g_file_get_path(file);
    if (!path) return;

    FILE *f = fopen(path, "w");
    if (!f) {
        g_warning("Failed to open %s for writing", path);
        g_free(path);
        return;
    }
    fprintf(f, "# GTK Aquarium species registry \u2014 auto-saved\n");
    for (int s = 0; s < species_count(); s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (!cfg) continue;
        EffectiveSpecies *eff = &app->world.effective[s];
        fprintf(f,
            "\n[species.%s]\n"
            "name = \"%s\"\n"
            "asset_path = \"%s\"\n"
            "count = %d\n"
            "sprite_width = %d\n"
            "sprite_height = %d\n"
            "mouth_forward_ratio = %.2f\n"
            "max_speed = %.0f\n"
            "min_speed = %.0f\n"
            "max_force = %.0f\n"
            "separation_radius = %.0f\n"
            "alignment_radius = %.0f\n"
            "cohesion_radius = %.0f\n"
            "separation_weight = %.1f\n"
            "alignment_weight = %.2f\n"
            "cohesion_weight = %.2f\n"
            "fear_radius = %.0f\n"
            "fear_weight = %.1f\n"
            "hunt_radius = %.0f\n"
            "hunt_weight = %.1f\n"
            "avoid_same_radius = %.0f\n"
            "avoid_same_weight = %.1f\n"
            "lifespan = %.0f\n"
            "respawn_delay = %.1f\n"
            "prey = [",
            cfg->key, cfg->name, cfg->asset_path,
            app->world.target_counts[s],
            cfg->sprite_width, cfg->sprite_height,
            cfg->mouth_forward_ratio,
            eff->max_speed,
            eff->min_speed,
            eff->max_force,
            eff->separation_radius,
            eff->alignment_radius,
            eff->cohesion_radius,
            eff->separation_weight,
            eff->alignment_weight,
            eff->cohesion_weight,
            eff->fear_radius,
            eff->fear_weight,
            eff->hunt_radius,
            eff->hunt_weight,
            eff->avoid_same_radius,
            eff->avoid_same_weight,
            eff->lifespan,
            eff->respawn_delay
        );
        int first = 1;
        for (int p = 0; p < species_count(); p++) {
            if (EATS(*cfg, p)) {
                const SpeciesConfig *prey_cfg = species_get((SpeciesKind) p);
                if (!prey_cfg) continue;
                fprintf(f, "%s\"%s\"", first ? "" : ", ", prey_cfg->key);
                first = 0;
            }
        }
        fprintf(f, "]\n");
    }
    fclose(f);
    g_free(path);
}

static void on_save_dialog_complete(GObject *source, GAsyncResult *result, gpointer data) {
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, NULL);
    if (!file) return;
    on_save_write(file, (AppState *) data);
    g_object_unref(file);
}

static void on_save_config(AppState *app) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Save Config");
    gtk_file_dialog_set_initial_name(dialog, "aquarium.toml");
    gtk_file_dialog_save(dialog, GTK_WINDOW(app->window), NULL, on_save_dialog_complete, app);
    g_object_unref(dialog);
}

static void on_load_dialog_complete(GObject *source, GAsyncResult *result, gpointer data) {
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, NULL);
    if (!file) return;

    AppState *app = (AppState *) data;
    char *path = g_file_get_path(file);
    if (!path) { g_object_unref(file); return; }

    species_load_defaults();
    if (!species_load_toml(path)) {
        g_warning("Failed to load config from %s", path);
        g_free(path);
        g_object_unref(file);
        return;
    }

    for (int s = 0; s < species_count(); s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (!cfg) continue;
        app->world.target_counts[s] = cfg->flock_size;
        app->world.effective[s].max_speed         = cfg->max_speed;
        app->world.effective[s].min_speed         = cfg->min_speed;
        app->world.effective[s].max_force         = cfg->max_force;
        app->world.effective[s].separation_radius = cfg->separation_radius;
        app->world.effective[s].separation_weight = cfg->separation_weight;
        app->world.effective[s].alignment_radius  = cfg->alignment_radius;
        app->world.effective[s].alignment_weight  = cfg->alignment_weight;
        app->world.effective[s].cohesion_radius   = cfg->cohesion_radius;
        app->world.effective[s].cohesion_weight   = cfg->cohesion_weight;
        app->world.effective[s].fear_radius       = cfg->fear_radius;
        app->world.effective[s].fear_weight       = cfg->fear_weight;
        app->world.effective[s].hunt_radius       = cfg->hunt_radius;
        app->world.effective[s].hunt_weight       = cfg->hunt_weight;
        app->world.effective[s].avoid_same_radius = cfg->avoid_same_radius;
        app->world.effective[s].avoid_same_weight = cfg->avoid_same_weight;
        app->world.effective[s].lifespan          = cfg->lifespan;
        app->world.effective[s].respawn_delay     = cfg->respawn_delay;
    }

    g_free(path);
    g_object_unref(file);

    sidebar_refresh(app);
}

static void on_load_config(AppState *app) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Load Config");
    gtk_file_dialog_open(dialog, GTK_WINDOW(app->window), NULL, on_load_dialog_complete, app);
    g_object_unref(dialog);
}

GtkWidget *sidebar_make_label(const char *text, bool bold) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    if (bold) {
        char markup[256];
        snprintf(markup, sizeof(markup), "<b>%s</b>", text);
        gtk_label_set_markup(GTK_LABEL(lbl), markup);
    }
    return lbl;
}

static GtkWidget *sidebar_build_vbox(AppState *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top   (vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_widget_set_margin_start (vbox, 12);
    gtk_widget_set_margin_end   (vbox, 12);

    GtkWidget *title = sidebar_make_label("Settings (Tab to toggle)", true);
    gtk_box_append(GTK_BOX(vbox), title);
    gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Population Counters */
    gtk_box_append(GTK_BOX(vbox), sidebar_make_label("Population Targets", true));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_box_append(GTK_BOX(vbox), grid);

    for (int s = 0; s < species_count(); s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (!cfg) continue;

        GtkWidget *img = gtk_picture_new_for_filename(cfg->asset_path);
        gtk_widget_set_size_request(img, 36, 24);
        gtk_grid_attach(GTK_GRID(grid), img, 0, s, 1, 1);

        gtk_grid_attach(GTK_GRID(grid), sidebar_make_label(cfg->name, false), 1, s, 1, 1);

        GtkWidget *spin = gtk_spin_button_new_with_range(0, MAX_ENTITIES, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), app->world.target_counts[s]);
        gtk_widget_set_hexpand(spin, TRUE);

        SpinCtx *ctx = g_new(SpinCtx, 1);
        ctx->app = app; ctx->species = s;
        g_signal_connect(spin, "value-changed", G_CALLBACK(on_target_count_changed), ctx);
        gtk_grid_attach(GTK_GRID(grid), spin, 2, s, 1, 1);
    }

    /* Config buttons */
    GtkWidget *cfg_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *reload_btn = gtk_button_new_with_label("Reload");
    g_signal_connect_swapped(reload_btn, "clicked", G_CALLBACK(on_reload_config), app);
    gtk_box_append(GTK_BOX(cfg_buttons), reload_btn);

    GtkWidget *load_btn = gtk_button_new_with_label("Load");
    g_signal_connect_swapped(load_btn, "clicked", G_CALLBACK(on_load_config), app);
    gtk_box_append(GTK_BOX(cfg_buttons), load_btn);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    g_signal_connect_swapped(save_btn, "clicked", G_CALLBACK(on_save_config), app);
    gtk_box_append(GTK_BOX(cfg_buttons), save_btn);

    gtk_box_append(GTK_BOX(vbox), cfg_buttons);

    gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Advanced */
    GtkWidget *expander = gtk_expander_new("Advanced Behavior");
    GtkWidget *adv_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(adv_box, 8);
    gtk_expander_set_child(GTK_EXPANDER(expander), adv_box);
    gtk_box_append(GTK_BOX(vbox), expander);

    for (int s = 0; s < species_count(); s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (!cfg) continue;
        EffectiveSpecies *eff = &app->world.effective[s];

        GtkWidget *s_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_box_append(GTK_BOX(adv_box), sidebar_make_label(cfg->name, true));

        struct { const char *lbl; double *ptr; double max; } fields[] = {
            {"Max Speed",   &eff->max_speed,   400.0},
            {"Fear Radius", &eff->fear_radius, 600.0},
            {"Fear Weight", &eff->fear_weight, 10.0},
            {"Hunt Radius", &eff->hunt_radius, 800.0},
            {"Hunt Weight", &eff->hunt_weight, 10.0},
        };

        for (int f = 0; f < 5; f++) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            GtkWidget *lbl = sidebar_make_label(fields[f].lbl, false);
            gtk_widget_set_size_request(lbl, 100, -1);
            gtk_box_append(GTK_BOX(row), lbl);

            GtkWidget *spin = gtk_spin_button_new_with_range(0.0, fields[f].max, fields[f].max > 50 ? 5.0 : 0.1);
            gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), *fields[f].ptr);
            gtk_widget_set_hexpand(spin, TRUE);

            SpinCtx *c = g_new(SpinCtx, 1);
            c->app = app; c->species = s; c->field = fields[f].ptr;
            g_signal_connect(spin, "value-changed", G_CALLBACK(on_adv_changed), c);

            gtk_box_append(GTK_BOX(row), spin);
            gtk_box_append(GTK_BOX(s_box), row);
        }
        gtk_box_append(GTK_BOX(adv_box), s_box);
    }

    return vbox;
}

void sidebar_refresh(AppState *app) {
    GtkWidget *vbox = sidebar_build_vbox(app);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(app->sidebar), vbox);
}

void sidebar_build(AppState *app) {
    app->sidebar = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(app->sidebar),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(app->sidebar, 280, -1);
    gtk_widget_add_css_class(app->sidebar, "sidebar");

    sidebar_refresh(app);
}
