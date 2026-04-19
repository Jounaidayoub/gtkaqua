# Architecture

## High-Level Overview

The application is a GTK4-driven simulation with a fixed timestep.

- `src/main.c`: GTK app/window setup and main tick scheduling
- `src/world.c`: world state orchestration (spawn/update/eat/respawn)
- `src/entity.c`: per-entity steering and visual updates
- `src/species.c`: species registry and tuning values
- `include/*.h`: shared data structures and constants

## Main Loop

`main.c` schedules updates with `g_timeout_add(TICK_MS, ...)`.

Each tick:

1. container dimensions are refreshed
2. `world_tick(&world)` runs one simulation step

## World Responsibilities (`world.c`)

- `world_init`: initialize world fields and counters
- `world_populate`: initial spawn by species `flock_size`
- `world_tick` order:
  1. update all alive entities (`entity_tick`)
  2. process eating
  3. process lifespan deaths
  4. apply visual transforms/placement (`entity_apply_visuals`)
  5. process respawns

## Entity Responsibilities (`entity.c`)

For each alive entity:

- accumulate steering forces:
  - same-species boids (separation/alignment/cohesion)
  - flee force from predators
  - hunt force toward prey
  - same-species avoidance (config-driven)
- integrate acceleration -> velocity -> position
- clamp speeds using per-entity speed variance
- apply toroidal wrap
- smooth rotation toward velocity heading

Visual phase:

- compute head and center placement
- clamp draw position to container bounds
- `gtk_fixed_move` widget
- update per-entity CSS transform (rotation, death flash)

## Data Model

### `World`

Stored in `include/world.h`.

- fixed-size entity pool (`MAX_ENTITIES`)
- alive counters per species
- simulation area (`width`, `height`)
- timing (`dt`, `elapsed`)

### `Entity`

Stored in `include/entity.h`.

- identity: id/species
- physics: pos/vel/acc/angle
- lifecycle state: `ENTITY_ALIVE`, `ENTITY_DYING`, `ENTITY_INACTIVE`
- GTK refs: widget and css provider

### `SpeciesConfig`

Stored in `include/species.h`, populated in `src/species.c`.

- sprite config (asset path, dimensions)
- movement and boids weights/radii
- predator/prey behavior
- same-species avoid behavior
- mouth hitpoint ratio
- ecosystem settings (lifespan/flock/respawn)

## Rendering Model

- each fish is a `GtkPicture` inside `GtkFixed`
- world coordinates are center-based
- visual rotation uses CSS `transform: rotate(...)`
- death effect is CSS pulse + opacity during `ENTITY_DYING`

## Why This Structure

- behavior tuning is centralized and data-driven in `SpeciesConfig`
- simulation update and view update are separated cleanly
- avoids species-specific ad hoc patches in logic flow
