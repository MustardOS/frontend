#include <string.h>
#include <sys/stat.h>
#include "overlay.h"

struct overlay_options_cache {
    time_t mtime;
    int valid;
    int values[overlay_option_field_count];
};

static struct overlay_options_cache cache = {0};

static void reload(void) {
    cache.valid = 0;

    char line[8];
    if (!read_line_from_file(OVERLAY_OPTIONS_PATH, 1, line, sizeof(line))) return;
    if (strcmp(line, "none") == 0) return;

    for (int i = 0; i < overlay_option_field_count; i++) {
        char field_line[8];
        if (!read_line_from_file(OVERLAY_OPTIONS_PATH, (size_t) (i + 1), field_line, sizeof(field_line))) return;
        cache.values[i] = safe_atoi(field_line);
    }

    cache.valid = 1;
}

int get_overlay_option_override(const enum overlay_option_field field, int *out) {
    struct stat st;

    if (stat(OVERLAY_OPTIONS_PATH, &st) != 0) {
        cache.mtime = 0;
        cache.valid = 0;
        return 0;
    }

    if (st.st_mtime != cache.mtime) {
        cache.mtime = st.st_mtime;
        reload();
    }

    if (!cache.valid) return 0;

    *out = cache.values[field];
    return 1;
}
