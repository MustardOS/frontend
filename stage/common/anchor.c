#include <sys/stat.h>
#include "anchor.h"
#include "rotate.h"

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

int get_anchor_rotate(int anchor, int rot) {
    if (rot == ROTATE_0) return anchor;

    static const int map_90[] = {
            [ANCHOR_TOP_LEFT]      = ANCHOR_TOP_RIGHT,
            [ANCHOR_TOP_MIDDLE]    = ANCHOR_CENTRE_RIGHT,
            [ANCHOR_TOP_RIGHT]     = ANCHOR_BOTTOM_RIGHT,

            [ANCHOR_CENTRE_LEFT]   = ANCHOR_TOP_MIDDLE,
            [ANCHOR_CENTRE_MIDDLE] = ANCHOR_CENTRE_MIDDLE,
            [ANCHOR_CENTRE_RIGHT]  = ANCHOR_BOTTOM_MIDDLE,

            [ANCHOR_BOTTOM_LEFT]   = ANCHOR_TOP_LEFT,
            [ANCHOR_BOTTOM_MIDDLE] = ANCHOR_CENTRE_LEFT,
            [ANCHOR_BOTTOM_RIGHT]  = ANCHOR_BOTTOM_LEFT,
    };

    static const int map_180[] = {
            [ANCHOR_TOP_LEFT]      = ANCHOR_BOTTOM_RIGHT,
            [ANCHOR_TOP_MIDDLE]    = ANCHOR_BOTTOM_MIDDLE,
            [ANCHOR_TOP_RIGHT]     = ANCHOR_BOTTOM_LEFT,

            [ANCHOR_CENTRE_LEFT]   = ANCHOR_CENTRE_RIGHT,
            [ANCHOR_CENTRE_MIDDLE] = ANCHOR_CENTRE_MIDDLE,
            [ANCHOR_CENTRE_RIGHT]  = ANCHOR_CENTRE_LEFT,

            [ANCHOR_BOTTOM_LEFT]   = ANCHOR_TOP_RIGHT,
            [ANCHOR_BOTTOM_MIDDLE] = ANCHOR_TOP_MIDDLE,
            [ANCHOR_BOTTOM_RIGHT]  = ANCHOR_TOP_LEFT,
    };

    static const int map_270[] = {
            [ANCHOR_TOP_LEFT]      = ANCHOR_BOTTOM_LEFT,
            [ANCHOR_TOP_MIDDLE]    = ANCHOR_CENTRE_LEFT,
            [ANCHOR_TOP_RIGHT]     = ANCHOR_TOP_LEFT,

            [ANCHOR_CENTRE_LEFT]   = ANCHOR_BOTTOM_MIDDLE,
            [ANCHOR_CENTRE_MIDDLE] = ANCHOR_CENTRE_MIDDLE,
            [ANCHOR_CENTRE_RIGHT]  = ANCHOR_TOP_MIDDLE,

            [ANCHOR_BOTTOM_LEFT]   = ANCHOR_BOTTOM_RIGHT,
            [ANCHOR_BOTTOM_MIDDLE] = ANCHOR_CENTRE_RIGHT,
            [ANCHOR_BOTTOM_RIGHT]  = ANCHOR_TOP_RIGHT,
    };

    switch (rot) {
        case ROTATE_90:
            return map_90[anchor];
        case ROTATE_180:
            return map_180[anchor];
        case ROTATE_270:
            return map_270[anchor];
        default:
            return anchor;
    }
}
