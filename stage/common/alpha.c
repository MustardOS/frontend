#include <sys/stat.h>
#include "alpha.h"

struct alpha_cache overlay_alpha_cache = {
        .path  = OVERLAY_ALPHA,
        .mtime = 0,
        .value = 1.0f
};

static float clamp_alpha(int v) {
    if (v < 0) return 0.0f;
    if (v > 255) return 1.0f;

    return (float) v / 255.0f;
}

float get_alpha_cached(struct alpha_cache *cache) {
    struct stat st;

    if (stat(cache->path, &st) != 0) {
        cache->mtime = 0;
        cache->value = 1.0f;
        return cache->value;
    }

    if (st.st_mtime == cache->mtime) return cache->value;
    cache->mtime = st.st_mtime;

    char buf[8];
    if (!read_line_from_file(cache->path, 1, buf, sizeof(buf))) {
        cache->value = 1.0f;
        return cache->value;
    }

    cache->value = clamp_alpha(safe_atoi(buf));
    return cache->value;
}
