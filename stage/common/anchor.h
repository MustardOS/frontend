#pragma once

#include "common.h"

#define OVERLAY_ANCHOR OVERLAY_CONFIG "gen_anchor"

struct anchor_cache {
    const char *path;
    time_t mtime;
    int value;
};

extern int overlay_anchor_cached;

extern struct anchor_cache overlay_anchor_cache;

enum overlay_anchor {
    ANCHOR_TOP_LEFT = 0,
    ANCHOR_TOP_MIDDLE = 1,
    ANCHOR_TOP_RIGHT = 2,
    ANCHOR_CENTRE_LEFT = 3,
    ANCHOR_CENTRE_MIDDLE = 4,
    ANCHOR_CENTRE_RIGHT = 5,
    ANCHOR_BOTTOM_LEFT = 6,
    ANCHOR_BOTTOM_MIDDLE = 7,
    ANCHOR_BOTTOM_RIGHT = 8
};

int get_anchor_cached(struct anchor_cache *cache);
