#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../image.h"
#include "../log.h"
#include "../saver.h"
#include "boxart.h"

#define BOX_PARTICLE_COUNT     8
#define BOX_PATH_MAX           4096
#define BOX_CACHE_COUNT        24
#define BOX_LOAD_RETRIES       8
#define BOX_CACHE_LOOKUPS      12
#define BOX_SPAWNS_PER_UPDATE  2
#define BOX_LOADS_PER_UPDATE   1
#define BOX_MIN_TEXTURE_HEIGHT 160
#define BOX_MAX_TEXTURE_HEIGHT 384

static const char *const exclude_folder[] = {"Application", "Archive", "Folder", "Root", "Task", "Theme"};
#define BOX_EXCLUDED_N ((int) (sizeof(exclude_folder) / sizeof(exclude_folder[0])))

typedef struct {
    SDL_Texture *tex;
    int path_index;
    int tex_w;
    int tex_h;
    int ref_count;
    uint32_t last_used;
} box_cache_entry_t;

typedef struct {
    box_cache_entry_t *cache;
    float x;
    float y;
    float vy;
    float display_h;
} box_particle_t;

typedef struct {
    saver_state_t base;
    char **paths;
    unsigned char *bad_paths;
    int path_count;
    int initial_fill;
    uint32_t use_serial;
    box_particle_t particles[BOX_PARTICLE_COUNT];
    box_cache_entry_t cache[BOX_CACHE_COUNT];
    SDL_Renderer *renderer;
} boxart_module_t;

static boxart_module_t mod;

static char *box_strdup(const char *src) {
    const size_t len = strlen(src) + 1;
    char *dst = malloc(len);
    if (!dst) return NULL;

    memcpy(dst, src, len);
    return dst;
}

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

static int texture_height_limit(void) {
    int limit = mod.base.screen_h * 45 / 100 + 16;

    if (limit < BOX_MIN_TEXTURE_HEIGHT) limit = BOX_MIN_TEXTURE_HEIGHT;
    if (limit > BOX_MAX_TEXTURE_HEIGHT) limit = BOX_MAX_TEXTURE_HEIGHT;

    return limit;
}

static SDL_Surface *limit_surface_size(SDL_Surface *src) {
    if (!src) return NULL;

    const int max_h = texture_height_limit();
    if (src->h <= max_h || src->h <= 0 || src->w <= 0) return src;

    const int dst_h = max_h;
    int dst_w = (int) ((int64_t) src->w * (int64_t) dst_h / (int64_t) src->h);
    if (dst_w < 1) dst_w = 1;

    SDL_Surface *dst = SDL_CreateRGBSurfaceWithFormat(0, dst_w, dst_h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!dst) return src;

    SDL_Rect dst_rect = {0, 0, dst_w, dst_h};
    if (SDL_BlitScaled(src, NULL, dst, &dst_rect) != 0) {
        SDL_FreeSurface(dst);
        return src;
    }

    SDL_FreeSurface(src);
    return dst;
}

static void cache_touch(box_cache_entry_t *entry) {
    if (!entry) return;

    if (++mod.use_serial == 0) {
        mod.use_serial = 1;
        for (int i = 0; i < BOX_CACHE_COUNT; i++) {
            mod.cache[i].last_used = 0;
        }
    }

    entry->last_used = mod.use_serial;
}

static box_cache_entry_t *cache_find_by_path(const int path_index) {
    for (int i = 0; i < BOX_CACHE_COUNT; i++) {
        box_cache_entry_t *entry = &mod.cache[i];
        if (entry->tex && entry->path_index == path_index) return entry;
    }

    return NULL;
}

static box_cache_entry_t *cache_find_slot(void) {
    box_cache_entry_t *oldest = NULL;

    for (int i = 0; i < BOX_CACHE_COUNT; i++) {
        box_cache_entry_t *entry = &mod.cache[i];

        if (!entry->tex) return entry;
        if (entry->ref_count > 0) continue;

        if (!oldest || entry->last_used < oldest->last_used) oldest = entry;
    }

    return oldest;
}

