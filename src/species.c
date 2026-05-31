#include "species.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "toml.h"

static SpeciesConfig SPECIES[MAX_SPECIES];
static int SPECIES_N = 0;

static uint64_t mask_for(int kind) {
    return (kind >= 0 && kind < 64) ? (1ULL << (uint64_t) kind) : 0;
}

static void add_default(
    const char *key,
    const char *name,
    const char *asset_path,
    int sprite_width,
    int sprite_height,
    double mouth_forward_ratio,
    double max_speed,
    double min_speed,
    double max_force,
    double separation_radius,
    double alignment_radius,
    double cohesion_radius,
    double separation_weight,
    double alignment_weight,
    double cohesion_weight,
    double fear_radius,
    double fear_weight,
    double hunt_radius,
    double hunt_weight,
    double avoid_same_radius,
    double avoid_same_weight,
    uint64_t prey_mask,
    double lifespan,
    int flock_size,
    double respawn_delay
) {
    if (SPECIES_N >= MAX_SPECIES) return;
    SpeciesConfig *s = &SPECIES[SPECIES_N++];
    memset(s, 0, sizeof(*s));
    snprintf(s->key, sizeof(s->key), "%s", key);
    snprintf(s->name, sizeof(s->name), "%s", name);
    snprintf(s->asset_path, sizeof(s->asset_path), "%s", asset_path);
    s->sprite_width = sprite_width;
    s->sprite_height = sprite_height;
    s->mouth_forward_ratio = mouth_forward_ratio;
    s->max_speed = max_speed;
    s->min_speed = min_speed;
    s->max_force = max_force;
    s->separation_radius = separation_radius;
    s->alignment_radius = alignment_radius;
    s->cohesion_radius = cohesion_radius;
    s->separation_weight = separation_weight;
    s->alignment_weight = alignment_weight;
    s->cohesion_weight = cohesion_weight;
    s->fear_radius = fear_radius;
    s->fear_weight = fear_weight;
    s->hunt_radius = hunt_radius;
    s->hunt_weight = hunt_weight;
    s->avoid_same_radius = avoid_same_radius;
    s->avoid_same_weight = avoid_same_weight;
    s->prey_mask = prey_mask;
    s->lifespan = lifespan;
    s->flock_size = flock_size;
    s->respawn_delay = respawn_delay;
}

void species_load_defaults(void) {
    SPECIES_N = 0;

    enum { GOLDFISH, CLOWNFISH, ANGELFISH, BARRACUDA, BASS, TROUT, TUNA, SHARK };

    add_default("goldfish", "Goldfish", "assets/goldfish.png", 46, 23, 0.40, 85, 30, 42, 24, 120, 100, 1.6, 1.35, 1.45, 190, 2.5, 0, 0, 0, 0, 0, 120, 45, 1.5);
    add_default("clownfish", "Clownfish", "assets/clownfish.png", 72, 42, 0.40, 90, 32, 45, 34, 115, 125, 1.7, 1.35, 1.45, 195, 2.6, 0, 0, 0, 0, 0, 120, 0, 1.5);
    add_default("angelfish", "Angelfish", "assets/angelfish.png", 30, 39, 0.40, 95, 34, 44, 38, 130, 142, 1.5, 1.25, 1.3, 210, 2.5, 0, 0, 0, 0, 0, 150, 10, 2.0);
    add_default("barracuda", "Barracuda", "assets/barracuda.png", 96, 38, 0.42, 130, 40, 60, 56, 100, 112, 1.3, 0.8, 0.6, 230, 1.5, 300, 2.7, 90, 0.8, mask_for(GOLDFISH) | mask_for(CLOWNFISH), 200, 0, 4.0);
    add_default("bass", "Bass", "assets/bass.png", 60, 32, 0.42, 112, 36, 53, 48, 96, 108, 1.4, 0.8, 0.7, 245, 1.7, 290, 2.4, 80, 0.7, mask_for(GOLDFISH) | mask_for(CLOWNFISH), 180, 5, 4.5);
    add_default("trout", "Trout", "assets/trout.png", 84, 48, 0.42, 110, 36, 52, 44, 94, 106, 1.4, 0.9, 0.7, 240, 1.8, 270, 2.25, 75, 0.7, mask_for(GOLDFISH), 180, 0, 4.5);
    add_default("tuna", "Tuna", "assets/tuna.png", 90, 54, 0.42, 125, 40, 56, 52, 100, 114, 1.3, 0.8, 0.6, 250, 1.7, 320, 2.5, 85, 0.8, mask_for(GOLDFISH) | mask_for(CLOWNFISH), 200, 4, 5.0);
    add_default("shark", "Shark", "assets/shark.png", 190, 96, 0.43, 145, 44, 62, 76, 130, 140, 1.1, 0.6, 0.4, 0, 0, 430, 3.2, 220, 3.4, mask_for(TUNA) | mask_for(BASS) | mask_for(TROUT) | mask_for(ANGELFISH) | mask_for(GOLDFISH), 300, 2, 6.0);
}

