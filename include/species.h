#ifndef SPECIES_H
#define SPECIES_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_SPECIES 64
#define SPECIES_KEY_MAX 64
#define SPECIES_NAME_MAX 64
#define SPECIES_ASSET_MAX 256

typedef int SpeciesKind;

typedef struct {
    char key[SPECIES_KEY_MAX];
    char name[SPECIES_NAME_MAX];
    char asset_path[SPECIES_ASSET_MAX];

    int sprite_width;
    int sprite_height;
    double mouth_forward_ratio;

    double max_speed;
    double min_speed;
    double max_force;

    double separation_radius;
    double alignment_radius;
    double cohesion_radius;
    double separation_weight;
    double alignment_weight;
    double cohesion_weight;

    double fear_radius;
    double fear_weight;
    double hunt_radius;
    double hunt_weight;

    double avoid_same_radius;
    double avoid_same_weight;

    uint64_t prey_mask;
    double lifespan;
    int flock_size;
    double respawn_delay;
} SpeciesConfig;

#define EATS(predator_cfg, prey_kind) ((predator_cfg).prey_mask & (1ULL << (uint64_t) (prey_kind)))

void species_load_defaults(void);
bool species_load_toml(const char *path);
int species_count(void);
const SpeciesConfig *species_get(SpeciesKind kind);
SpeciesConfig *species_get_mut(SpeciesKind kind);
SpeciesKind species_find_key(const char *key);

#endif