static box_cache_entry_t *cache_random_loaded(void) {
    int loaded = 0;

    for (int i = 0; i < BOX_CACHE_COUNT; i++) {
        if (mod.cache[i].tex) loaded++;
    }

    if (loaded == 0) return NULL;

    int chosen = saver_rand_range(loaded);
    for (int i = 0; i < BOX_CACHE_COUNT; i++) {
        box_cache_entry_t *entry = &mod.cache[i];
        if (!entry->tex) continue;

        if (chosen-- == 0) {
            cache_touch(entry);
            return entry;
        }
    }

    return NULL;
}

static box_cache_entry_t *cache_load_path(const int path_index) {
    if (path_index < 0 || path_index >= mod.path_count) return NULL;
    if (mod.bad_paths && mod.bad_paths[path_index]) return NULL;

    box_cache_entry_t *existing = cache_find_by_path(path_index);
    if (existing) {
        cache_touch(existing);
        return existing;
    }

    box_cache_entry_t *slot = cache_find_slot();
    if (!slot) return NULL;

    SDL_Surface *surf = IMG_Load(mod.paths[path_index]);
    if (!surf) {
        if (mod.bad_paths) mod.bad_paths[path_index] = 1;
        return NULL;
    }

    surf = limit_surface_size(surf);

    SDL_Texture *tex = SDL_CreateTextureFromSurface(mod.renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) return NULL;

    if (slot->tex) SDL_DestroyTexture(slot->tex);

    slot->tex = tex;
    slot->path_index = path_index;
    slot->ref_count = 0;
    SDL_QueryTexture(slot->tex, NULL, NULL, &slot->tex_w, &slot->tex_h);
    SDL_SetTextureBlendMode(slot->tex, SDL_BLENDMODE_BLEND);
    cache_touch(slot);

    return slot;
}

static box_cache_entry_t *cache_get_random(const int allow_load, int *loaded_new) {
    if (loaded_new) *loaded_new = 0;
    if (mod.path_count == 0) return NULL;

    const int start = saver_rand_range(mod.path_count);
    const int lookup_limit = mod.path_count < BOX_CACHE_LOOKUPS ? mod.path_count : BOX_CACHE_LOOKUPS;

    for (int attempt = 0; attempt < lookup_limit; attempt++) {
        const int idx = (start + attempt) % mod.path_count;
        box_cache_entry_t *entry = cache_find_by_path(idx);
        if (!entry) continue;

        cache_touch(entry);
        return entry;
    }

    if (!allow_load) return cache_random_loaded();

    const int load_limit = mod.path_count < BOX_LOAD_RETRIES ? mod.path_count : BOX_LOAD_RETRIES;
    for (int attempt = 0; attempt < load_limit; attempt++) {
        const int idx = (start + attempt) % mod.path_count;
        if (mod.bad_paths && mod.bad_paths[idx]) continue;

        box_cache_entry_t *entry = cache_find_by_path(idx);
        if (entry) {
            cache_touch(entry);
            return entry;
        }

        entry = cache_load_path(idx);
        if (!entry) continue;

        if (loaded_new) *loaded_new = 1;
        return entry;
    }

    return cache_random_loaded();
}

static void release_particle(box_particle_t *p) {
    if (!p) return;

    if (p->cache && p->cache->ref_count > 0) p->cache->ref_count--;

    p->cache = NULL;
    p->x = 0.0f;
    p->y = 0.0f;
    p->vy = 0.0f;
    p->display_h = 0.0f;
}

static void release_particles(void) {
    for (int i = 0; i < BOX_PARTICLE_COUNT; i++) {
        release_particle(&mod.particles[i]);
    }
}

static int particles_ready(void) {
    for (int i = 0; i < BOX_PARTICLE_COUNT; i++) {
        if (!mod.particles[i].cache) return 0;
    }

    return 1;
}

static int spawn_particle(box_particle_t *p, const int initial, const int allow_load, int *loaded_new) {
    const int sw = mod.base.screen_w;
    const int sh = mod.base.screen_h;

    box_cache_entry_t *entry = cache_get_random(allow_load, loaded_new);
    if (!entry) return 0;

    release_particle(p);
    p->cache = entry;
    p->cache->ref_count++;

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

    return 1;
}

