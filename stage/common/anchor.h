#pragma once

#include "common.h"
#include "overlay.h"

#define OVERLAY_ANCHOR OVERLAY_CONFIG "gen_anchor"

struct anchor_cache {
    const char *path;
    time_t mtime;
    int value;
    enum overlay_option_field field;
};

extern int overlay_anchor_cached;

extern struct anchor_cache overlay_anchor_cache;

enum overlay_anchor {
    anchor_top_left = 0,
    anchor_top_middle = 1,
    anchor_top_right = 2,
    anchor_centre_left = 3,
    anchor_centre_middle = 4,
    anchor_centre_right = 5,
    anchor_bottom_left = 6,
    anchor_bottom_middle = 7,
    anchor_bottom_right = 8
};

int get_anchor_cached(struct anchor_cache *cache);

int get_anchor_rotate(int anchor, int rot);
