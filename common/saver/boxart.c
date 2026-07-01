#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../image.h"
#include "../log.h"
#include "../saver.h"
#include "boxart.h"

#define BOX_PARTICLE_COUNT 8
#define BOX_PATH_MAX       4096
#define BOX_LOAD_RETRIES   5

static const char *const exclude_folder[] = {"Application", "Archive", "Folder", "Root", "Task", "Theme"};
#define BOX_EXCLUDED_N ((int) (sizeof(exclude_folder) / sizeof(exclude_folder[0])))

typedef struct {
    SDL_Texture *tex;
    int tex_w, tex_h;
    float x, y;
    float vy;
    float display_h;
} box_particle_t;

typedef struct {
    saver_state_t base;
    char **paths;
    int path_count;
    box_particle_t particles[BOX_PARTICLE_COUNT];
    SDL_Renderer *renderer;
} boxart_module_t;

static boxart_module_t mod;

static int is_excluded(const char *name) {
    for (int i = 0; i < BOX_EXCLUDED_N; i++) {
        if (strcmp(name, exclude_folder[i]) == 0) return 1;
    }
    return 0;
}

static float randf01(void) {
    return (float) saver_rand_range(10000) / 10000.0f;
}

static float vy_base(void) {
    int speed = saver_read_speed();
    if (speed <= 0) speed = SAVER_SPEED_DEFAULT;

    return (float) mod.base.screen_h * (float) speed / 540000.0f;
}

static int load_particle_tex(box_particle_t *p) {
    if (mod.path_count == 0) return 0;

    const int start = saver_rand_range(mod.path_count);
    for (int attempt = 0; attempt < BOX_LOAD_RETRIES; attempt++) {
        const int idx = (start + attempt) % mod.path_count;

        SDL_Surface *surf = IMG_Load(mod.paths[idx]);
        if (!surf) continue;

        SDL_Texture *tex = SDL_CreateTextureFromSurface(mod.renderer, surf);
        SDL_FreeSurface(surf);
        if (!tex) continue;

        if (p->tex) SDL_DestroyTexture(p->tex);
        p->tex = tex;
        SDL_QueryTexture(p->tex, NULL, NULL, &p->tex_w, &p->tex_h);
        SDL_SetTextureBlendMode(p->tex, SDL_BLENDMODE_BLEND);
        return 1;
    }
    return 0;
}

static void spawn_particle(box_particle_t *p, const int initial) {
    const int sw = mod.base.screen_w;
    const int sh = mod.base.screen_h;

    if (!load_particle_tex(p)) return;

    const float scale = 0.12f + randf01() * 0.33f;
    p->display_h = (float) sh * scale;
    p->x = randf01() * (float) sw;

    const float size_factor = (scale - 0.12f) / 0.33f;
    p->vy = vy_base() * (0.3f + size_factor * 0.7f + randf01() * 0.5f);

    if (initial) {
        p->y = randf01() * ((float) sh + p->display_h);
    } else {
        p->y = (float) sh + p->display_h * 0.5f;
    }
}

static void on_idle_enter(void *user) {
    boxart_module_t *m = user;
    if (m->path_count == 0) return;

    for (int i = 0; i < BOX_PARTICLE_COUNT; i++) {
        spawn_particle(&m->particles[i], 1);
    }
}

static void collect_box_images(const char *cat_path, char ***paths, int *count, int *cap) {
    DIR *sys_d = opendir(cat_path);
    if (!sys_d) return;

    struct dirent *sys_ent;
    while ((sys_ent = readdir(sys_d)) != NULL) {
        if (sys_ent->d_name[0] == '.') continue;
        if (is_excluded(sys_ent->d_name)) continue;

        char box_dir[4096];
        snprintf(box_dir, sizeof(box_dir), "%s/%s/box", cat_path, sys_ent->d_name);

        DIR *box_d = opendir(box_dir);
        if (!box_d) continue;

        struct dirent *img_ent;
        while ((img_ent = readdir(box_d)) != NULL && *count < BOX_PATH_MAX) {
            if (img_ent->d_name[0] == '.') continue;

            const char *dot = strrchr(img_ent->d_name, '.');
            if (!dot) continue;

            char lo[16];
            int k;
            const char *raw = dot + 1;
            for (k = 0; raw[k] && k < 15; k++)
                lo[k] = (char) tolower((unsigned char) raw[k]);
            lo[k] = '\0';
            if (!is_image_ext(lo)) continue;

            if (*count >= *cap) {
                int new_cap = *cap > 0 ? *cap * 2 : 64;
                if (new_cap > BOX_PATH_MAX) new_cap = BOX_PATH_MAX;
                char **tmp = realloc(*paths, (size_t) new_cap * sizeof(char *));
                if (!tmp) {
                    closedir(box_d);
                    closedir(sys_d);
                    return;
                }
                *paths = tmp;
                *cap = new_cap;
            }

            char full[4096];
            snprintf(full, sizeof(full), "%s/%s", box_dir, img_ent->d_name);
            char *dup = strdup(full);
            if (dup) (*paths)[(*count)++] = dup;
        }
        closedir(box_d);
    }
    closedir(sys_d);
}

