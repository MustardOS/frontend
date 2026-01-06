#include <sys/stat.h>
#include "anchor.h"

int overlay_anchor_cached = -1;

struct anchor_cache overlay_anchor_cache = {
        .path  = OVERLAY_ANCHOR,
        .mtime = 0,
        .value = ANCHOR_TOP_LEFT
};

static inline int clamp_anchor(int v) {
    return (v >= 0 && v <= 8) ? v : ANCHOR_TOP_LEFT;
}

int get_anchor_cached(struct anchor_cache *cache) {
    struct stat st;

    if (stat(cache->path, &st) != 0) {
        cache->mtime = 0;
        cache->value = ANCHOR_TOP_LEFT;
        return cache->value;
    }

    if (st.st_mtime == cache->mtime) return cache->value;
    cache->mtime = st.st_mtime;

    char buf[8];
    if (!read_line_from_file(cache->path, 1, buf, sizeof(buf))) {
        cache->value = ANCHOR_TOP_LEFT;
        return cache->value;
    }

    cache->value = clamp_anchor(safe_atoi(buf));
    return cache->value;
}
