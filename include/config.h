#ifndef CONFIG_H
#define CONFIG_H

/*
 * Global simulation/app constants.
 * These are compile-time knobs that affect timing, visuals, and startup behavior.
 */

#define MAX_ENTITIES 256
#define TICK_MS 16
#define DT (TICK_MS / 1000.0)

#define EAT_RADIUS 10.0
#define ADRENALINE_BOOST 2.5
#define ROTATION_SMOOTH 0.1
#define DEATH_FLASH_DURATION 0.22

#define ASSETS_DIR "assets/"

#define START_WIDTH 1280
#define START_HEIGHT 720
#define START_FULLSCREEN 0

/* How long (seconds) a fish can go without eating before starving */
#define STARVATION_SECONDS 120.0

#endif
