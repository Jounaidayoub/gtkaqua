#ifndef SPECIES_H
#define SPECIES_H

/*
 * Species-level data model:
 * Each SpeciesConfig entry defines movement, flocking, predator/prey rules,
 * and population behavior for one species.
 */

typedef enum {
    SPECIES_GOLDFISH,
    SPECIES_CLOWNFISH,
    SPECIES_ANGELFISH,
    SPECIES_BARRACUDA,
    SPECIES_BASS,
    SPECIES_TROUT,
    SPECIES_TUNA,
    SPECIES_SHARK,
    SPECIES_COUNT
} SpeciesKind;

typedef struct {
    /* Identity and sprite metadata */
    SpeciesKind kind;
    const char *name;
    const char *asset_path;
    int sprite_width;
    int sprite_height;
    double mouth_forward_ratio;

    /* Movement model */
    double max_speed;
    double min_speed;
    double max_force;

    /* Boids forces (same-species only) */
    double separation_radius;
    double alignment_radius;
    double cohesion_radius;
    double separation_weight;
    double alignment_weight;
    double cohesion_weight;

    /* Predator/prey steering */
    double fear_radius;
    double fear_weight;
    double hunt_radius;
    double hunt_weight;

    /* Extra same-species spacing (anti-clump) */
    double avoid_same_radius;
    double avoid_same_weight;

    /* Ecosystem and lifecycle */
    int prey_mask;
    double lifespan;
    int flock_size;
    int max_population;
    double respawn_delay;
} SpeciesConfig;

#define EATS(predator_cfg, prey_kind) ((predator_cfg).prey_mask & (1 << (prey_kind)))

const SpeciesConfig *species_get(SpeciesKind kind);

#endif
