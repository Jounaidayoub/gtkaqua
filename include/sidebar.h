#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <gtk/gtk.h>
#include "app_state.h"

GtkWidget *sidebar_make_label(const char *text, bool bold);
void sidebar_build(AppState *app);
void sidebar_refresh(AppState *app);

#endif
