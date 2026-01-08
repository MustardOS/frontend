#pragma once

#include <SDL2/SDL.h>
#include <stddef.h>
#include <time.h>
#include <GLES2/gl2.h>
#include <linux/limits.h>

#define MAX_BUFFER_SIZE 1024

#define INDICATOR_STEPS   10
#define INDICATOR_SHOW_MS 1024

#define OVERLAY_LOADER  "/tmp/ovl_go"
#define INTERNAL_SHARE  "/opt/muos/share" // Do NOT put a trailing slash here!
#define INTERNAL_STORE  "/run/muos/storage/"
#define INTERNAL_CONFIG "/opt/muos/config/"
#define INTERNAL_DEVICE "/opt/muos/device/config/"

#define CATALOGUE_PATH INTERNAL_STORE "info/catalogue"
#define THEME_PATH     INTERNAL_STORE "theme/%s"

#define ACTIVE_THEME   INTERNAL_CONFIG "theme/active"
#define GENERAL_CONFIG INTERNAL_CONFIG "settings/general/"
#define OVERLAY_CONFIG INTERNAL_CONFIG "settings/overlay/"

#define OVERLAY_TRY "Trying '%s' overlay at: %s"
#define OVERLAY_NOP "overlay not found (core=%s system=%s content=%s dim=%s step=%d)"

#define A_SIZE(x) (sizeof(x) / sizeof((x)[0]))

extern SDL_Window *render_window;

extern char dimension[32];
extern char overlay_path[PATH_MAX];

extern int disable_hw_overlay;

typedef struct {
    GLfloat x, y;
    GLfloat u, v;
} gl_vtx_t;

enum render_method {
    RENDER_SDL = 1,
    RENDER_GLES = 2
};

struct flag_cache {
    const char *path;
    time_t mtime;
    int value;
};

struct overlay_go_cache {
    time_t mtime;
    char content[512];
    char system[256];
    char core[256];
    int valid;
};

struct overlay_resolver {
    int render_method;

    void (*get_dimension)(enum render_method type, void *ctx, char *out, size_t out_sz);
};

extern struct overlay_go_cache ovl_go_cache;

int is_overlay_disabled(void);

float safe_atof(const char *str);

int safe_atoi(const char *str);

uint64_t now_ms(void);

int read_percent(const char *path, int max_pct, int *out);

int read_float(const char *path, float *out);

int read_line_from_file(const char *filename, size_t line_number, char *out, size_t out_size);

int load_stage_image(const char *type, const char *core, const char *sys,
                     const char *file, const char *dim, char *img_path);

void get_dimension(enum render_method type, void *ctx, char *out, size_t out_sz);

int load_overlay_common(const struct overlay_resolver *res, void *ctx, char *overlay_path);
