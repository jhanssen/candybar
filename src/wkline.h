#ifndef WKLINE_H
#define WKLINE_H

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <jansson.h>
#include <string.h>
#include <unistd.h>
#include <webkit/webkit.h>

#include "util/config.h"
#include "util/copy_prop.h"
#include "util/gdk_helpers.h"
#include "util/log.h"

typedef enum {
	WKLINE_POSITION_TOP,
	WKLINE_POSITION_BOTTOM,
} wkline_position_t;

struct wkline {
	wkline_position_t position;
	int width;
	int height;
	int screen;
	const char *theme_uri;
	json_t *config;
};

#endif
