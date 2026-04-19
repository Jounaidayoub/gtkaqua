# Getting Started

## Prerequisites

Install these tools/packages:

- C toolchain (`cc`, `make`)
- GTK4 development package (`pkg-config gtk4` must resolve)
- `ffmpeg` for asset conversion
- optional: `bear` for `compile_commands.json`

## Project Structure

```
gtkaqua/
├── Makefile
├── include/
├── src/
├── Fish/
├── assets/
├── build/
└── docs/
```

## First Build

1. Generate runtime assets:

```bash
make assets
```

2. Build:

```bash
make
```

3. Run:

```bash
./build/bin/aquarium
```

## Common Commands

- Build app: `make`
- Rebuild assets: `make assets`
- Clean: `make clean`
- Generate compile database: `make bear`

## Runtime Controls

- `F11` toggles fullscreen
- `Esc` exits fullscreen

## Typical Development Loop

```bash
make assets
make
./build/bin/aquarium
```

Use `make bear` when you need clangd/editor indexing refresh.
