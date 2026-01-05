#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GLES2/gl2.h>
#include "../common/options.h"
#include "../common/log.h"
#include "mustage.h"

static void (*real_SDL_RenderPresent)(SDL_Renderer *);

static void (*real_SDL_GL_SwapWindow)(SDL_Window *);

static int fb_cached_w = 0;
static int fb_cached_h = 0;

static SDL_Texture *sdl_tex;
static int sdl_ready;
static int sdl_attempted;
static int sdl_tex_w;
static int sdl_tex_h;

static GLuint gl_prog;
static GLuint gl_tex;

static GLint gl_a_pos;
static GLint gl_a_uv;
static GLint gl_u_tex;
static GLint gl_u_alpha;

static int gl_ready;
static int gl_attempted;
static int gl_tex_w;
static int gl_tex_h;

static GLfloat vtx_main[16];
static int vtx_main_valid = 0;
static GLfloat vtx_batt[16];
static int vtx_batt_valid = 0;

static SDL_Window *gl_window;

static char dimension[32];

static char overlay_path[512];
static int overlay_anchor_cached = -1;
static int overlay_scale_cached = -1;

static char battery_overlay_path[512];
static int battery_last_enabled = -1;
static int battery_anchor_cached = -1;
static int battery_scale_cached = -1;

static SDL_Texture *battery_sdl_tex;
static int battery_sdl_ready;
static int battery_sdl_attempted;
static int battery_sdl_w;
static int battery_sdl_h;

static GLuint battery_gl_tex;
static int battery_gl_ready;
static int battery_gl_attempted;
static int battery_gl_w;
static int battery_gl_h;

static struct overlay_go_cache ovl_go_cache = {
        .mtime = 0,
        .valid = 0
};

static const struct overlay_resolver SDL_RESOLVER = {
        .render_type = TYPE_SDL,
        .get_dimension = get_dimension,
};

static const struct overlay_resolver GL_RESOLVER = {
        .render_type = TYPE_GL,
        .get_dimension = get_dimension,
};

static struct anchor_cache overlay_anchor_cache = {
        .path  = OVERLAY_ANCHOR,
        .mtime = 0,
        .value = ANCHOR_TOP_LEFT
};

static struct anchor_cache battery_anchor_cache = {
        .path  = BATTERY_ANCHOR,
        .mtime = 0,
        .value = ANCHOR_TOP_LEFT
};

static struct alpha_cache overlay_alpha_cache = {
        .path  = OVERLAY_ALPHA,
        .mtime = 0,
        .value = 1.0f
};

static struct alpha_cache battery_alpha_cache = {
        .path  = BATTERY_ALPHA,
        .mtime = 0,
        .value = 1.0f
};

static struct scale_cache overlay_scale_cache = {
        .path  = OVERLAY_SCALE,
        .mtime = 0,
        .value = SCALE_ORIGINAL
};

static struct scale_cache battery_scale_cache = {
        .path  = BATTERY_SCALE,
        .mtime = 0,
        .value = SCALE_ORIGINAL
};

static const char *vs_src = "attribute vec2 a_pos;"
                            "attribute vec2 a_uv;"
                            "varying vec2 v_uv;"
                            "void main(){"
                            "    gl_Position = vec4(a_pos,0.0,1.0);"
                            "    v_uv = a_uv;"
                            "}";

static const char *fs_src = "precision mediump float;"
                            "uniform sampler2D u_tex;"
                            "uniform float u_alpha;"
                            "varying vec2 v_uv;"
                            "void main(){"
                            "    vec4 c = texture2D(u_tex, v_uv);"
                            "    gl_FragColor = vec4(c.rgb, c.a * u_alpha);"
                            "}";

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

static inline int file_exist(const char *filename) {
    return access(filename, F_OK) == 0;
}

static inline int directory_exist(const char *dirname) {
    struct stat st;
    return dirname && stat(dirname, &st) == 0 && S_ISDIR(st.st_mode);
}

