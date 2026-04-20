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

typedef struct World {
    /* Entity storage */
    Entity entities[MAX_ENTITIES];
    int entity_count;

    /* Per-species alive counters (used by respawn logic) */
    int alive_counts[SPECIES_COUNT];

    /* Simulation bounds */
    double width;
    double height;

    /* GTK container that holds all sprite widgets */
    GtkWidget *container;

    /* Timing and ids */
    double dt;
    double elapsed;
    int next_id;

    /* Next time each species is allowed to respawn */
    double respawn_ready[SPECIES_COUNT];

    /* Draw physics overlays (vectors, bounds, edges) when true */
    bool debug_mode;
} World;

void world_init(World *w, GtkWidget *container, double width, double height);
void world_tick(World *w);
int world_spawn(World *w, SpeciesKind kind, Vec2 pos, Vec2 vel);
void world_despawn(World *w, int index);
void world_populate(World *w);

gboolean world_tick_cb(gpointer data);

#endif
