#ifndef CONF_H
#define CONF_H

#include "species.h"

typedef struct {
    double eat_radius;
    double adrenaline_boost;
    double rotation_smooth;
    double death_flash_duration;
    double starvation_seconds;
    int start_width;
    int start_height;
    int start_fullscreen;
    int tick_ms;
    char assets_dir[256];
} GlobalConfig;

typedef struct {
    GlobalConfig global;
    SpeciesConfig species[MAX_SPECIES];
    int species_count;
    char last_error[512];
} AquariumConfig;

int conf_load(AquariumConfig *cfg, const char *override_path);
const char *conf_error(const AquariumConfig *cfg);

#endif
