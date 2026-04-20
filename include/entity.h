#ifndef ENTITY_H
#define ENTITY_H

/*
 * Entity-level data model:
 * Runtime instance of one creature in the world, including physics state,
 * lifecycle state, and GTK widget handles.
 */

#include <gtk/gtk.h>
#include <stdbool.h>

#include "species.h"
#include "vec2.h"

typedef enum {
    /* Active and simulated this tick. */
    ENTITY_ALIVE,
    /* Recently killed; renders death flash before becoming inactive. */
    ENTITY_DYING,
    /* Hidden/inactive; eligible for respawn. */
    ENTITY_INACTIVE
} EntityState;

typedef struct {
    /* Identity */
    int id;
    SpeciesKind species;

    /* Physics */
    Vec2 pos;
    Vec2 vel;
    Vec2 acc;

    /* Facing direction in degrees (smoothed) */
    double angle;

    /* Lifecycle */
    double age;
    EntityState state;

    /* GTK widget state */
    GtkWidget *widget;
    GtkCssProvider *css_provider;
    char css_class[32];

    /* Respawn/death visuals and individual variation */
    double respawn_at;
    double speed_scale;
    double death_timer;
} Entity;

struct World;

void entity_tick(struct World *w, int index);
void entity_apply_visuals(struct World *w, int index);
Vec2 entity_head_point(const Entity *e, const SpeciesConfig *cfg);
gboolean entity_bounds_rect(const struct World *w, const Entity *e, double *x, double *y, double *width, double *height);

#endif
