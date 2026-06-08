#include "species.h"

#include <string.h>

static SpeciesConfig g_species[MAX_SPECIES];
static int g_species_count = 0;

int species_count(void) {
    return g_species_count;
}

const SpeciesConfig *species_get(int index) {
    if (index < 0 || index >= g_species_count) return 0;
    return &g_species[index];
}

int species_find_by_id(const char *id) {
    for (int i = 0; i < g_species_count; i++) {
        if (strcmp(g_species[i].id, id) == 0) return i;
    }
    return -1;
}

void species_init(const SpeciesConfig *configs, int count) {
    if (count > MAX_SPECIES) count = MAX_SPECIES;
    g_species_count = count;
    for (int i = 0; i < count; i++) {
        g_species[i] = configs[i];
    }
}

void species_destroy(void) {
    g_species_count = 0;
}
