#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <SDL2/SDL.h>
#include "common.h"
#include "../../common/log.h"

SDL_Window *render_window;

char dimension[32];
char overlay_path[PATH_MAX];

int disable_hw_overlay = -1;

struct overlay_go_cache ovl_go_cache = {
        .mtime = 0,
        .valid = 0
};

int is_overlay_disabled(void) {
    if (disable_hw_overlay < 0) {
        const char *v = getenv("DISABLE_HW_OVERLAY");
        disable_hw_overlay = (v && (v[0] == '1' || v[0] == 'y' || v[0] == 'Y'));
    }

    return disable_hw_overlay;
}

float safe_atof(const char *str) {
    if (str == NULL) return 0.0f;

    errno = 0;
    char *str_ptr;
    double val = strtod(str, &str_ptr);

    if (str_ptr == str) return 0.0f;
    if (*str_ptr != '\0') return 0.0f;

    if (errno == ERANGE || !isfinite(val)) return 0.0f;

    if (val > FLT_MAX) return FLT_MAX;
    if (val < -FLT_MAX) return -FLT_MAX;

    return (float) val;
}

int safe_atoi(const char *str) {
    if (str == NULL) return 0;

    errno = 0;
    char *str_ptr;
    long val = strtol(str, &str_ptr, 10);

    if (str_ptr == str) return 0;
    if (*str_ptr != '\0') return 0;
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (val > INT_MAX || val < INT_MIN)) return 0;

    return (int) val;
}

static int file_exist(const char *filename) {
    return access(filename, F_OK) == 0;
}

static int directory_exist(const char *dirname) {
    struct stat st;
    return dirname && stat(dirname, &st) == 0 && S_ISDIR(st.st_mode);
}

uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t) ts.tv_sec * 1000ULL + (uint64_t) ts.tv_nsec / 1000000ULL;
}

int read_percent(const char *path, int max, int *out) {
    char buf[32];
    char *end;
    long raw;

    if (max <= 0) return 0;
    if (!read_line_from_file(path, 1, buf, sizeof(buf))) return 0;

    raw = strtol(buf, &end, 10);
    if (end == buf) return 0;

    if (raw < 0) raw = 0;
    if (raw > max) raw = max;

    *out = (int) ((raw * 100L) / max);
    return 1;
}

int read_float(const char *path, float *out) {
    char buf[32];

    if (!read_line_from_file(path, 1, buf, sizeof(buf))) return 0;
    *out = strtof(buf, NULL);

    return 1;
}

