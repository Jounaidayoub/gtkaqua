#include "entity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "world.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double rand_range(double min_v, double max_v) {
    double t = (double) rand() / (double) RAND_MAX;
    return min_v + (max_v - min_v) * t;
}

static double clamp_double(double v, double min_v, double max_v) {
    if (v < min_v) {
        return min_v;
    }
    if (v > max_v) {
        return max_v;
    }
    return v;
}

gboolean entity_bounds_rect(const struct World *w, const Entity *e, double *x, double *y, double *width, double *height) {
    if (w == 0 || e == 0 || x == 0 || y == 0 || width == 0 || height == 0) {
        return FALSE;
    }

    const SpeciesConfig *cfg = species_get(e->species);
    if (cfg == 0) {
        return FALSE;
    }

    Vec2 head = entity_head_point(e, cfg);
    Vec2 dir = vec2_normalize(e->vel);
    if (vec2_len_sq(dir) < 1e-8) {
        double rad = (e->angle - 180.0) * (M_PI / 180.0);
        dir = (Vec2) {cos(rad), sin(rad)};
    }

    double head_offset = ((double) cfg->sprite_width * cfg->mouth_forward_ratio);
    Vec2 center = vec2_sub(head, vec2_scale(dir, head_offset));

    double draw_x = center.x - ((double) cfg->sprite_width * 0.5);
    double draw_y = center.y - ((double) cfg->sprite_height * 0.5);
    double max_x = w->width - (double) cfg->sprite_width;
    double max_y = w->height - (double) cfg->sprite_height;
    if (max_x < 0.0) {
        max_x = 0.0;
    }
    if (max_y < 0.0) {
        max_y = 0.0;
    }
    draw_x = clamp_double(draw_x, 0.0, max_x);
    draw_y = clamp_double(draw_y, 0.0, max_y);

    *x = draw_x;
    *y = draw_y;
    *width = (double) cfg->sprite_width;
    *height = (double) cfg->sprite_height;
    return TRUE;
}

static double heading_deg_from_vel(Vec2 vel) {
    return atan2(vel.y, vel.x) * (180.0 / M_PI) + 180.0;
}

Vec2 entity_head_point(const Entity *e, const SpeciesConfig *cfg) {
    Vec2 dir = vec2_normalize(e->vel);
    if (vec2_len_sq(dir) < 1e-8) {
        double rad = (e->angle - 180.0) * (M_PI / 180.0);
        dir = (Vec2) {cos(rad), sin(rad)};
    }
    double forward = ((double) cfg->sprite_width * cfg->mouth_forward_ratio);
    return vec2_add(e->pos, vec2_scale(dir, forward));
}

static Vec2 steer_toward(Vec2 current_vel, Vec2 desired_vel, double max_force) {
    Vec2 steer = vec2_sub(desired_vel, current_vel);
    return vec2_limit(steer, max_force);
}

static double entity_max_speed(const Entity *e, const SpeciesConfig *cfg) {
    return cfg->max_speed * e->speed_scale;
}

static double entity_min_speed(const Entity *e, const SpeciesConfig *cfg) {
    return cfg->min_speed * e->speed_scale;
}

static Vec2 force_boids(World *w, int index) {
    Entity *self = &w->entities[index];
    const SpeciesConfig *cfg = species_get(self->species);

    Vec2 separation = {0.0, 0.0};
    Vec2 alignment = {0.0, 0.0};
    Vec2 cohesion_sum = {0.0, 0.0};
    int sep_count = 0;
    int ali_count = 0;
    int coh_count = 0;

    for (int i = 0; i < w->entity_count; i++) {
        if (i == index) {
            continue;
        }
        Entity *other = &w->entities[i];
        if (other->state != ENTITY_ALIVE || other->species != self->species) {
            continue;
        }

        Vec2 d = vec2_delta_wrap(self->pos, other->pos, w->width, w->height);
        double dist_sq = vec2_len_sq(d);
        if (dist_sq < 1e-6) {
            continue;
        }

        if (dist_sq < cfg->separation_radius * cfg->separation_radius) {
            Vec2 away = vec2_scale(d, -1.0 / dist_sq);
            separation = vec2_add(separation, away);
            sep_count++;
        }

        if (dist_sq < cfg->alignment_radius * cfg->alignment_radius) {
            alignment = vec2_add(alignment, other->vel);
            ali_count++;
        }

        if (dist_sq < cfg->cohesion_radius * cfg->cohesion_radius) {
            Vec2 local = vec2_add(self->pos, d);
            cohesion_sum = vec2_add(cohesion_sum, local);
            coh_count++;
        }
    }

    Vec2 out = {0.0, 0.0};

    if (sep_count > 0) {
        separation = vec2_scale(separation, 1.0 / (double) sep_count);
        Vec2 desired = vec2_scale(vec2_normalize(separation), entity_max_speed(self, cfg));
        Vec2 steer = steer_toward(self->vel, desired, cfg->max_force);
        out = vec2_add(out, vec2_scale(steer, cfg->separation_weight));
    }

    if (ali_count > 0) {
        alignment = vec2_scale(alignment, 1.0 / (double) ali_count);
        Vec2 desired = vec2_scale(vec2_normalize(alignment), entity_max_speed(self, cfg));
        Vec2 steer = steer_toward(self->vel, desired, cfg->max_force);
        out = vec2_add(out, vec2_scale(steer, cfg->alignment_weight));
    }

    if (coh_count > 0) {
        Vec2 center = vec2_scale(cohesion_sum, 1.0 / (double) coh_count);
        Vec2 to_center = vec2_sub(center, self->pos);
        Vec2 desired = vec2_scale(vec2_normalize(to_center), entity_max_speed(self, cfg));
        Vec2 steer = steer_toward(self->vel, desired, cfg->max_force);
        out = vec2_add(out, vec2_scale(steer, cfg->cohesion_weight));
    }

    return out;
}

