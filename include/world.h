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
    int alive_counts[MAX_SPECIES];
    double width;
    double height;
    GtkWidget *container;
    double dt;
    double elapsed;
    int next_id;
    double respawn_ready[MAX_SPECIES];
} World;

void world_init(World *w, GtkWidget *container, double width, double height);
void world_tick(World *w);
int world_spawn(World *w, int species_index, Vec2 pos, Vec2 vel);
void world_despawn(World *w, int index);
void world_populate(World *w);
void world_populate_enabled(World *w, const gboolean *enabled);
void world_clear(World *w);
void world_remove_species(World *w, int species_index, int count);
void world_set_time_scale(World *w, double scale);
void world_remove_last(World *w);
gboolean world_tick_cb(gpointer data);

#endif
