#include "conf.h"
#include "config.h"

#include "toml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ---- Global variable definitions (declared extern in config.h) ---- */
double g_eat_radius = 10.0;
double g_adrenaline_boost = 2.5;
double g_rotation_smooth = 0.1;
double g_death_flash_duration = 0.22;
double g_starvation_seconds = 120.0;
int g_start_width = 1280;
int g_start_height = 720;
int g_start_fullscreen = 0;
int g_tick_ms = 16;
const char *g_assets_dir = "assets/";

/* ---- Helpers ---- */

static void set_error(AquariumConfig *cfg, const char *msg) {
    strncpy(cfg->last_error, msg, sizeof(cfg->last_error) - 1);
    cfg->last_error[sizeof(cfg->last_error) - 1] = '\0';
}

static int try_path(const char *path, AquariumConfig *cfg) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char errbuf[256];
    toml_table_t *tbl = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);
    if (!tbl) {
        char msg[512];
        snprintf(msg, sizeof(msg), "%s: %s", path, errbuf);
        set_error(cfg, msg);
        return -1;
    }

    /* ---- [global] ---- */
    toml_table_t *global = toml_table_in(tbl, "global");
    if (global) {
        toml_datum_t v;

        v = toml_double_in(global, "eat_radius");
        if (v.ok) cfg->global.eat_radius = v.u.d;

        v = toml_double_in(global, "adrenaline_boost");
        if (v.ok) cfg->global.adrenaline_boost = v.u.d;

        v = toml_double_in(global, "rotation_smooth");
        if (v.ok) cfg->global.rotation_smooth = v.u.d;

        v = toml_double_in(global, "death_flash_duration");
        if (v.ok) cfg->global.death_flash_duration = v.u.d;

        v = toml_double_in(global, "starvation_seconds");
        if (v.ok) cfg->global.starvation_seconds = v.u.d;

        v = toml_int_in(global, "start_width");
        if (v.ok && v.u.i > 0) cfg->global.start_width = (int)v.u.i;

        v = toml_int_in(global, "start_height");
        if (v.ok && v.u.i > 0) cfg->global.start_height = (int)v.u.i;

        v = toml_bool_in(global, "start_fullscreen");
        if (v.ok) cfg->global.start_fullscreen = v.u.b ? 1 : 0;

        v = toml_int_in(global, "tick_ms");
        if (v.ok && v.u.i >= 1) cfg->global.tick_ms = (int)v.u.i;

        v = toml_string_in(global, "assets_dir");
        if (v.ok) {
            strncpy(cfg->global.assets_dir, v.u.s, sizeof(cfg->global.assets_dir) - 1);
            free(v.u.s);
        }
    }

    /* ---- [[species]] — Pass 1: read all scalar fields ---- */
    toml_array_t *arr = toml_array_in(tbl, "species");
    int n = arr ? toml_array_nelem(arr) : 0;
    if (n > MAX_SPECIES) n = MAX_SPECIES;

    for (int i = 0; i < n; i++) {
        toml_table_t *s = toml_table_at(arr, i);
        if (!s) continue;

        SpeciesConfig *sp = &cfg->species[i];
        memset(sp, 0, sizeof(SpeciesConfig));

        toml_datum_t ds;

        ds = toml_string_in(s, "id");
        if (ds.ok) {
            strncpy(sp->id, ds.u.s, sizeof(sp->id) - 1);
            free(ds.u.s);
        }
        if (sp->id[0] == '\0') {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "species entry %d: missing or empty 'id'", i);
            toml_free(tbl);
            return -1;
        }

        ds = toml_string_in(s, "name");
        if (ds.ok) {
            strncpy(sp->name, ds.u.s, sizeof(sp->name) - 1);
            free(ds.u.s);
        } else {
            strncpy(sp->name, sp->id, sizeof(sp->name) - 1);
        }

        ds = toml_string_in(s, "sprite");
        if (ds.ok) {
            strncpy(sp->asset_path, ds.u.s, sizeof(sp->asset_path) - 1);
            free(ds.u.s);
        }
        if (sp->asset_path[0] == '\0') {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "species '%s': missing 'sprite' path", sp->id);
            toml_free(tbl);
            return -1;
        }

        toml_datum_t di;

        di = toml_int_in(s, "sprite_width");
        if (di.ok && di.u.i > 0) sp->sprite_width = (int)di.u.i;
        di = toml_int_in(s, "sprite_height");
        if (di.ok && di.u.i > 0) sp->sprite_height = (int)di.u.i;
        di = toml_int_in(s, "flock_size");
        if (di.ok && di.u.i >= 0) sp->flock_size = (int)di.u.i;
        di = toml_int_in(s, "max_population");
        if (di.ok && di.u.i >= 0) sp->max_population = (int)di.u.i;

        toml_datum_t dd;

        dd = toml_double_in(s, "mouth_forward_ratio");
        if (dd.ok) sp->mouth_forward_ratio = dd.u.d;
        dd = toml_double_in(s, "max_speed");
        if (dd.ok) sp->max_speed = dd.u.d;
        dd = toml_double_in(s, "min_speed");
        if (dd.ok) sp->min_speed = dd.u.d;
        dd = toml_double_in(s, "max_force");
        if (dd.ok) sp->max_force = dd.u.d;
        dd = toml_double_in(s, "separation_radius");
        if (dd.ok) sp->separation_radius = dd.u.d;
        dd = toml_double_in(s, "alignment_radius");
        if (dd.ok) sp->alignment_radius = dd.u.d;
        dd = toml_double_in(s, "cohesion_radius");
        if (dd.ok) sp->cohesion_radius = dd.u.d;
        dd = toml_double_in(s, "separation_weight");
        if (dd.ok) sp->separation_weight = dd.u.d;
        dd = toml_double_in(s, "alignment_weight");
        if (dd.ok) sp->alignment_weight = dd.u.d;
        dd = toml_double_in(s, "cohesion_weight");
        if (dd.ok) sp->cohesion_weight = dd.u.d;
        dd = toml_double_in(s, "fear_radius");
        if (dd.ok) sp->fear_radius = dd.u.d;
        dd = toml_double_in(s, "fear_weight");
        if (dd.ok) sp->fear_weight = dd.u.d;
        dd = toml_double_in(s, "hunt_radius");
        if (dd.ok) sp->hunt_radius = dd.u.d;
        dd = toml_double_in(s, "hunt_weight");
        if (dd.ok) sp->hunt_weight = dd.u.d;
        dd = toml_double_in(s, "avoid_same_radius");
        if (dd.ok) sp->avoid_same_radius = dd.u.d;
        dd = toml_double_in(s, "avoid_same_weight");
        if (dd.ok) sp->avoid_same_weight = dd.u.d;
        dd = toml_double_in(s, "lifespan");
        if (dd.ok) sp->lifespan = dd.u.d;
        dd = toml_double_in(s, "respawn_delay");
        if (dd.ok) sp->respawn_delay = dd.u.d;

        toml_datum_t db = toml_bool_in(s, "is_apex");
        if (db.ok) sp->is_apex = db.u.b ? true : false;
    }

    cfg->species_count = n;

    /* ---- Pass 2: resolve prey name arrays to bitmasks ---- */
    for (int i = 0; i < n; i++) {
        toml_table_t *s = toml_table_at(arr, i);
        if (!s) continue;

        cfg->species[i].prey_mask = 0;
        toml_array_t *prey_arr = toml_array_in(s, "prey");
        if (!prey_arr) continue;

        int pn = toml_array_nelem(prey_arr);
        for (int p = 0; p < pn; p++) {
            toml_datum_t prey_ds = toml_string_at(prey_arr, p);
            if (!prey_ds.ok) continue;

            int idx = -1;
            for (int j = 0; j < n; j++) {
                if (strcmp(cfg->species[j].id, prey_ds.u.s) == 0) {
                    idx = j;
                    break;
                }
            }
            if (idx < 0) {
                snprintf(cfg->last_error, sizeof(cfg->last_error),
                         "species '%s': unknown prey '%s'", cfg->species[i].id, prey_ds.u.s);
                free(prey_ds.u.s);
                toml_free(tbl);
                return -1;
            }
            cfg->species[i].prey_mask |= (1 << idx);
            free(prey_ds.u.s);
        }
    }

    toml_free(tbl);

    /* Write global values to process-wide globals */
    g_eat_radius = cfg->global.eat_radius;
    g_adrenaline_boost = cfg->global.adrenaline_boost;
    g_rotation_smooth = cfg->global.rotation_smooth;
    g_death_flash_duration = cfg->global.death_flash_duration;
    g_starvation_seconds = cfg->global.starvation_seconds;
    g_start_width = cfg->global.start_width;
    g_start_height = cfg->global.start_height;
    g_start_fullscreen = cfg->global.start_fullscreen;
    g_tick_ms = cfg->global.tick_ms;
    g_assets_dir = cfg->global.assets_dir;

    return 0;
}

