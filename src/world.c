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

    w->entity_count  = 0;
    w->width         = width;
    w->height        = height;
    w->container     = container;
    w->dt            = DT;
    w->elapsed       = 0.0;
    w->next_id       = 1;
    w->debug_mode    = false;

    for (int s = 0; s < species_count(); s++) {
        w->alive_counts[s]  = 0;
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        w->target_counts[s] = cfg ? cfg->flock_size : 0;

        if (cfg) {
            w->effective[s].max_speed         = cfg->max_speed;
            w->effective[s].min_speed         = cfg->min_speed;
            w->effective[s].max_force         = cfg->max_force;
            w->effective[s].separation_radius = cfg->separation_radius;
            w->effective[s].separation_weight = cfg->separation_weight;
            w->effective[s].alignment_radius  = cfg->alignment_radius;
            w->effective[s].alignment_weight  = cfg->alignment_weight;
            w->effective[s].cohesion_radius   = cfg->cohesion_radius;
            w->effective[s].cohesion_weight   = cfg->cohesion_weight;
            w->effective[s].fear_radius       = cfg->fear_radius;
            w->effective[s].fear_weight       = cfg->fear_weight;
            w->effective[s].hunt_radius       = cfg->hunt_radius;
            w->effective[s].hunt_weight       = cfg->hunt_weight;
            w->effective[s].avoid_same_radius = cfg->avoid_same_radius;
            w->effective[s].avoid_same_weight = cfg->avoid_same_weight;
            w->effective[s].lifespan          = cfg->lifespan;
            w->effective[s].respawn_delay     = cfg->respawn_delay;
        }
    }
}

int world_spawn(World *w, SpeciesKind kind, Vec2 pos, Vec2 vel) {
    const SpeciesConfig *cfg = species_get(kind);
    if (cfg == 0) return -1;

    int index = -1;
    if (w->entity_count < MAX_ENTITIES) {
        index = w->entity_count++;
    } else {
        /* BUGFIX: If pool is full, find a dead/inactive fish slot and reuse it! */
        for (int i = 0; i < MAX_ENTITIES; i++) {
            if (w->entities[i].state == ENTITY_INACTIVE) {
                index = i;
                break;
            }
        }
        if (index == -1) return -1; /* Literally 256 alive/dying fish on screen */
    }

    Entity *e = &w->entities[index];

    /* If we are reusing a slot, clean up its old GTK widget to prevent ghost images */
    if (e->widget != NULL) {
        gtk_fixed_remove(GTK_FIXED(w->container), e->widget);
        e->widget = NULL;
    }
    if (e->css_provider != NULL) {
        gtk_style_context_remove_provider_for_display(
            gdk_display_get_default(), GTK_STYLE_PROVIDER(e->css_provider));
        g_object_unref(e->css_provider);
        e->css_provider = NULL;
    }

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
    if (index < 0 || index >= w->entity_count) return;

    Entity *e = &w->entities[index];
    if (e->state == ENTITY_ALIVE) {
        e->state = ENTITY_DYING;
        if (w->alive_counts[e->species] > 0) {
            w->alive_counts[e->species]--;
        }
        e->death_timer = DEATH_FLASH_DURATION;
    }
}

void world_populate(World *w) {
    for (int s = 0; s < species_count(); s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (cfg == 0) continue;

        int target = w->target_counts[s];
        if (target <= 0) continue;

        int clusters = 1;
        if (target >= 18)     clusters = 4;
        else if (target >= 8) clusters = 2;

        Vec2 centers[4];
        for (int c = 0; c < clusters; c++) centers[c] = random_position(w);

        for (int i = 0; i < target; i++) {
            Vec2 center = centers[i % clusters];
            Vec2 p = {
                center.x + rand_range(-90.0, 90.0),
                center.y + rand_range(-70.0, 70.0),
            };
            p = vec2_wrap(p, w->width, w->height);
            Vec2 v = random_velocity_for(cfg);
            world_spawn(w, (SpeciesKind) s, p, v);
        }
    }
}

static void world_handle_eating(World *w) {
    for (int i = 0; i < w->entity_count; i++) {
        Entity *pred = &w->entities[i];
        if (pred->state != ENTITY_ALIVE) continue;
        const SpeciesConfig *pred_cfg = species_get(pred->species);
        if (pred_cfg == 0 || pred_cfg->prey_mask == 0) continue;

        Vec2 pred_mouth = entity_head_point(pred, pred_cfg);

        for (int j = 0; j < w->entity_count; j++) {
            if (i == j) continue;
            Entity *prey = &w->entities[j];
            if (prey->state != ENTITY_ALIVE || !EATS(*pred_cfg, prey->species)) continue;

            const SpeciesConfig *prey_cfg = species_get(prey->species);
            if (prey_cfg == 0) continue;

            double prey_hit = ((double) (prey_cfg->sprite_width < prey_cfg->sprite_height ? prey_cfg->sprite_width : prey_cfg->sprite_height)) * 0.28;
            if (prey_hit < 8.0) prey_hit = 8.0;
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
        if (e->state != ENTITY_ALIVE) continue;
        const SpeciesConfig *cfg = species_get(e->species);
        if (cfg != 0 && cfg->lifespan > 0.0 && e->age > cfg->lifespan) {
            world_despawn(w, i);
        }
    }
}

static void world_sync_target_counts(World *w) {
    for (int s = 0; s < species_count(); s++) {
        const SpeciesConfig *cfg = species_get((SpeciesKind) s);
        if (!cfg) continue;

        int target = w->target_counts[s];
        int alive  = w->alive_counts[s];

        /* Need more: spawn one per tick to avoid lag spikes */
        if (alive < target && w->alive_counts[s] < MAX_ENTITIES) {
            Vec2 p = random_position(w);
            Vec2 v = random_velocity_for(cfg);
            world_spawn(w, (SpeciesKind) s, p, v);
        }
        /* Need fewer: despawn the oldest one */
        else if (alive > target) {
            for (int i = 0; i < w->entity_count; i++) {
                Entity *e = &w->entities[i];
                if (e->state == ENTITY_ALIVE && e->species == (SpeciesKind) s) {
                    world_despawn(w, i);
                    break;
                }
            }
        }
    }
}

void world_tick(World *w) {
    w->elapsed += w->dt;

    world_sync_target_counts(w);

    for (int i = 0; i < w->entity_count; i++) {
        entity_tick(w, i);
    }

    world_handle_eating(w);
    world_handle_lifespans(w);

    for (int i = 0; i < w->entity_count; i++) {
        entity_apply_visuals(w, i);
    }
}

gboolean world_tick_cb(gpointer data) {
    World *w = (World *) data;
    world_tick(w);
    return G_SOURCE_CONTINUE;
}
