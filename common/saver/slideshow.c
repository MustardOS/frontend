#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "../image.h"
#include "../log.h"
#include "../options.h"
#include "../saver.h"
#include "../language.h"
#include "slideshow.h"

#define CROSSFADE_MS 1500u

#define SLIDESHOW_FONT_FILE OPT_PATH "share/font/mucredits.ttf"

typedef struct {
    SDL_Texture *tex;
    int img_w, img_h;
    float start_x, start_y, start_w, start_h;
    float end_x, end_y, end_w, end_h;
    uint32_t start_ms;
} slide_t;

typedef struct {
    saver_state_t base;
    char **paths;
    int path_count;
    int current_idx;
    int next_idx;
    slide_t current;
    slide_t next;
    int fading;
    float fade_t;
    uint32_t slide_display_ms;
    SDL_Renderer *renderer;
    SDL_Texture *tex_empty;
    int empty_w, empty_h;
} slideshow_module_t;

static slideshow_module_t mod;

static float randf01(void) {
    return (float) saver_rand_range(10000) / 10000.0f;
}

static uint32_t compute_slide_ms(void) {
    int speed = saver_read_speed();
    if (speed <= 0) speed = SAVER_SPEED_DEFAULT;
    const uint32_t ms = 900000u / (uint32_t) speed;
    const uint32_t min_ms = CROSSFADE_MS + 1000u;
    return ms > min_ms ? ms : min_ms;
}

static void setup_kb(slide_t *s, const int screen_w, const int screen_h) {
    const float iw = (float) s->img_w;
    const float ih = (float) s->img_h;

    const float fw = (float) screen_w / iw;
    const float fh = (float) screen_h / ih;
    const float fill = fw > fh ? fw : fh;

    // Two scale tiers: lo = [1.0, 1.06], hi = [1.06, 1.12] relative to fill
    const float scale_lo = fill * (1.0f + randf01() * 0.06f);
    const float scale_hi = fill * (1.06f + randf01() * 0.06f);

    float sw, sh, ew, eh;
    if (saver_rand_range(2)) {
        sw = iw * scale_lo;
        sh = ih * scale_lo;
        ew = iw * scale_hi;
        eh = ih * scale_hi;
    } else {
        sw = iw * scale_hi;
        sh = ih * scale_hi;
        ew = iw * scale_lo;
        eh = ih * scale_lo;
    }

    // Pan offset: x in [-(w - screen_w), 0], y in [-(h - screen_h), 0]
    const float pan_x0 = sw > (float) screen_w ? randf01() * (sw - (float) screen_w) : 0.0f;
    const float pan_y0 = sh > (float) screen_h ? randf01() * (sh - (float) screen_h) : 0.0f;
    const float pan_x1 = ew > (float) screen_w ? randf01() * (ew - (float) screen_w) : 0.0f;
    const float pan_y1 = eh > (float) screen_h ? randf01() * (eh - (float) screen_h) : 0.0f;

    s->start_x = -pan_x0;
    s->start_y = -pan_y0;
    s->start_w = sw;
    s->start_h = sh;
    s->end_x = -pan_x1;
    s->end_y = -pan_y1;
    s->end_w = ew;
    s->end_h = eh;
}

static SDL_Rect kb_rect(const slide_t *s, const uint32_t now, const uint32_t display_ms) {
    float t = (float) (now - s->start_ms) / (float) display_ms;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    t = t * t * (3.0f - 2.0f * t);

    const float x = s->start_x + (s->end_x - s->start_x) * t;
    const float y = s->start_y + (s->end_y - s->start_y) * t;
    const float w = s->start_w + (s->end_w - s->start_w) * t;
    const float h = s->start_h + (s->end_h - s->start_h) * t;

    return (SDL_Rect) {(int) roundf(x), (int) roundf(y), (int) roundf(w), (int) roundf(h)};
}

