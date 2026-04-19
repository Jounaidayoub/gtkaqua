# Assets and Pipeline

## Source vs Runtime Assets

- Raw source assets live in `Fish/`
- Runtime-ready assets live in `assets/`

Always run `make assets` after updating files in `Fish/`.

## Orientation Contract

Runtime simulation assumes all prepared assets in `assets/` face LEFT.

This is enforced in the Makefile pipeline:

- webp species are converted and scaled
- the four png species (`fish1..4`) are flipped with `hflip`

This keeps runtime rotation logic simple and consistent.

## Current Conversion Commands (via `make assets`)

- shark/bass/trout/tuna: webp -> png + scale
- goldfish/clownfish/angelfish/barracuda: png -> hflip -> png

## When Adding New Species Art

1. place source file in `Fish/`
2. add conversion line in `Makefile` under `assets` target
3. ensure final orientation in `assets/` is LEFT-facing
4. add species config entry in `src/species.c`

## Troubleshooting

- If fish appear reversed in motion, check `assets/` orientation first.
- Do not patch orientation per species in runtime unless absolutely unavoidable.
- Re-run `make assets` to restore canonical prepared assets.