__attribute__((constructor))
static void resolve_symbols(void) {
    real_SDL_RenderPresent = dlsym(RTLD_NEXT, "SDL_RenderPresent");
    real_SDL_GL_SwapWindow = dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");

    if (!real_SDL_RenderPresent) LOG_ERROR("stage", "dlsym SDL_RenderPresent failed");
    if (!real_SDL_GL_SwapWindow) LOG_ERROR("stage", "dlsym SDL_GL_SwapWindow failed");
}

static struct flag_cache battery_enable_cache = {
        .path  = BATTERY_DETECT,
        .mtime = 0,
        .value = 0
};

static inline int battery_overlay_enabled(void) {
    struct stat st;

    if (stat(battery_enable_cache.path, &st) != 0) {
        battery_enable_cache.mtime = 0;
        battery_enable_cache.value = 0;
        return 0;
    }

    if (st.st_mtime == battery_enable_cache.mtime) return battery_enable_cache.value;

    battery_enable_cache.mtime = st.st_mtime;
    battery_enable_cache.value = 1;

    return 1;
}

static int read_line_from_file(const char *filename, size_t line_number, char *out, size_t out_size) {
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

static inline int clamp_scale(int v) {
    return (v >= 0 && v <= 2) ? v : SCALE_ORIGINAL;
}

static int get_scale_cached(struct scale_cache *cache) {
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

static inline int clamp_anchor(int v) {
    return (v >= 0 && v <= 8) ? v : ANCHOR_TOP_LEFT;
}

static int get_anchor_cached(struct anchor_cache *cache) {
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

static void set_sdl_render(SDL_Renderer *renderer, int tex_w, int tex_h, int anchor, int scale, SDL_Rect *out) {
    int fb_w, fb_h;
    SDL_GetRendererOutputSize(renderer, &fb_w, &fb_h);

    int draw_w = tex_w;
    int draw_h = tex_h;

    if (scale == SCALE_FIT) {
        float sx = (float) fb_w / (float) tex_w;
        float sy = (float) fb_h / (float) tex_h;

        float s = (sx < sy) ? sx : sy;

        draw_w = (int) lroundf((float) tex_w * s);
        draw_h = (int) lroundf((float) tex_h * s);
    } else if (scale == SCALE_STRETCH) {
        draw_w = fb_w;
        draw_h = fb_h;
    }

    if (draw_w < 1) draw_w = 1;
    if (draw_h < 1) draw_h = 1;

    int x = 0;
    int y = 0;

    switch (anchor) {
        case ANCHOR_TOP_MIDDLE:
            x = (fb_w - draw_w) / 2;
            break;
        case ANCHOR_TOP_RIGHT:
            x = fb_w - draw_w;
            break;
        case ANCHOR_CENTRE_LEFT:
            y = (fb_h - draw_h) / 2;
            break;
        case ANCHOR_CENTRE_MIDDLE:
            x = (fb_w - draw_w) / 2;
            y = (fb_h - draw_h) / 2;
            break;
        case ANCHOR_CENTRE_RIGHT:
            x = fb_w - draw_w;
            y = (fb_h - draw_h) / 2;
            break;
        case ANCHOR_BOTTOM_LEFT:
            y = fb_h - draw_h;
            break;
        case ANCHOR_BOTTOM_MIDDLE:
            x = (fb_w - draw_w) / 2;
            y = fb_h - draw_h;
            break;
        case ANCHOR_BOTTOM_RIGHT:
            x = fb_w - draw_w;
            y = fb_h - draw_h;
            break;
        default:
            break;
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    out->x = x;
    out->y = y;
    out->w = draw_w;
    out->h = draw_h;
}

static void set_gl_render(GLfloat *out, int tex_w, int tex_h, int fb_w, int fb_h, int anchor, int scale) {
    float draw_w = (float) tex_w;
    float draw_h = (float) tex_h;

    if (scale == SCALE_FIT) {
        float sx = (float) fb_w / draw_w;
        float sy = (float) fb_h / draw_h;

        float s = (sx < sy) ? sx : sy;

        draw_w *= s;
        draw_h *= s;
    } else if (scale == SCALE_STRETCH) {
        draw_w = (float) fb_w;
        draw_h = (float) fb_h;
    }

    if (draw_w < 1) draw_w = 1;
    if (draw_h < 1) draw_h = 1;

    float w_ndc = (draw_w / (float) fb_w) * 2.0f;
    float h_ndc = (draw_h / (float) fb_h) * 2.0f;

    float x0;
    float x1;
    float y0;
    float y1;

    switch (anchor) {
        case ANCHOR_TOP_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_TOP_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_CENTRE_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_CENTRE_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_CENTRE_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_BOTTOM_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        case ANCHOR_BOTTOM_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        case ANCHOR_BOTTOM_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        default:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
    }

    out[0] = x0;
    out[1] = y0;
    out[2] = 0.0f;
    out[3] = 0.0f;
    out[4] = x0;
    out[5] = y1;
    out[6] = 0.0f;
    out[7] = 1.0f;
    out[8] = x1;
    out[9] = y0;
    out[10] = 1.0f;
    out[11] = 0.0f;
    out[12] = x1;
    out[13] = y1;
    out[14] = 1.0f;
    out[15] = 1.0f;
}

static float clamp_alpha(int v) {
    if (v < 0) return 0.0f;
    if (v > 255) return 1.0f;
    return (float) v / 255.0f;
}

static float get_alpha_cached(struct alpha_cache *cache) {
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

static const char *get_active_theme_cached(void) {
    static char theme[256];
    static time_t mtime = 0;

    struct stat st;

    if (stat("/opt/muos/config/theme/active", &st) != 0) return NULL;
    if (theme[0] && st.st_mtime == mtime) return theme;
    if (!read_line_from_file("/opt/muos/config/theme/active", 1, theme, sizeof(theme))) return NULL;

    mtime = st.st_mtime;
    return theme;
}

static int load_image_static(const char *file, const char *dim, char *img) {
    enum static_kind {
        ST_THEME, ST_MEDIA
    };

    static char theme_path[MAX_BUFFER_SIZE];

    const char *active_theme = get_active_theme_cached();
    if (active_theme) {
        snprintf(theme_path, sizeof(theme_path), "%stheme/%s", RUN_STORAGE_PATH, active_theme);
    } else {
        theme_path[0] = '\0';
    }

    const int have_theme = theme_path[0] && directory_exist(theme_path);
    const char *media_path = "/opt/muos/share/media";

    struct {
        enum static_kind kind;
        const char *base;
        const char *dir;
        const char *dim;
        const char *file;
    } order[] = {
            {ST_THEME, theme_path, "image/", dim,      file},
            {ST_THEME, theme_path, "image/", dim,      ""},
            {ST_THEME, theme_path, dim,      "image/", file},
            {ST_THEME, theme_path, dim,      "image/", ""},
            {ST_THEME, theme_path, "image/", "",       file},
            {ST_THEME, theme_path, "image/", "",       ""},

            {ST_MEDIA, media_path, "",       "",       file},
            {ST_MEDIA, media_path, "",       "",       ""},
    };

    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        if (order[i].kind == ST_THEME && !have_theme) continue;
        if (!order[i].file || order[i].file[0] == '\0') continue;

        int n = snprintf(img, 512, "%s/%s%s%s.png",
                         order[i].base, order[i].dir, order[i].dim, order[i].file);

        LOG_DEBUG("stage", "Trying battery overlay at: %s", img);

        if (n > 0 && (size_t) n < 512 && file_exist(img)) return 1;
    }

    return 0;
}

static int load_image_catalogue(const char *core, const char *sys, const char *file, const char *dim, char *img) {
    enum catalogue_kind {
        CAT_THEME, CAT_INFO
    };

    static char theme_cat_path[MAX_BUFFER_SIZE];

    const char *active_theme = get_active_theme_cached();
    if (active_theme) {
        snprintf(theme_cat_path, sizeof(theme_cat_path), "%stheme/%s", RUN_STORAGE_PATH, active_theme);
    } else {
        theme_cat_path[0] = '\0';
    }

    const int have_theme = theme_cat_path[0] && directory_exist(theme_cat_path);
    const char *global_cat_path = RUN_STORAGE_PATH "info/catalogue";

    struct {
        enum catalogue_kind kind;
        const char *base;
        const char *dim;
        const char *file;
    } order[] = {
            {CAT_THEME, theme_cat_path,  dim, file},
            {CAT_THEME, theme_cat_path,  dim, core},
            {CAT_THEME, theme_cat_path,  dim, ""},
            {CAT_THEME, theme_cat_path,  "",  file},
            {CAT_THEME, theme_cat_path,  "",  core},
            {CAT_THEME, theme_cat_path,  "",  ""},

            {CAT_INFO,  global_cat_path, dim, file},
            {CAT_INFO,  global_cat_path, dim, core},
            {CAT_INFO,  global_cat_path, dim, ""},
            {CAT_INFO,  global_cat_path, "",  file},
            {CAT_INFO,  global_cat_path, "",  core},
            {CAT_INFO,  global_cat_path, "",  ""},

            {CAT_THEME, theme_cat_path,  dim, ""},
            {CAT_THEME, theme_cat_path,  "",  ""},

            {CAT_THEME, theme_cat_path,  dim, "default"},
            {CAT_THEME, theme_cat_path,  "",  "default"},

            {CAT_INFO,  global_cat_path, dim, ""},
            {CAT_INFO,  global_cat_path, "",  ""},

            {CAT_INFO,  global_cat_path, dim, "default"},
            {CAT_INFO,  global_cat_path, "",  "default"},
    };

    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        if (order[i].kind == CAT_THEME && !have_theme) continue;
        if (!order[i].file || order[i].file[0] == '\0') continue;

        int n = snprintf(img, 512, "%s/%s/overlay/%s%s.png",
                         order[i].base, sys, order[i].dim, order[i].file);

        LOG_DEBUG("stage", "Trying image overlay at: %s", img);

        if (n > 0 && (size_t) n < 512 && file_exist(img)) return 1;
    }

    return 0;
}

static int read_overlay_go_cached(struct overlay_go_cache *c) {
    struct stat st;

    if (stat("/tmp/ovl_go", &st) != 0) {
        c->valid = 0;
        return 0;
    }

    if (c->valid && st.st_mtime == c->mtime) return 1;

    c->mtime = st.st_mtime;
    c->valid = 0;

    if (!read_line_from_file("/tmp/ovl_go", 1, c->content, sizeof(c->content))) return 0;
    if (!read_line_from_file("/tmp/ovl_go", 2, c->system, sizeof(c->system))) return 0;
    if (!read_line_from_file("/tmp/ovl_go", 3, c->core, sizeof(c->core))) return 0;

    c->valid = 1;
    return 1;
}

void get_dimension(enum render_type type, void *ctx, char *out, size_t out_sz) {
    int w = 0;
    int h = 0;

    switch (type) {
        case TYPE_SDL: {
            SDL_Renderer *r = ctx;
            SDL_GetRendererOutputSize(r, &w, &h);
            break;
        }
        case TYPE_GL: {
            SDL_Window *win = ctx;
            SDL_GL_GetDrawableSize(win, &w, &h);
            break;
        }
    }

    snprintf(out, out_sz, "%dx%d/", w, h);
}

static inline void battery_overlay_update(void) {
    int enabled = battery_overlay_enabled();
    if (enabled == battery_last_enabled) return;

    battery_last_enabled = enabled;
    if (enabled) {
        battery_sdl_attempted = 0;
        battery_gl_attempted = 0;
        return;
    }

    if (battery_sdl_tex) {
        SDL_DestroyTexture(battery_sdl_tex);
        battery_sdl_tex = NULL;
    }

    if (battery_gl_tex) {
        glDeleteTextures(1, &battery_gl_tex);
        battery_gl_tex = 0;
    }

    battery_anchor_cache.mtime = 0;
    battery_alpha_cache.mtime = 0;
    battery_scale_cache.mtime = 0;

    battery_anchor_cached = -1;
    battery_scale_cached = -1;

    battery_sdl_ready = 0;
    battery_gl_ready = 0;

    vtx_batt_valid = 0;
    battery_overlay_path[0] = '\0';
}

static int resolve_battery_overlay(enum render_type type, void *ctx) {
    char dim[32];
    battery_overlay_path[0] = '\0';

    switch (type) {
        case TYPE_SDL:
            get_dimension(TYPE_SDL, ctx, dim, sizeof(dim));
            break;
        case TYPE_GL:
            get_dimension(TYPE_GL, ctx, dim, sizeof(dim));
            break;
    }

    if (load_image_static("low_battery", dim, battery_overlay_path)) {
        LOG_INFO("stage", "Battery overlay resolved: %s", battery_overlay_path);
        return 1;
    }

    LOG_WARN("stage", "Battery overlay not found (dim=%s)", dim);
    return 0;
}

static void sdl_battery_overlay_init(SDL_Renderer *renderer) {
    if (battery_sdl_ready || battery_sdl_attempted) return;
    battery_sdl_attempted = 1;

    if (!battery_overlay_enabled()) return;
    if (!resolve_battery_overlay(TYPE_SDL, renderer))return;

    SDL_Surface *surface = IMG_Load(battery_overlay_path);
    if (!surface) {
        LOG_ERROR("stage", "Low Battery PNG load failed: %s", IMG_GetError());
        return;
    }

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    battery_sdl_tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (!battery_sdl_tex) {
        LOG_ERROR("stage", "Low Battery texture create failed: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    SDL_SetTextureBlendMode(battery_sdl_tex, SDL_BLENDMODE_BLEND);
    SDL_QueryTexture(battery_sdl_tex, NULL, NULL, &battery_sdl_w, &battery_sdl_h);

    SDL_FreeSurface(surface);
    battery_sdl_ready = 1;
}

static void gl_battery_overlay_init(void) {
    if (battery_gl_ready || battery_gl_attempted) return;
    battery_gl_attempted = 1;

    if (!battery_overlay_enabled()) return;
    if (!resolve_battery_overlay(TYPE_GL, gl_window)) return;

    SDL_Surface *raw = IMG_Load(battery_overlay_path);
    if (!raw) {
        LOG_ERROR("stage", "Low Battery GL image load failed: %s", IMG_GetError());
        return;
    }

    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);

    if (!rgba) return;

    battery_gl_w = rgba->w;
    battery_gl_h = rgba->h;

    glGenTextures(1, &battery_gl_tex);
    glBindTexture(GL_TEXTURE_2D, battery_gl_tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);

    SDL_FreeSurface(rgba);
    battery_gl_ready = 1;
}

static int load_overlay_common(const struct overlay_resolver *res, void *ctx, char *out) {
    res->get_dimension(res->render_type, ctx, dimension, sizeof(dimension));

    if (!read_overlay_go_cached(&ovl_go_cache)) return 0;

    if (load_image_catalogue(ovl_go_cache.core, ovl_go_cache.system, ovl_go_cache.content, dimension, out)) {
        LOG_INFO("stage", "Overlay loaded: %s", out);
        return 1;
    }

    LOG_WARN("stage", "Overlay not found (core=%s, system=%s, content=%s, dim=%s)",
             ovl_go_cache.core, ovl_go_cache.system, ovl_go_cache.content, dimension);

    return 0;
}

static void sdl_overlay_init(SDL_Renderer *renderer) {
    if (sdl_ready) return;

    if (sdl_attempted) return;
    sdl_attempted = 1;

    if (!load_overlay_common(&SDL_RESOLVER, renderer, overlay_path)) {
        LOG_WARN("stage", "SDL overlay failed to resolve path");
        return;
    }

    SDL_Surface *surface = IMG_Load(overlay_path);
    if (!surface) {
        LOG_ERROR("stage", "SDL Image load failed: %s", IMG_GetError());
        return;
    }

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    sdl_tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (!sdl_tex) {
        LOG_ERROR("stage", "SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    SDL_SetTextureBlendMode(sdl_tex, SDL_BLENDMODE_BLEND);
    SDL_QueryTexture(sdl_tex, NULL, NULL, &sdl_tex_w, &sdl_tex_h);

    SDL_FreeSurface(surface);
    sdl_ready = 1;
}

void SDL_RenderPresent(SDL_Renderer *renderer) {
    if (!real_SDL_RenderPresent) return;

    battery_overlay_update();

    float overlay_alpha = get_alpha_cached(&overlay_alpha_cache);
    float battery_alpha = get_alpha_cached(&battery_alpha_cache);

    sdl_overlay_init(renderer);

    if (sdl_tex) {
        int overlay_anchor = get_anchor_cached(&overlay_anchor_cache);
        int overlay_scale = get_scale_cached(&overlay_scale_cache);

        SDL_Rect overlay_dst;
        set_sdl_render(renderer, sdl_tex_w, sdl_tex_h, overlay_anchor, overlay_scale, &overlay_dst);

        SDL_SetTextureAlphaMod(sdl_tex, (Uint8) (overlay_alpha * 255.0f));
        SDL_RenderCopy(renderer, sdl_tex, NULL, &overlay_dst);
    }

    sdl_battery_overlay_init(renderer);

    if (battery_sdl_tex) {
        int battery_anchor = get_anchor_cached(&battery_anchor_cache);
        int battery_scale = get_scale_cached(&battery_scale_cache);

        SDL_Rect battery_dst;
        set_sdl_render(renderer, battery_sdl_w, battery_sdl_h, battery_anchor, battery_scale, &battery_dst);

        SDL_SetTextureAlphaMod(battery_sdl_tex, (Uint8) (battery_alpha * 255.0f));
        SDL_RenderCopy(renderer, battery_sdl_tex, NULL, &battery_dst);
    }

    real_SDL_RenderPresent(renderer);
}

static GLuint gl_compile_checked(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);

    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log_buffer[512];
        GLsizei n = 0;
        glGetShaderInfoLog(sh, (GLsizei) sizeof(log_buffer), &n, log_buffer);
        LOG_ERROR("stage", "GL shader compile failed: %.*s", (int) n, log_buffer);
        glDeleteShader(sh);
        return 0;
    }

    return sh;
}

static GLuint gl_link_checked(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log_buffer[512];
        GLsizei n = 0;
        glGetProgramInfoLog(p, (GLsizei) sizeof(log_buffer), &n, log_buffer);
        LOG_ERROR("stage", "GL program link failed: %.*s", (int) n, log_buffer);
        glDeleteProgram(p);
        return 0;
    }

    return p;
}

static void gl_overlay_init(void) {
    if (!gl_window) return;

    if (gl_attempted) return;
    gl_attempted = 1;

    if (!load_overlay_common(&GL_RESOLVER, gl_window, overlay_path)) {
        LOG_WARN("stage", "GL overlay failed to resolve path");
        return;
    }

    GLuint vs = gl_compile_checked(GL_VERTEX_SHADER, vs_src);
    GLuint fs = gl_compile_checked(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    gl_prog = gl_link_checked(vs, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!gl_prog) return;

    gl_a_pos = glGetAttribLocation(gl_prog, "a_pos");
    gl_a_uv = glGetAttribLocation(gl_prog, "a_uv");
    gl_u_tex = glGetUniformLocation(gl_prog, "u_tex");
    gl_u_alpha = glGetUniformLocation(gl_prog, "u_alpha");

    if (gl_u_alpha < 0) LOG_WARN("stage", "Alpha transparency not found in image...");

    SDL_Surface *raw = IMG_Load(overlay_path);
    if (!raw) {
        LOG_ERROR("stage", "GL Image load failed: %s", IMG_GetError());
        glDeleteProgram(gl_prog);
        gl_prog = 0;
        return;
    }

    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);
    if (!rgba) {
        LOG_ERROR("stage", "SDL_ConvertSurfaceFormat failed: %s", SDL_GetError());
        glDeleteProgram(gl_prog);
        gl_prog = 0;
        return;
    }

    gl_tex_w = rgba->w;
    gl_tex_h = rgba->h;

    glGenTextures(1, &gl_tex);
    glBindTexture(GL_TEXTURE_2D, gl_tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);

    SDL_FreeSurface(rgba);

    gl_ready = 1;
    vtx_main_valid = 0;
}

struct gl_restore_min {
    GLint program;
    GLint active_tex;
    GLint tex_binding;
    GLint viewport[4];

    GLboolean blend;
    GLint blend_src_rgb;
    GLint blend_dst_rgb;
};

static void gl_save_min(struct gl_restore_min *st) {
    glGetIntegerv(GL_CURRENT_PROGRAM, &st->program);

    glGetIntegerv(GL_ACTIVE_TEXTURE, &st->active_tex);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &st->tex_binding);

    glGetIntegerv(GL_VIEWPORT, st->viewport);

    st->blend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_RGB, &st->blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &st->blend_dst_rgb);
}

static void gl_restore_min(const struct gl_restore_min *st) {
    if (st->blend) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);

    glBlendFunc(st->blend_src_rgb, st->blend_dst_rgb);
    glUseProgram(st->program);

    glActiveTexture(st->active_tex);
    glBindTexture(GL_TEXTURE_2D, (GLuint) st->tex_binding);

    glViewport(st->viewport[0], st->viewport[1], st->viewport[2], st->viewport[3]);
}

