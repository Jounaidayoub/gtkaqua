#include "world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "entity.h"

static double rand_range(double min_v, double max_v) {
    double t = (double) rand() / (double) RAND_MAX;
    return min_v + (max_v - min_v) * t;
}

static Vec2 random_position(World *w) {
    Vec2 p = {
        rand_range(0.0, w->width),
        rand_range(0.0, w->height),
    };
    return p;
}

static Vec2 random_velocity_for(const SpeciesConfig *cfg) {
    Vec2 d = vec2_random_unit();
    double s = rand_range(cfg->min_speed, cfg->max_speed);
    return vec2_scale(d, s);
}

static double heading_deg_from_vel(Vec2 vel) {
    return atan2(vel.y, vel.x) * (180.0 / M_PI) + 180.0;
}

void world_init(World *w, GtkWidget *container, double width, double height) {
    srand((unsigned int) time(0));

    w->entity_count = 0;
    w->width = width;
    w->height = height;
    w->container = container;
    w->dt = DT;
    w->elapsed = 0.0;
    w->next_id = 1;
    w->debug_mode = false;

    for (int s = 0; s < SPECIES_COUNT; s++) {
        w->alive_counts[s] = 0;
        w->respawn_ready[s] = 0.0;
    }
}

int world_spawn(World *w, SpeciesKind kind, Vec2 pos, Vec2 vel) {
    if (w->entity_count >= MAX_ENTITIES) {
        return -1;
    }

    const SpeciesConfig *cfg = species_get(kind);
    if (cfg == 0) {
        return -1;
    }

    int index = w->entity_count++;
    Entity *e = &w->entities[index];

    e->id = w->next_id++;
    e->species = kind;
    e->pos = pos;
    e->vel = vel;
    e->acc = (Vec2) {0.0, 0.0};
    e->angle = heading_deg_from_vel(vel);
    e->age = 0.0;
    e->state = ENTITY_ALIVE;
    e->respawn_at = 0.0;
    e->speed_scale = rand_range(0.82, 1.30);
    e->death_timer = 0.0;
    snprintf(e->css_class, sizeof(e->css_class), "entity-%d", e->id);

    e->widget = gtk_picture_new_for_filename(cfg->asset_path);
    gtk_widget_set_size_request(e->widget, cfg->sprite_width, cfg->sprite_height);
    gtk_widget_set_overflow(e->widget, GTK_OVERFLOW_VISIBLE);
    gtk_widget_add_css_class(e->widget, e->css_class);
    gtk_fixed_put(GTK_FIXED(w->container), e->widget, pos.x, pos.y);

    e->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(e->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );

    w->alive_counts[kind]++;
    return index;
}

void world_despawn(World *w, int index) {
    if (index < 0 || index >= w->entity_count) {
        return;
    }

    Entity *e = &w->entities[index];
    if (e->state == ENTITY_ALIVE) {
        e->state = ENTITY_DYING;
        if (w->alive_counts[e->species] > 0) {
            w->alive_counts[e->species]--;
        }
        e->death_timer = DEATH_FLASH_DURATION;
    }

    const SpeciesConfig *cfg = species_get(e->species);
    if (cfg != 0) {
        e->respawn_at = w->elapsed + cfg->respawn_delay;
        if (w->respawn_ready[e->species] < e->respawn_at) {
            w->respawn_ready[e->species] = e->respawn_at;
        }
    }

}

void world_populate(World *w) {
    for (int s = 0; s < SPECIES_COUNT; s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (cfg == 0) {
            continue;
        }
        int clusters = 1;
        if (cfg->flock_size >= 18) {
            clusters = 4;
        } else if (cfg->flock_size >= 8) {
            clusters = 2;
        }

        Vec2 centers[4];
        for (int c = 0; c < clusters; c++) {
            centers[c] = random_position(w);
        }

        for (int i = 0; i < cfg->flock_size; i++) {
            Vec2 center = centers[i % clusters];
            Vec2 p = {
                center.x + rand_range(-90.0, 90.0),
                center.y + rand_range(-70.0, 70.0),
            };
            p = vec2_wrap(p, w->width, w->height);
            Vec2 v = random_velocity_for(cfg);
            world_spawn(w, cfg->kind, p, v);
        }
    }
}

