#pragma once

#include "common.h"

#define OVERLAY_SCALE OVERLAY_CONFIG "gen_scale"

struct scale_cache {
    const char *path;
    time_t mtime;
    int value;
};

extern int overlay_scale_cached;

extern struct scale_cache overlay_scale_cache;

enum overlay_scale {
    SCALE_ORIGINAL = 0,
    SCALE_FIT = 1,
    SCALE_STRETCH = 2
};

int get_scale_cached(struct scale_cache *cache);