static void gl_draw(int fb_w, int fb_h) {
    if (!gl_ready) {
        gl_overlay_init();
        return;
    }

    float overlay_alpha = get_alpha_cached(&overlay_alpha_cache);
    float battery_alpha = get_alpha_cached(&battery_alpha_cache);

    int overlay_anchor = get_anchor_cached(&overlay_anchor_cache);
    if (overlay_anchor != overlay_anchor_cached) {
        overlay_anchor_cached = overlay_anchor;
        vtx_main_valid = 0;
    }

    int battery_anchor = get_anchor_cached(&battery_anchor_cache);
    if (battery_anchor != battery_anchor_cached) {
        battery_anchor_cached = battery_anchor;
        vtx_batt_valid = 0;
    }

    int overlay_scale = get_scale_cached(&overlay_scale_cache);
    if (overlay_scale != overlay_scale_cached) {
        overlay_scale_cached = overlay_scale;
        vtx_main_valid = 0;
    }

    int battery_scale = get_scale_cached(&battery_scale_cache);
    if (battery_scale != battery_scale_cached) {
        battery_scale_cached = battery_scale;
        vtx_batt_valid = 0;
    }

    gl_battery_overlay_init();

    if (!vtx_main_valid) {
        set_gl_render(vtx_main, gl_tex_w, gl_tex_h, fb_w, fb_h, overlay_anchor, overlay_scale);
        vtx_main_valid = 1;
    }

    if (battery_gl_ready && !vtx_batt_valid) {
        set_gl_render(vtx_batt, battery_gl_w, battery_gl_h, fb_w, fb_h, battery_anchor, battery_scale);
        vtx_batt_valid = 1;
    }

    struct gl_restore_min st;
    gl_save_min(&st);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(gl_prog);
    glViewport(0, 0, fb_w, fb_h);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(gl_u_tex, 0);

    glEnableVertexAttribArray(gl_a_pos);
    glEnableVertexAttribArray(gl_a_uv);

    if (gl_u_alpha >= 0) glUniform1f(gl_u_alpha, overlay_alpha);
    glBindTexture(GL_TEXTURE_2D, gl_tex);
    glVertexAttribPointer(gl_a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vtx_main);
    glVertexAttribPointer(gl_a_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vtx_main + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (battery_gl_ready) {
        if (gl_u_alpha >= 0) glUniform1f(gl_u_alpha, battery_alpha);
        glBindTexture(GL_TEXTURE_2D, battery_gl_tex);
        glVertexAttribPointer(gl_a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vtx_batt);
        glVertexAttribPointer(gl_a_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vtx_batt + 2);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glDisableVertexAttribArray(gl_a_pos);
    glDisableVertexAttribArray(gl_a_uv);

    gl_restore_min(&st);
}

void SDL_GL_SwapWindow(SDL_Window *window) {
    if (!real_SDL_GL_SwapWindow) return;

    gl_window = window;

    battery_overlay_update();

    static int fb_init = 0;

    if (!fb_init) {
        SDL_GL_GetDrawableSize(window, &fb_cached_w, &fb_cached_h);
        fb_init = 1;
    } else {
        int nw, nh;
        SDL_GL_GetDrawableSize(window, &nw, &nh);
        if (nw != fb_cached_w || nh != fb_cached_h) {
            fb_cached_w = nw;
            fb_cached_h = nh;
            vtx_main_valid = 0;
            vtx_batt_valid = 0;
        }
    }

    int dw = fb_cached_w;
    int dh = fb_cached_h;

    gl_draw(dw, dh);

    real_SDL_GL_SwapWindow(window);
}