static void spawn_missing_particles(void) {
    int spawns_left = BOX_SPAWNS_PER_UPDATE;
    int loads_left = BOX_LOADS_PER_UPDATE;

    for (int i = 0; i < BOX_PARTICLE_COUNT && spawns_left > 0; i++) {
        box_particle_t *p = &mod.particles[i];
        int loaded_new = 0;

        if (p->cache) continue;
        if (!spawn_particle(p, mod.initial_fill, loads_left > 0, &loaded_new)) continue;

        spawns_left--;
        if (loaded_new) loads_left--;
    }

    if (mod.initial_fill && particles_ready()) mod.initial_fill = 0;
}

static void on_idle_enter(void *user) {
    boxart_module_t *m = user;
    if (m->path_count == 0) return;

    release_particles();
    m->initial_fill = 1;
}

static void collect_box_images(const char *cat_path, char ***paths, int *count, int *cap) {
    if (!cat_path) return;

    DIR *sys_d = opendir(cat_path);
    if (!sys_d) return;

    struct dirent *sys_ent;
    while ((sys_ent = readdir(sys_d)) != NULL) {
        if (sys_ent->d_name[0] == '.') continue;
        if (is_excluded(sys_ent->d_name)) continue;

        char box_dir[4096];
        int written = snprintf(box_dir, sizeof(box_dir), "%s/%s/box", cat_path, sys_ent->d_name);
        if (written < 0 || written >= (int) sizeof(box_dir)) continue;

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
            for (k = 0; raw[k] && k < 15; k++) {
                lo[k] = (char) tolower((unsigned char) raw[k]);
            }
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
            written = snprintf(full, sizeof(full), "%s/%s", box_dir, img_ent->d_name);
            if (written < 0 || written >= (int) sizeof(full)) continue;

            char *dup = box_strdup(full);
            if (dup) (*paths)[(*count)++] = dup;
        }

        closedir(box_d);
    }

    closedir(sys_d);
}

int boxart_init(SDL_Renderer *renderer, const char *catalogue_path, const int screen_w, const int screen_h) {
    memset(&mod, 0, sizeof(mod));
    mod.renderer = renderer;

    for (int i = 0; i < BOX_CACHE_COUNT; i++) {
        mod.cache[i].path_index = -1;
    }

    char **paths = NULL;
    int count = 0;
    int cap = 0;

    collect_box_images(catalogue_path, &paths, &count, &cap);

    mod.paths = paths;
    mod.path_count = count;
    if (count > 0) mod.bad_paths = calloc((size_t) count, sizeof(unsigned char));

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

        if (!p->cache) continue;

        p->y -= p->vy * (float) elapsed;

        if (p->y + p->display_h * 0.5f < 0.0f) {
            release_particle(p);
        }
    }

    spawn_missing_particles();
}

void boxart_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;
    if (mod.path_count == 0) return;

    int order[BOX_PARTICLE_COUNT];
    for (int i = 0; i < BOX_PARTICLE_COUNT; i++) {
        order[i] = i;
    }

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
        const box_cache_entry_t *entry = p->cache;

        if (!entry || !entry->tex || entry->tex_h == 0) continue;

        const float aspect = (float) entry->tex_w / (float) entry->tex_h;
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

        SDL_SetTextureAlphaMod(entry->tex, (uint8_t) (alpha * 255.0f));
        SDL_RenderCopy(renderer, entry->tex, NULL, &rect);
    }
}

int boxart_active(void) {
    if (mod.path_count == 0) return 0;
    return saver_active_base(&mod.base);
}

void boxart_stop(void) {
    saver_stop_base(&mod.base);
    release_particles();
    mod.initial_fill = 0;
}

void boxart_shutdown(void) {
    release_particles();

    for (int i = 0; i < BOX_CACHE_COUNT; i++) {
        if (mod.cache[i].tex) {
            SDL_DestroyTexture(mod.cache[i].tex);
            mod.cache[i].tex = NULL;
        }
        mod.cache[i].path_index = -1;
        mod.cache[i].tex_w = 0;
        mod.cache[i].tex_h = 0;
        mod.cache[i].ref_count = 0;
        mod.cache[i].last_used = 0;
    }

    for (int i = 0; i < mod.path_count; i++) {
        free(mod.paths[i]);
    }
    free(mod.paths);
    free(mod.bad_paths);

    mod.paths = NULL;
    mod.bad_paths = NULL;
    mod.path_count = 0;
    mod.initial_fill = 0;
    mod.use_serial = 0;

    saver_shutdown_base(&mod.base);
}
