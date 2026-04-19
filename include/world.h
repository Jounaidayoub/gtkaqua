#ifndef WORLD_H
#define WORLD_H

#include <gtk/gtk.h>

#include "config.h"
#include "entity.h"
#include "species.h"
#include "vec2.h"

typedef struct World {
    Entity entities[MAX_ENTITIES];
    int entity_count;

    int alive_counts[SPECIES_COUNT];

    double width;
    double height;

    GtkWidget *container;

    double dt;
    double elapsed;
    int next_id;

    double respawn_ready[SPECIES_COUNT];
} World;

void world_init(World *w, GtkWidget *container, double width, double height);
void world_tick(World *w);
int world_spawn(World *w, SpeciesKind kind, Vec2 pos, Vec2 vel);
void world_despawn(World *w, int index);
void world_populate(World *w);

gboolean world_tick_cb(gpointer data);

#endif
