#ifndef SPECIES_H
#define SPECIES_H

#include <stdbool.h>

#define MAX_SPECIES 32

typedef struct {
    char id[64];
    char name[64];
    char asset_path[256];
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
    int prey_mask;
    double lifespan;
    int flock_size;
    int max_population;
    double respawn_delay;
    bool is_apex;
} SpeciesConfig;

#define EATS(predator_cfg, prey_index) ((predator_cfg).prey_mask & (1 << (prey_index)))

int species_count(void);
const SpeciesConfig *species_get(int index);
int species_find_by_id(const char *id);
void species_init(const SpeciesConfig *configs, int count);
void species_destroy(void);

#endif
