# Simulation and Tuning

## Simulation Rules

Per tick, active entities follow this behavior stack:

1. boids with same species
2. flee from nearby predators
3. hunt nearest valid prey
4. avoid same-species crowding (if configured)
5. move and rotate
6. be eaten, die by age, or respawn later

## Boids Components

Configured per species:

- `separation_radius`, `separation_weight`
- `alignment_radius`, `alignment_weight`
- `cohesion_radius`, `cohesion_weight`

Use larger radii for more coherent schools and larger separation weight for stronger splitting.

## Predator / Prey

- `prey_mask` defines what a species can eat
- `hunt_radius`, `hunt_weight` control aggression and chase strength
- `fear_radius`, `fear_weight` control panic distance/urgency
- `ADRENALINE_BOOST` increases escape speed while fleeing

## Eating Geometry

Eating is not center-center collision.

- predator uses mouth point derived from heading
- mouth projection distance uses `mouth_forward_ratio`
- prey contributes hit radius from sprite size

Tune `mouth_forward_ratio` per species if mouth feel is too early/late.

## Same-Species Avoidance

Use these fields to avoid predator clumping or schooling overlap:

- `avoid_same_radius`
- `avoid_same_weight`

Set to zero to disable for a species.

## Lifecycle

Entity states:

- `ENTITY_ALIVE` - simulated normally
- `ENTITY_DYING` - death flash window
- `ENTITY_INACTIVE` - hidden, waiting for respawn

Respawn uses:

- `flock_size` as target alive count
- `respawn_delay` between respawn opportunities

## Tuning Workflow

1. change one species at a time in `src/species.c`
2. rebuild and run
3. observe at least 30-60 seconds
4. adjust one axis only (speed, radii, weights, or populations)

## Practical Suggestions

- If schools feel too rigid: lower alignment weight slightly.
- If predators fail to catch prey: increase hunt radius or mouth ratio.
- If predators stack on each other: increase avoid radius/weight.
- If scenes feel empty: raise `flock_size` and lower `respawn_delay`.