static Vec2 force_flee(World *w, int index, gboolean *is_fleeing) {
    Entity *self = &w->entities[index];
    const SpeciesConfig *cfg = species_get(self->species);
    Vec2 fear = {0.0, 0.0};
    int fear_count = 0;

    if (cfg->fear_radius <= 0.0 || cfg->fear_weight <= 0.0) {
        *is_fleeing = FALSE;
        return fear;
    }

    for (int i = 0; i < w->entity_count; i++) {
        if (i == index) {
            continue;
        }
        Entity *other = &w->entities[i];
        if (other->state != ENTITY_ALIVE) {
            continue;
        }

        const SpeciesConfig *other_cfg = species_get(other->species);
        if (!EATS(*other_cfg, self->species)) {
            continue;
        }

        Vec2 d = vec2_delta_wrap(self->pos, other->pos, w->width, w->height);
        double dist_sq = vec2_len_sq(d);
        if (dist_sq >= cfg->fear_radius * cfg->fear_radius || dist_sq < 1e-8) {
            continue;
        }

        Vec2 away = vec2_scale(d, -1.0 / dist_sq);
        fear = vec2_add(fear, away);
        fear_count++;
    }

    if (fear_count == 0) {
        *is_fleeing = FALSE;
        return fear;
    }

    fear = vec2_scale(fear, 1.0 / (double) fear_count);
    Vec2 desired = vec2_scale(vec2_normalize(fear), entity_max_speed(self, cfg) * ADRENALINE_BOOST);
    Vec2 steer = steer_toward(self->vel, desired, cfg->max_force * ADRENALINE_BOOST);
    *is_fleeing = TRUE;
    return vec2_scale(steer, cfg->fear_weight);
}

static Vec2 force_hunt(World *w, int index) {
    Entity *self = &w->entities[index];
    const SpeciesConfig *cfg = species_get(self->species);
    if (cfg->hunt_radius <= 0.0 || cfg->hunt_weight <= 0.0 || cfg->prey_mask == 0) {
        Vec2 z = {0.0, 0.0};
        return z;
    }

    int nearest = -1;
    double nearest_dist_sq = cfg->hunt_radius * cfg->hunt_radius;

    for (int i = 0; i < w->entity_count; i++) {
        if (i == index) {
            continue;
        }
        Entity *other = &w->entities[i];
        if (other->state != ENTITY_ALIVE || !EATS(*cfg, other->species)) {
            continue;
        }

        double dist_sq = vec2_dist_sq_wrap(self->pos, other->pos, w->width, w->height);
        if (dist_sq < nearest_dist_sq) {
            nearest_dist_sq = dist_sq;
            nearest = i;
        }
    }

    if (nearest < 0) {
        Vec2 z = {0.0, 0.0};
        return z;
    }

    Vec2 delta = vec2_delta_wrap(self->pos, w->entities[nearest].pos, w->width, w->height);
    Vec2 desired = vec2_scale(vec2_normalize(delta), entity_max_speed(self, cfg));
    Vec2 steer = steer_toward(self->vel, desired, cfg->max_force);
    return vec2_scale(steer, cfg->hunt_weight);
}

