#include <errno.h>
#include <string.h>
#include "common.h"
#include "filter.h"

#define FILTER_DIR INTERNAL_SHARE "/filter/"
#define FILTER_NAME_PATH OVERLAY_RUNNER "filter"

typedef enum {
    SECTION_NONE,
    SECTION_PROFILE,
    SECTION_MATRIX
} filter_section_t;

const colour_filter_matrix_t filter_none;

static struct {
    colour_filter_matrix_t current;
    int loaded;
} cache;

// For future reference the matrix follows: (and R R R etc...)
//  | Rr Gr Br |
//  | Rg Gg Bg |
//  | Rb Gb Bb |

const colour_filter_matrix_t filter_none = {
        .enabled = 0,
        .matrix = {
                1.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 1.0f}
};

static int parse_float3(const char *line, float *a, float *b, float *c) {
    char *end;
    errno = 0;

    *a = strtof(line, &end);
    if (end == line || errno == ERANGE) return 0;

    char *p = end;
    *b = strtof(p, &end);
    if (end == p || errno == ERANGE) return 0;

    p = end;
    *c = strtof(p, &end);
    if (end == p || errno == ERANGE) return 0;

    return 1;
}

static int parse_enabled(const char *line, int *out) {
    const char *ptr = strchr(line, '=');
    if (!ptr) return 0;

    ptr++;
    while (*ptr == ' ' || *ptr == '\t') ptr++;

    errno = 0;
    char *end;
    long v = strtol(ptr, &end, 10);

    if (end == ptr) return 0;
    if (errno == ERANGE) return 0;

    *out = (v != 0);
    return 1;
}

static int load_filter_file(const char *name, colour_filter_matrix_t *out) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s.ini", FILTER_DIR, name);

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    memset(out, 0, sizeof(*out));
    out->enabled = 1;

    filter_section_t section = SECTION_NONE;
    int row = 0;
    char line[128];

    int seen_matrix = 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0; // Fuck Windows!
        if (line[0] == 0 || line[0] == '#') continue;

        if (line[0] == '[') {
            if (!strcmp(line, "[profile]")) {
                section = SECTION_PROFILE;
            } else if (!strcmp(line, "[matrix]")) {
                section = SECTION_MATRIX;
                seen_matrix = 1;
            } else {
                section = SECTION_NONE;
            }
            continue;
        }

        if (section == SECTION_PROFILE) {
            if (!strncmp(line, "enabled", 7)) {
                int v;
                if (parse_enabled(line, &v)) out->enabled = v;
            }
            continue;
        }

        if (section == SECTION_MATRIX && row < 3) {
            float a, b, c;
            if (!parse_float3(line, &a, &b, &c)) {
                fclose(f);
                return 0;
            }

            out->matrix[row * 3 + 0] = a;
            out->matrix[row * 3 + 1] = b;
            out->matrix[row * 3 + 2] = c;

            row++;
            continue;
        }
    }

    fclose(f);
    return seen_matrix && row == 3;
}

const colour_filter_matrix_t *colour_filter_get(void) {
    if (cache.loaded) return &cache.current;
    char name[64] = "none";

    read_line_from_file(FILTER_NAME_PATH, 1, name, sizeof(name));
    if (!load_filter_file(name, &cache.current)) memcpy(&cache.current, &filter_none, sizeof(filter_none));

    for (int i = 0; i < 9; i++) {
        cache.current.matrix[i] = clamp_float(cache.current.matrix[i], -2.0f, 2.0f);
    }

    cache.loaded = 1;
    return &cache.current;
}

void colour_filter_reset(void) {
    memset(&cache, 0, sizeof(cache));
}