int conf_load(AquariumConfig *cfg, const char *override_path) {
    memset(cfg, 0, sizeof(AquariumConfig));
    cfg->last_error[0] = '\0';

    /* Fill with sensible defaults */
    cfg->global.eat_radius = 10.0;
    cfg->global.adrenaline_boost = 2.5;
    cfg->global.rotation_smooth = 0.1;
    cfg->global.death_flash_duration = 0.22;
    cfg->global.starvation_seconds = 120.0;
    cfg->global.start_width = 1280;
    cfg->global.start_height = 720;
    cfg->global.start_fullscreen = 0;
    cfg->global.tick_ms = 16;
    strncpy(cfg->global.assets_dir, "assets/", sizeof(cfg->global.assets_dir) - 1);

    /* Build candidate paths */
    const char *paths[8];
    int npaths = 0;
    char buf[8][1024];

    if (override_path && override_path[0]) {
        paths[npaths] = override_path;
        npaths++;
    }

    /* 1. ./aquarium.conf */
    snprintf(buf[npaths], sizeof(buf[npaths]), "aquarium.conf");
    paths[npaths] = buf[npaths];
    npaths++;

    /* 2. $XDG_CONFIG_HOME/gtkaqua/aquarium.conf or ~/.config/gtkaqua/aquarium.conf */
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buf[npaths], sizeof(buf[npaths]), "%s/gtkaqua/aquarium.conf", xdg);
        paths[npaths] = buf[npaths];
        npaths++;
    } else {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(buf[npaths], sizeof(buf[npaths]), "%s/.config/gtkaqua/aquarium.conf", home);
            paths[npaths] = buf[npaths];
            npaths++;
        }
    }

    /* 3. /etc/gtkaqua/aquarium.conf */
    snprintf(buf[npaths], sizeof(buf[npaths]), "/etc/gtkaqua/aquarium.conf");
    paths[npaths] = buf[npaths];
    npaths++;

    for (int i = 0; i < npaths; i++) {
        struct stat st;
        if (stat(paths[i], &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }
        int ret = try_path(paths[i], cfg);
        if (ret == 0) return 0;
        return -1;
    }

    set_error(cfg,
        "no aquarium.conf found (checked ./aquarium.conf, "
        "~/.config/gtkaqua/aquarium.conf, /etc/gtkaqua/aquarium.conf)");
    return -1;
}

const char *conf_error(const AquariumConfig *cfg) {
    return cfg->last_error;
}