int boxart_init(SDL_Renderer *renderer, const char *catalogue_path, const int screen_w, const int screen_h) {
    memset(&mod, 0, sizeof(mod));
    mod.renderer = renderer;

    char **paths = NULL;
    int count = 0, cap = 0;

    collect_box_images(catalogue_path, &paths, &count, &cap);

    mod.paths = paths;
    mod.path_count = count;

    saver_init_base(&mod.base, screen_w, screen_h, "Flying Box Art", 0, 0, 0, NULL, on_idle_enter, &mod);

    LOG_INFO("saver", "Box Art: found %d image%s", count, count == 1 ? "" : "s");

    return 1;
}

void boxart_update(void) {
    const uint32_t now = SDL_GetTicks();

    if (!saver_poll_idle(&mod.base, now)) return;
    if (mod.path_count == 0) return;

    const uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    for (int i = 0; i < BOX_PARTICLE_COUNT; i++) {
        box_particle_t *p = &mod.particles[i];

        if (!p->tex) {
            spawn_particle(p, 0);
            continue;
        }

        p->y -= p->vy * (float) elapsed;

        if (p->y + p->display_h * 0.5f < 0.0f) {
            spawn_particle(p, 0);
        }
    }
}

void boxart_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;
    if (mod.path_count == 0) return;

    int order[BOX_PARTICLE_COUNT];
    for (int i = 0; i < BOX_PARTICLE_COUNT; i++)
        order[i] = i;
    for (int i = 0; i < BOX_PARTICLE_COUNT - 1; i++) {
        for (int j = i + 1; j < BOX_PARTICLE_COUNT; j++) {
            if (mod.particles[order[j]].display_h < mod.particles[order[i]].display_h) {
                const int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }

    const int sh = mod.base.screen_h;

    for (int i = 0; i < BOX_PARTICLE_COUNT; i++) {
        const box_particle_t *p = &mod.particles[order[i]];
        if (!p->tex || p->tex_h == 0) continue;

        const float aspect = (float) p->tex_w / (float) p->tex_h;
        const float dh = p->display_h;
        const float dw = dh * aspect;

        SDL_Rect rect = {(int) (p->x - dw * 0.5f), (int) (p->y - dh * 0.5f), (int) dw, (int) dh};

        const float screen_t = p->y / (float) sh;
        float alpha = 1.0f;
        if (screen_t > 0.85f) {
            alpha = (1.0f - screen_t) / 0.15f;
        } else if (screen_t < 0.15f) {
            alpha = screen_t / 0.15f;
        }
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;

        SDL_SetTextureAlphaMod(p->tex, (uint8_t) (alpha * 255.0f));
        SDL_RenderCopy(renderer, p->tex, NULL, &rect);
    }
}

int boxart_active(void) {
    if (mod.path_count == 0) return 0;
    return saver_active_base(&mod.base);
}

void boxart_stop(void) {
    saver_stop_base(&mod.base);
}

void boxart_shutdown(void) {
    for (int i = 0; i < BOX_PARTICLE_COUNT; i++) {
        if (mod.particles[i].tex) {
            SDL_DestroyTexture(mod.particles[i].tex);
            mod.particles[i].tex = NULL;
        }
    }

    for (int i = 0; i < mod.path_count; i++)
        free(mod.paths[i]);
    free(mod.paths);
    mod.paths = NULL;
    mod.path_count = 0;

    saver_shutdown_base(&mod.base);
}