int read_line_from_file(const char *filename, size_t line_number, char *out, size_t out_size) {
    FILE *f = fopen(filename, "r");
    if (!f) return 0;

    size_t cur = 0;
    char buf[MAX_BUFFER_SIZE];

    while (fgets(buf, sizeof(buf), f)) {
        cur++;
        if (cur == line_number) {
            size_t len = strlen(buf);
            if (len && buf[len - 1] == '\n') buf[len - 1] = '\0';

            strncpy(out, buf, out_size - 1);
            out[out_size - 1] = '\0';

            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

static int read_overlay_loader(struct overlay_go_cache *cache) {
    struct stat st;

    if (stat(OVERLAY_LOADER, &st) != 0) {
        cache->valid = 0;
        return 0;
    }

    if (cache->valid && st.st_mtime == cache->mtime) return 1;
    cache->mtime = st.st_mtime;
    cache->valid = 0;

    if (!read_line_from_file(OVERLAY_LOADER, 1, cache->content, sizeof(cache->content))) return 0;
    if (!read_line_from_file(OVERLAY_LOADER, 2, cache->system, sizeof(cache->system))) return 0;
    if (!read_line_from_file(OVERLAY_LOADER, 3, cache->core, sizeof(cache->core))) return 0;

    cache->valid = 1;
    return 1;
}

static const char *get_active_theme(void) {
    static char theme[256];
    static time_t mtime = 0;

    struct stat st;

    if (stat(ACTIVE_THEME, &st) != 0) return NULL;
    if (theme[0] && st.st_mtime == mtime) return theme;
    if (!read_line_from_file(ACTIVE_THEME, 1, theme, sizeof(theme))) return NULL;

    mtime = st.st_mtime;
    return theme;
}

int load_stage_image(const char *type, const char *core, const char *sys,
                     const char *file, const char *dim, char *img_path) {
    enum img_loc {
        IMG_INFO,
        IMG_THEME,
        IMG_MEDIA
    };

    enum overlay_layout {
        OVL_PLAIN, // overlay/<type>/...
        OVL_SYSTEM // overlay/<sys>/<type>/...
    };

    static int have_theme;
    static char theme_path[MAX_BUFFER_SIZE];

    const char *active_theme = get_active_theme();
    if (active_theme) {
        snprintf(theme_path, sizeof(theme_path), THEME_PATH, active_theme);
        have_theme = directory_exist(theme_path);
    } else {
        have_theme = 0;
    }

    const struct {
        enum img_loc loc;
        const char *base;
        const char *sys;
    } base_path[] = {
            {IMG_INFO,  CATALOGUE_PATH, sys},
            {IMG_THEME, theme_path,     ""},
            {IMG_MEDIA, INTERNAL_SHARE, ""}
    };

    const char *dims[] = {dim, ""};
    const char *files[] = {file, core, "default", ""};

    for (size_t b = 0; b < A_SIZE(base_path); b++) {
        if (base_path[b].loc == IMG_THEME && !have_theme) continue;

        for (size_t d = 0; d < A_SIZE(dims); d++) {
            for (size_t f = 0; f < A_SIZE(files); f++) {
                if (!files[f] || files[f][0] == '\0') continue;

                // Catalogue paths do not vary by layout
                if (base_path[b].sys[0]) {
                    int n = snprintf(img_path, sizeof(overlay_path), "%s/%s/overlay/%s/%s%s.png",
                                     base_path[b].base, base_path[b].sys, type, dims[d], files[f]
                    );

                    // LOG_DEBUG("stage", OVERLAY_TRY, type, img_path);
                    if (n > 0 && (size_t) n < sizeof(overlay_path) && file_exist(img_path)) return 1;

                    continue;
                }

                // Theme and Internal paths
                for (enum overlay_layout l = OVL_PLAIN; l <= OVL_SYSTEM; l++) {
                    if (l == OVL_SYSTEM && (!sys || sys[0] == '\0')) continue;

                    int n;
                    if (l == OVL_SYSTEM) {
                        // Theme and Internal Path with system folder
                        n = snprintf(img_path, sizeof(overlay_path), "%s/overlay/%s/%s/%s%s.png",
                                     base_path[b].base, sys, type, dims[d], files[f]
                        );
                    } else {
                        // Plain overlay, everything else!
                        n = snprintf(img_path, sizeof(overlay_path), "%s/overlay/%s/%s%s.png",
                                     base_path[b].base, type, dims[d], files[f]
                        );
                    }

                    // LOG_DEBUG("stage", OVERLAY_TRY, type, img_path);
                    if (n > 0 && (size_t) n < sizeof(overlay_path) && file_exist(img_path)) return 1;
                }
            }
        }
    }

    return 0;
}

void get_dimension(enum render_method type, void *ctx, char *out, size_t out_sz) {
    int w = 0;
    int h = 0;

    switch (type) {
        case RENDER_SDL: {
            SDL_Renderer *r = ctx;
            SDL_GetRendererOutputSize(r, &w, &h);
            break;
        }
        case RENDER_GLES: {
            SDL_Window *win = ctx;
            SDL_GL_GetDrawableSize(win, &w, &h);
            break;
        }
    }

    snprintf(out, out_sz, "%dx%d/", w, h);
}

int load_overlay_common(const struct overlay_resolver *res, void *ctx, char *overlay_path) {
    res->get_dimension(res->render_method, ctx, dimension, sizeof(dimension));
    if (!read_overlay_loader(&ovl_go_cache)) return 0;

    if (load_stage_image("content", ovl_go_cache.core, ovl_go_cache.system,
                         ovl_go_cache.content, dimension, overlay_path)) {
        LOG_SUCCESS("stage", "Overlay loaded: %s", overlay_path);
        return 1;
    }

    LOG_WARN("stage", "Overlay not found (core=%s, system=%s, content=%s, dim=%s)",
             ovl_go_cache.core, ovl_go_cache.system, ovl_go_cache.content, dimension);
    return 0;
}
