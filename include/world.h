#ifndef WORLD_H
#define WORLD_H

/*
 * World-level data model:
 * Owns the entity pool, simulation dimensions, timing, and per-species
 * population bookkeeping.
 */

#include <gtk/gtk.h>

#include "config.h"
#include "entity.h"
#include "species.h"
#include "vec2.h"

typedef struct {
    double  max_speed;
    double  min_speed;
    double  max_force;
    double  separation_radius;
    double  separation_weight;
    double  alignment_radius;
    double  alignment_weight;
    double  cohesion_radius;
    double  cohesion_weight;
    double  fear_radius;
    double  fear_weight;
    double  hunt_radius;
    double  hunt_weight;
    double  avoid_same_radius;
    double  avoid_same_weight;
    double  lifespan;
    double  respawn_delay;
} EffectiveSpecies;

typedef struct World {
    /* Entity storage */
    Entity entities[MAX_ENTITIES];
    int entity_count;

    /* Live UI configuration */
    int target_counts[MAX_SPECIES];
    EffectiveSpecies effective[MAX_SPECIES];

    /* Per-species alive counters */
    int alive_counts[MAX_SPECIES];

    /* Simulation bounds */
    double width;
    double height;

    /* GTK container that holds all sprite widgets */
    GtkWidget *container;

    /* Timing and ids */
    double dt;
    double elapsed;
    int next_id;

    /* Draw physics overlays (vectors, bounds, edges) when true */
    bool debug_mode;
} World;

void world_init(World *w, GtkWidget *container, double width, double height);
void world_tick(World *w);
int  world_spawn(World *w, SpeciesKind kind, Vec2 pos, Vec2 vel);
void world_despawn(World *w, int index);
void world_populate(World *w);

gboolean world_tick_cb(gpointer data);

#endif
