#include <sys/stat.h>
#include "anchor.h"
#include "rotate.h"

int overlay_anchor_cached = -1;

struct anchor_cache overlay_anchor_cache = {
    .path = OVERLAY_ANCHOR, .mtime = 0, .value = anchor_top_left, .field = opt_gen_anchor
};

static int clamp_anchor(const int v) {
    return v >= 0 && v <= 8 ? v : anchor_top_left;
}

int get_anchor_cached(struct anchor_cache *cache) {
    int override_val;
    if (get_overlay_option_override(cache->field, &override_val)) return clamp_anchor(override_val);

    struct stat st;

    if (stat(cache->path, &st) != 0) {
        cache->mtime = 0;
        cache->value = anchor_top_left;
        return cache->value;
    }

    if (st.st_mtime == cache->mtime) return cache->value;
    cache->mtime = st.st_mtime;

    char buf[8];
    if (!read_line_from_file(cache->path, 1, buf, sizeof(buf))) {
        cache->value = anchor_top_left;
        return cache->value;
    }

    cache->value = clamp_anchor(safe_atoi(buf));
    return cache->value;
}

int get_anchor_rotate(const int anchor, const int rot) {
    if (rot == rotate_0) return anchor;

    static const int map_90[] = {
        [anchor_top_left] = anchor_top_right,         [anchor_top_middle] = anchor_centre_right,
        [anchor_top_right] = anchor_bottom_right,

        [anchor_centre_left] = anchor_top_middle,     [anchor_centre_middle] = anchor_centre_middle,
        [anchor_centre_right] = anchor_bottom_middle,

        [anchor_bottom_left] = anchor_top_left,       [anchor_bottom_middle] = anchor_centre_left,
        [anchor_bottom_right] = anchor_bottom_left,
    };

    static const int map_180[] = {
        [anchor_top_left] = anchor_bottom_right,    [anchor_top_middle] = anchor_bottom_middle,
        [anchor_top_right] = anchor_bottom_left,

        [anchor_centre_left] = anchor_centre_right, [anchor_centre_middle] = anchor_centre_middle,
        [anchor_centre_right] = anchor_centre_left,

        [anchor_bottom_left] = anchor_top_right,    [anchor_bottom_middle] = anchor_top_middle,
        [anchor_bottom_right] = anchor_top_left,
    };

    static const int map_270[] = {
        [anchor_top_left] = anchor_bottom_left,      [anchor_top_middle] = anchor_centre_left,
        [anchor_top_right] = anchor_top_left,

        [anchor_centre_left] = anchor_bottom_middle, [anchor_centre_middle] = anchor_centre_middle,
        [anchor_centre_right] = anchor_top_middle,

        [anchor_bottom_left] = anchor_bottom_right,  [anchor_bottom_middle] = anchor_centre_right,
        [anchor_bottom_right] = anchor_top_right,
    };

    switch (rot) {
        case rotate_90:
            return map_90[anchor];
        case rotate_180:
            return map_180[anchor];
        case rotate_270:
            return map_270[anchor];
        default:
            return anchor;
    }
}
