#pragma once

#include "common.h"
#include "overlay.h"

#define OVERLAY_ALPHA OVERLAY_CONFIG "gen_alpha"

struct alpha_cache {
    const char *path;
    time_t mtime;
    float value;
    enum overlay_option_field field;
};

extern struct alpha_cache overlay_alpha_cache;

float get_alpha_cached(struct alpha_cache *cache);