static void load_slide(slide_t *s, SDL_Renderer *renderer, const char *path, const int screen_w, const int screen_h) {
    if (s->tex) SDL_DestroyTexture(s->tex);
    memset(s, 0, sizeof(*s));

    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        LOG_WARN("saver", "Slideshow: failed to load '%s': %s", path, IMG_GetError());
        return;
    }

    s->tex = SDL_CreateTextureFromSurface(renderer, surf);
    s->img_w = surf->w;
    s->img_h = surf->h;
    SDL_FreeSurface(surf);

    if (!s->tex) {
        LOG_WARN("saver", "Slideshow: texture creation failed for '%s'", path);
        return;
    }

    SDL_SetTextureBlendMode(s->tex, SDL_BLENDMODE_BLEND);
    setup_kb(s, screen_w, screen_h);
    s->start_ms = SDL_GetTicks();
}

static void free_slide(slide_t *s) {
    if (s->tex) SDL_DestroyTexture(s->tex);
    memset(s, 0, sizeof(*s));
}

static void build_empty_message(SDL_Renderer *renderer, const int screen_w, const int screen_h) {
    if (!TTF_WasInit() && TTF_Init() != 0) {
        LOG_ERROR("saver", "Slideshow: TTF_Init failed: %s", TTF_GetError());
        return;
    }

    int font_size = screen_h / 20;
    if (font_size < 14) font_size = 14;

    TTF_Font *font = TTF_OpenFont(SLIDESHOW_FONT_FILE, font_size);
    if (!font) {
        LOG_ERROR("saver", "Slideshow: failed to open font %s: %s", SLIDESHOW_FONT_FILE, TTF_GetError());
        return;
    }

    TTF_SetFontWrappedAlign(font, TTF_WRAPPED_ALIGN_CENTER);

    const int margin_x = (int) ((float) screen_w * 0.1f);
    int wrap_w = screen_w - margin_x * 2;
    if (wrap_w < 32) wrap_w = 32;

    const SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended_Wrapped(font, lang.generic.no_slideshow_image, white, wrap_w);
    if (surf) {
        mod.tex_empty = SDL_CreateTextureFromSurface(renderer, surf);
        mod.empty_w = surf->w;
        mod.empty_h = surf->h;
        SDL_FreeSurface(surf);
    } else {
        LOG_ERROR("saver", "Slideshow: failed to render empty message: %s", TTF_GetError());
    }

    TTF_CloseFont(font);
}

static void render_empty_state(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, NULL);

    if (!mod.tex_empty) return;

    const SDL_Rect dst = {
        (mod.base.screen_w - mod.empty_w) / 2, (mod.base.screen_h - mod.empty_h) / 2, mod.empty_w, mod.empty_h
    };
    SDL_RenderCopy(renderer, mod.tex_empty, NULL, &dst);
}

static void on_idle_enter(void *user) {
    slideshow_module_t *m = user;
    if (m->path_count == 0) return;

    free_slide(&m->next);
    m->fading = 0;
    m->fade_t = 0.0f;

    load_slide(&m->current, m->renderer, m->paths[m->current_idx], m->base.screen_w, m->base.screen_h);
    m->slide_display_ms = compute_slide_ms();
}

static int path_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *) a, *(const char *const *) b);
}

static void scan_dir(const char *dir, char ***paths, int *count, int *cap) {
    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type != DT_REG && entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN) continue;

        const char *dot = strrchr(entry->d_name, '.');
        if (!dot) continue;
        char lo[16];
        int k;
        const char *raw = dot + 1;
        for (k = 0; raw[k] && k < 15; k++)
            lo[k] = (char) tolower((unsigned char) raw[k]);
        lo[k] = '\0';
        if (!is_image_ext(lo)) continue;

        if (*count >= *cap) {
            const int new_cap = *cap > 0 ? *cap * 2 : 16;
            char **tmp = realloc(*paths, (size_t) new_cap * sizeof(char *));
            if (!tmp) break;
            *paths = tmp;
            *cap = new_cap;
        }

        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, entry->d_name);
        char *dup = strdup(full);
        if (dup) (*paths)[(*count)++] = dup;
    }
    closedir(d);
}