int species_count(void) {
    return SPECIES_N;
}

const SpeciesConfig *species_get(SpeciesKind kind) {
    if (kind < 0 || kind >= SPECIES_N) return NULL;
    return &SPECIES[kind];
}

SpeciesConfig *species_get_mut(SpeciesKind kind) {
    if (kind < 0 || kind >= SPECIES_N) return NULL;
    return &SPECIES[kind];
}

SpeciesKind species_find_key(const char *key) {
    if (!key) return -1;
    for (int i = 0; i < SPECIES_N; i++) {
        if (strcmp(SPECIES[i].key, key) == 0) return i;
    }
    return -1;
}

static void read_string(toml_table_t *t, const char *key, char *out, size_t out_sz) {
    toml_datum_t d = toml_string_in(t, key);
    if (!d.ok) return;
    snprintf(out, out_sz, "%s", d.u.s);
    free(d.u.s);
}

static void read_int(toml_table_t *t, const char *key, int *out) {
    toml_datum_t d = toml_int_in(t, key);
    if (d.ok) *out = (int) d.u.i;
}

static void read_double(toml_table_t *t, const char *key, double *out) {
    toml_datum_t d = toml_double_in(t, key);
    if (d.ok) {
        *out = d.u.d;
        return;
    }
    d = toml_int_in(t, key);
    if (d.ok) *out = (double) d.u.i;
}

static void parse_species_table(const char *key, toml_table_t *t, SpeciesConfig *s) {
    memset(s, 0, sizeof(*s));
    snprintf(s->key, sizeof(s->key), "%s", key);
    snprintf(s->name, sizeof(s->name), "%s", key);

    read_string(t, "name", s->name, sizeof(s->name));
    read_string(t, "asset_path", s->asset_path, sizeof(s->asset_path));
    read_int(t, "count", &s->flock_size);
    read_int(t, "sprite_width", &s->sprite_width);
    read_int(t, "sprite_height", &s->sprite_height);
    read_double(t, "mouth_forward_ratio", &s->mouth_forward_ratio);
    read_double(t, "max_speed", &s->max_speed);
    read_double(t, "min_speed", &s->min_speed);
    read_double(t, "max_force", &s->max_force);
    read_double(t, "separation_radius", &s->separation_radius);
    read_double(t, "alignment_radius", &s->alignment_radius);
    read_double(t, "cohesion_radius", &s->cohesion_radius);
    read_double(t, "separation_weight", &s->separation_weight);
    read_double(t, "alignment_weight", &s->alignment_weight);
    read_double(t, "cohesion_weight", &s->cohesion_weight);
    read_double(t, "fear_radius", &s->fear_radius);
    read_double(t, "fear_weight", &s->fear_weight);
    read_double(t, "hunt_radius", &s->hunt_radius);
    read_double(t, "hunt_weight", &s->hunt_weight);
    read_double(t, "avoid_same_radius", &s->avoid_same_radius);
    read_double(t, "avoid_same_weight", &s->avoid_same_weight);
    read_double(t, "lifespan", &s->lifespan);
    read_double(t, "respawn_delay", &s->respawn_delay);
}

bool species_load_toml(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char errbuf[256];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);
    if (!root) {
        fprintf(stderr, "Failed to parse %s: %s\n", path, errbuf);
        return false;
    }

    toml_table_t *species = toml_table_in(root, "species");
    if (!species) {
        toml_free(root);
        return false;
    }

    SpeciesConfig loaded[MAX_SPECIES];
    int loaded_n = 0;

    int n = toml_table_ntab(species);
    for (int i = 0; i < n && loaded_n < MAX_SPECIES; i++) {
        const char *key = toml_key_in(species, i);
        toml_table_t *table = key ? toml_table_in(species, key) : NULL;
        if (!key || !table) continue;
        parse_species_table(key, table, &loaded[loaded_n++]);
    }

    for (int i = 0; i < loaded_n; i++) {
        const char *key = loaded[i].key;
        toml_table_t *table = toml_table_in(species, key);
        toml_array_t *prey = table ? toml_array_in(table, "prey") : NULL;
        if (!prey) continue;
        int prey_n = toml_array_nelem(prey);
        for (int p = 0; p < prey_n; p++) {
            toml_datum_t d = toml_string_at(prey, p);
            if (!d.ok) continue;
            for (int j = 0; j < loaded_n; j++) {
                if (strcmp(loaded[j].key, d.u.s) == 0) {
                    loaded[i].prey_mask |= mask_for(j);
                    break;
                }
            }
            free(d.u.s);
        }
    }

    if (loaded_n > 0) {
        memcpy(SPECIES, loaded, sizeof(SpeciesConfig) * (size_t) loaded_n);
        SPECIES_N = loaded_n;
    }

    toml_free(root);
    return loaded_n > 0;
}
