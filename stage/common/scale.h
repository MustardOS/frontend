#pragma once

#include "common.h"
#include "overlay.h"

#define OVERLAY_SCALE OVERLAY_CONFIG "gen_scale"

struct scale_cache {
    const char *path;
    time_t mtime;
    int value;
    enum overlay_option_field field;
};

extern int overlay_scale_cached;

extern struct scale_cache overlay_scale_cache;

enum overlay_scale { scale_original = 0, scale_fit = 1, scale_stretch = 2 };

int get_scale_cached(struct scale_cache *cache);