static void world_handle_eating(World *w) {
    for (int i = 0; i < w->entity_count; i++) {
        Entity *pred = &w->entities[i];
        if (pred->state != ENTITY_ALIVE) {
            continue;
        }
        const SpeciesConfig *pred_cfg = species_get(pred->species);
        if (pred_cfg == 0 || pred_cfg->prey_mask == 0) {
            continue;
        }

        Vec2 pred_mouth = entity_head_point(pred, pred_cfg);

        for (int j = 0; j < w->entity_count; j++) {
            if (i == j) {
                continue;
            }
            Entity *prey = &w->entities[j];
            if (prey->state != ENTITY_ALIVE || !EATS(*pred_cfg, prey->species)) {
                continue;
            }

            const SpeciesConfig *prey_cfg = species_get(prey->species);
            if (prey_cfg == 0) {
                continue;
            }

            double prey_hit = ((double) (prey_cfg->sprite_width < prey_cfg->sprite_height ? prey_cfg->sprite_width : prey_cfg->sprite_height)) * 0.28;
            if (prey_hit < 8.0) {
                prey_hit = 8.0;
            }
            double eat_dist = EAT_RADIUS + prey_hit;

            double d2 = vec2_dist_sq_wrap(pred_mouth, prey->pos, w->width, w->height);
            if (d2 < eat_dist * eat_dist) {
                world_despawn(w, j);
            }
        }
    }
}

static void world_handle_lifespans(World *w) {
    for (int i = 0; i < w->entity_count; i++) {
        Entity *e = &w->entities[i];
        if (e->state != ENTITY_ALIVE) {
            continue;
        }
        const SpeciesConfig *cfg = species_get(e->species);
        if (cfg != 0 && cfg->lifespan > 0.0 && e->age > cfg->lifespan) {
            world_despawn(w, i);
        }
    }
}

static void world_handle_respawns(World *w) {
    for (int s = 0; s < SPECIES_COUNT; s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (cfg == 0) {
            continue;
        }

        if (w->alive_counts[s] >= cfg->flock_size) {
            continue;
        }

        if (w->elapsed < w->respawn_ready[s]) {
            continue;
        }

        for (int i = 0; i < w->entity_count; i++) {
            Entity *e = &w->entities[i];
            if (e->state != ENTITY_INACTIVE || e->species != (SpeciesKind) s) {
                continue;
            }
            e->state = ENTITY_ALIVE;
            e->age = 0.0;
            e->respawn_at = 0.0;
            e->pos = random_position(w);
            e->vel = random_velocity_for(cfg);
            e->acc = (Vec2) {0.0, 0.0};
            e->angle = heading_deg_from_vel(e->vel);
            e->speed_scale = rand_range(0.82, 1.30);
            e->death_timer = 0.0;
            if (e->widget != 0) {
                gtk_widget_set_visible(e->widget, TRUE);
            }
            w->alive_counts[s]++;
            w->respawn_ready[s] = w->elapsed + cfg->respawn_delay;
            break;
        }
    }
}

void world_tick(World *w) {
    w->elapsed += w->dt;

    for (int i = 0; i < w->entity_count; i++) {
        entity_tick(w, i);
    }

    world_handle_eating(w);
    world_handle_lifespans(w);

    for (int i = 0; i < w->entity_count; i++) {
        entity_apply_visuals(w, i);
    }

    world_handle_respawns(w);
}

gboolean world_tick_cb(gpointer data) {
    World *w = (World *) data;
    world_tick(w);
    return G_SOURCE_CONTINUE;
}