int slideshow_init(
    SDL_Renderer *renderer, const char *const *dirs, const int dir_count, const int screen_w, const int screen_h
) {
    memset(&mod, 0, sizeof(mod));
    mod.renderer = renderer;

    char **paths = NULL;
    int count = 0, cap = 0;

    for (int i = 0; i < dir_count; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(dirs[i], dirs[j]) == 0) {
                dup = 1;
                break;
            }
        }
        if (!dup) scan_dir(dirs[i], &paths, &count, &cap);
    }

    if (count > 0) {
        qsort(paths, (size_t) count, sizeof(char *), path_cmp);

        int unique = 1;
        for (int i = 1; i < count; i++) {
            if (strcmp(paths[i], paths[i - 1]) != 0) {
                paths[unique++] = paths[i];
            } else {
                free(paths[i]);
            }
        }
        count = unique;
    }

    mod.paths = paths;
    mod.path_count = count;
    mod.current_idx = count > 0 ? saver_rand_range(count) : 0;
    mod.next_idx = count > 1 ? (mod.current_idx + 1) % count : 0;

    if (count == 0) build_empty_message(renderer, screen_w, screen_h);

    saver_init_base(&mod.base, screen_w, screen_h, "Image Slideshow", 0, 0, 0, NULL, on_idle_enter, &mod);

    LOG_INFO(
        "saver", "Slideshow: found %d image%s across %d director%s", count, count == 1 ? "" : "s", dir_count,
        dir_count == 1 ? "y" : "ies"
    );

    return 1;
}

void slideshow_update(void) {
    const uint32_t now = SDL_GetTicks();

    if (!saver_poll_idle(&mod.base, now)) return;
    if (mod.path_count == 0 || !mod.current.tex) return;

    const uint32_t elapsed = now - mod.current.start_ms;
    const uint32_t fade_start = mod.slide_display_ms > CROSSFADE_MS ? mod.slide_display_ms - CROSSFADE_MS : 0;

    if (!mod.fading && elapsed >= fade_start) {
        mod.next_idx = (mod.current_idx + 1) % mod.path_count;
        load_slide(&mod.next, mod.renderer, mod.paths[mod.next_idx], mod.base.screen_w, mod.base.screen_h);
        mod.fading = 1;
        mod.fade_t = 0.0f;
    }

    if (mod.fading) {
        const uint32_t fade_elapsed = elapsed > fade_start ? elapsed - fade_start : 0;
        mod.fade_t = (float) fade_elapsed / (float) CROSSFADE_MS;

        if (mod.fade_t >= 1.0f) {
            free_slide(&mod.current);
            mod.current = mod.next;
            memset(&mod.next, 0, sizeof(mod.next));
            mod.current_idx = mod.next_idx;
            mod.fading = 0;
            mod.fade_t = 0.0f;
            mod.slide_display_ms = compute_slide_ms();
        }
    }

    mod.base.last_tick = now;
}

void slideshow_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;

    if (mod.path_count == 0) {
        render_empty_state(renderer);
        return;
    }

    if (!mod.current.tex) return;

    const uint32_t now = SDL_GetTicks();

    const SDL_Rect cr = kb_rect(&mod.current, now, mod.slide_display_ms);
    SDL_SetTextureAlphaMod(mod.current.tex, 255);
    SDL_RenderCopy(renderer, mod.current.tex, NULL, &cr);

    if (mod.fading && mod.next.tex) {
        const float ft = mod.fade_t < 0.0f ? 0.0f : mod.fade_t > 1.0f ? 1.0f : mod.fade_t;
        const SDL_Rect nr = kb_rect(&mod.next, now, mod.slide_display_ms);
        SDL_SetTextureAlphaMod(mod.next.tex, (uint8_t) (ft * 255.0f));
        SDL_RenderCopy(renderer, mod.next.tex, NULL, &nr);
    }
}

int slideshow_active(void) {
    return saver_active_base(&mod.base);
}

void slideshow_stop(void) {
    saver_stop_base(&mod.base);
}

void slideshow_shutdown(void) {
    free_slide(&mod.current);
    free_slide(&mod.next);

    if (mod.tex_empty) {
        SDL_DestroyTexture(mod.tex_empty);
        mod.tex_empty = NULL;
    }

    for (int i = 0; i < mod.path_count; i++)
        free(mod.paths[i]);

    free(mod.paths);
    mod.paths = NULL;
    mod.path_count = 0;

    saver_shutdown_base(&mod.base);
}
