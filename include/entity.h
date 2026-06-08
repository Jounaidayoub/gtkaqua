#ifndef ENTITY_H
#define ENTITY_H

#include <gtk/gtk.h>
#include <stdbool.h>

#include "species.h"
#include "vec2.h"

typedef enum {
    ENTITY_ALIVE,
    ENTITY_DYING,
    ENTITY_INACTIVE
} EntityState;

typedef struct {
    int id;
    int species;
    Vec2 pos;
    Vec2 vel;
    Vec2 acc;
    double angle;
    double age;
    double last_eaten;
    EntityState state;
    GtkWidget *widget;
    GtkCssProvider *css_provider;
    char css_class[32];
    double respawn_at;
    double speed_scale;
    double death_timer;
    int anim_frame;
    double anim_timer;
} Entity;

struct World;

void entity_tick(struct World *w, int index);
void entity_apply_visuals(struct World *w, int index);
Vec2 entity_head_point(const Entity *e, const SpeciesConfig *cfg);

#endif
