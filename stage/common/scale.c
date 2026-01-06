#include <sys/stat.h>
#include "scale.h"

int overlay_scale_cached = -1;

struct scale_cache overlay_scale_cache = {
        .path  = OVERLAY_SCALE,
        .mtime = 0,
        .value = SCALE_ORIGINAL
};

static inline int clamp_scale(int v) {
    return (v >= 0 && v <= 2) ? v : SCALE_ORIGINAL;
}

int get_scale_cached(struct scale_cache *cache) {
    struct stat st;

    if (stat(cache->path, &st) != 0) {
        cache->mtime = 0;
        cache->value = SCALE_ORIGINAL;
        return cache->value;
    }

    if (st.st_mtime == cache->mtime) return cache->value;
    cache->mtime = st.st_mtime;

    char buf[8];
    if (!read_line_from_file(cache->path, 1, buf, sizeof(buf))) {
        cache->value = SCALE_ORIGINAL;
        return cache->value;
    }

    cache->value = clamp_scale(safe_atoi(buf));
    return cache->value;
}