static Vec2 force_same_species_avoid(World *w, int index) {
    Entity *self = &w->entities[index];
    const SpeciesConfig *cfg = species_get(self->species);
    if (cfg == 0 || cfg->avoid_same_radius <= 0.0 || cfg->avoid_same_weight <= 0.0) {
        return (Vec2) {0.0, 0.0};
    }

    Vec2 away = {0.0, 0.0};
    int count = 0;

    for (int i = 0; i < w->entity_count; i++) {
        if (i == index) {
            continue;
        }
        Entity *other = &w->entities[i];
        if (other->state != ENTITY_ALIVE || other->species != self->species) {
            continue;
        }

        Vec2 d = vec2_delta_wrap(self->pos, other->pos, w->width, w->height);
        double dist_sq = vec2_len_sq(d);
        if (dist_sq < 1e-6 || dist_sq > cfg->avoid_same_radius * cfg->avoid_same_radius) {
            continue;
        }

        Vec2 push = vec2_scale(d, -1.0 / dist_sq);
        away = vec2_add(away, push);
        count++;
    }

    if (count == 0) {
        return (Vec2) {0.0, 0.0};
    }

    away = vec2_scale(away, 1.0 / (double) count);
    Vec2 desired = vec2_scale(vec2_normalize(away), entity_max_speed(self, cfg));
    Vec2 steer = steer_toward(self->vel, desired, cfg->max_force * 1.5);
    return vec2_scale(steer, cfg->avoid_same_weight);
}

void entity_tick(struct World *w, int index) {
    Entity *e = &w->entities[index];
    const SpeciesConfig *cfg = species_get(e->species);
    if (e->state != ENTITY_ALIVE || cfg == 0) {
        return;
    }

    e->age += w->dt;
    e->acc = (Vec2) {0.0, 0.0};

    e->acc = vec2_add(e->acc, force_boids(w, index));

    gboolean fleeing = FALSE;
    e->acc = vec2_add(e->acc, force_flee(w, index, &fleeing));
    e->acc = vec2_add(e->acc, force_hunt(w, index));
    e->acc = vec2_add(e->acc, force_same_species_avoid(w, index));

    e->acc = vec2_limit(e->acc, cfg->max_force * 4.0);
    e->vel = vec2_add(e->vel, vec2_scale(e->acc, w->dt));

    double speed_cap = entity_max_speed(e, cfg);
    if (fleeing) {
        speed_cap *= ADRENALINE_BOOST;
    }
    e->vel = vec2_limit(e->vel, speed_cap);

    double speed_sq = vec2_len_sq(e->vel);
    double min_speed = entity_min_speed(e, cfg);
    if (speed_sq < min_speed * min_speed) {
        double speed = sqrt(speed_sq);
        Vec2 dir = speed > 1e-6 ? vec2_scale(e->vel, 1.0 / speed) : vec2_random_unit();
        e->vel = vec2_scale(dir, rand_range(entity_min_speed(e, cfg), speed_cap));
    }

    e->pos = vec2_add(e->pos, vec2_scale(e->vel, w->dt));
    e->pos = vec2_wrap(e->pos, w->width, w->height);

    double target_deg = heading_deg_from_vel(e->vel);
    double diff = target_deg - e->angle;
    while (diff > 180.0) {
        diff -= 360.0;
    }
    while (diff < -180.0) {
        diff += 360.0;
    }
    e->angle += diff * ROTATION_SMOOTH;
}

void entity_apply_visuals(struct World *w, int index) {
    Entity *e = &w->entities[index];
    if (e->widget == 0) {
        return;
    }

    if (e->state == ENTITY_INACTIVE) {
        gtk_widget_set_visible(e->widget, FALSE);
        return;
    }

    double draw_x = 0.0;
    double draw_y = 0.0;
    double draw_w = 0.0;
    double draw_h = 0.0;
    if (!entity_bounds_rect(w, e, &draw_x, &draw_y, &draw_w, &draw_h)) {
        return;
    }

    gtk_fixed_move(GTK_FIXED(w->container), e->widget, draw_x, draw_y);

    if (e->css_provider != 0) {
        char css[224];
        if (e->state == ENTITY_DYING) {
            double pulse = fmod(e->death_timer * 24.0, 2.0) < 1.0 ? 1.0 : 0.0;
            double scale = 1.15 + (0.35 * pulse);
            double opacity = 0.35 + (0.55 * pulse);
            snprintf(
                css,
                sizeof(css),
                ".%s { transform: rotate(%.1fdeg) scale(%.2f); opacity: %.2f; }",
                e->css_class,
                e->angle,
                scale,
                opacity
            );
            e->death_timer -= w->dt;
            if (e->death_timer <= 0.0) {
                e->death_timer = 0.0;
                e->state = ENTITY_INACTIVE;
                gtk_widget_set_visible(e->widget, FALSE);
            }
        } else {
            snprintf(css, sizeof(css), ".%s { transform: rotate(%.1fdeg); opacity: 1.0; }", e->css_class, e->angle);
        }
        gtk_css_provider_load_from_string(e->css_provider, css);
    }
}
