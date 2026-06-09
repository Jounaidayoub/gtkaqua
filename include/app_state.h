#ifndef APP_STATE_H
#define APP_STATE_H

#include <gtk/gtk.h>
#include <stdbool.h>

#include "config.h"
#include "world.h"

typedef struct {
    GtkWidget       *window;
    GtkWidget       *root_box;
    GtkWidget       *sidebar;
    GtkWidget       *main_area;
    GtkWidget       *dashboard;
    GtkWidget       *overlay;
    GtkWidget       *background_picture;
    GtkMediaStream  *background_stream;
    GtkWidget       *container;
    GtkWidget       *debug_overlay;
    bool             fullscreen;
    bool             debug_mode;
    World            world;
    GtkWidget       *dash_labels[MAX_SPECIES];
} AppState;

#endif
