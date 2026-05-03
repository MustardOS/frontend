#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../log.h"
#include "../saver.h"
#include "dvd.h"

typedef struct {
    saver_state_t base;

    SDL_Texture *tex;
    SDL_Rect rect;

    int32_t fx, fy;
    int32_t vx, vy;
    int32_t speed_fp_per_ms;

    int32_t max_fx;
    int32_t max_fy;
} dvd_module_t;

static dvd_module_t mod = {0};

static void dvd_bounce_colour(void) {
    static int index = 0;
    static int last_index = -1;

    if (index == last_index) index = (index + 1) % SAVER_PASTEL_COUNT;

    uint8_t r, g, b;
    saver_pastel_pick(index, &r, &g, &b);

    if (mod.tex) SDL_SetTextureColorMod(mod.tex, r, g, b);
    LOG_INFO("saver", "DVD Colour Change: #%02X%02X%02X", r, g, b);

    last_index = index;
    index = (index + 1) % SAVER_PASTEL_COUNT;
}

static void dvd_recompute_layout(void) {
    int mx = mod.base.screen_w - mod.rect.w;
    int my = mod.base.screen_h - mod.rect.h;

    if (mx < 0) mx = 0;
    if (my < 0) my = 0;

    mod.max_fx = (int32_t) mx << SAVER_FRAME_SHF;
    mod.max_fy = (int32_t) my << SAVER_FRAME_SHF;
}

static void dvd_launch(void) {
    int sx = (random() & 1) ? 1 : -1;
    int sy = (random() & 1) ? 1 : -1;

    mod.vx = sx;
    mod.vy = sy;
}

static void dvd_on_speed_changed(void *user) {
    (void) user;

    int speed = mod.base.speed;
    if (speed < 1) speed = 1;

    mod.speed_fp_per_ms = (int32_t) ((speed * SAVER_FRAME_ONE) / 1000);
}

static void dvd_on_idle_enter(void *user) {
    (void) user;

    int max_x = mod.base.screen_w - mod.rect.w;
    int max_y = mod.base.screen_h - mod.rect.h;

    if (max_x < 1) max_x = 1;
    if (max_y < 1) max_y = 1;

    mod.fx = ((int32_t) saver_rand_range(max_x)) << SAVER_FRAME_SHF;
    mod.fy = ((int32_t) saver_rand_range(max_y)) << SAVER_FRAME_SHF;
    dvd_launch();

    if (mod.tex) SDL_SetTextureColorMod(mod.tex, 255, 255, 255);
}

int dvd_init(SDL_Renderer *renderer, const char *png_path, int screen_w, int screen_h) {
    SDL_Surface *surf = IMG_Load(png_path);
    if (!surf) {
        LOG_ERROR("saver", "DVD Bounce PNG load failed: %s", IMG_GetError());
        return 0;
    }

    mod.tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!mod.tex) return 0;

    SDL_QueryTexture(mod.tex, NULL, NULL, &mod.rect.w, &mod.rect.h);

    saver_init_base(&mod.base, screen_w, screen_h, "DVD Bounce", 255, 255, 255, dvd_on_speed_changed, dvd_on_idle_enter, &mod);

    dvd_on_speed_changed(NULL);
    dvd_recompute_layout();

    int max_x = screen_w - mod.rect.w;
    int max_y = screen_h - mod.rect.h;

    if (max_x < 1) max_x = 1;
    if (max_y < 1) max_y = 1;

    mod.fx = ((int32_t) saver_rand_range(max_x)) << SAVER_FRAME_SHF;
    mod.fy = ((int32_t) saver_rand_range(max_y)) << SAVER_FRAME_SHF;
    dvd_launch();

    mod.rect.x = mod.fx >> SAVER_FRAME_SHF;
    mod.rect.y = mod.fy >> SAVER_FRAME_SHF;

    SDL_SetTextureColorMod(mod.tex, 255, 255, 255);

    LOG_INFO("saver", "DVD Bounce Initialised (%dx%d), speed=%d",
             mod.rect.w, mod.rect.h, mod.base.speed);

    return 1;
}

void dvd_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    if (mod.speed_fp_per_ms == 0) return;
    int32_t step = mod.speed_fp_per_ms * (int32_t) elapsed;

    mod.fx += mod.vx * step;
    mod.fy += mod.vy * step;

    int bounced = 0;
    if (mod.fx <= 0) {
        mod.fx = 0;
        mod.vx = -mod.vx;
        bounced = 1;
    } else if (mod.fx >= mod.max_fx) {
        mod.fx = mod.max_fx;
        mod.vx = -mod.vx;
        bounced = 1;
    }

    if (mod.fy <= 0) {
        mod.fy = 0;
        mod.vy = -mod.vy;
        bounced = 1;
    } else if (mod.fy >= mod.max_fy) {
        mod.fy = mod.max_fy;
        mod.vy = -mod.vy;
        bounced = 1;
    }

    if (bounced && mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) dvd_bounce_colour();

    int x = mod.fx >> SAVER_FRAME_SHF;
    int y = mod.fy >> SAVER_FRAME_SHF;
    if (x != mod.rect.x) mod.rect.x = x;
    if (y != mod.rect.y) mod.rect.y = y;
}

void dvd_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.tex) return;

    SDL_SetTextureBlendMode(mod.tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(mod.tex, 255);
    SDL_RenderCopy(renderer, mod.tex, NULL, &mod.rect);

    if (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) {
        int range = mod.base.speed - SAVER_SPEED_COLOUR_THRESHOLD;
        if (range > 120) range = 120;

        const int min_boost = 32;
        const int max_boost = 96;
        int boost = min_boost + (range * (max_boost - min_boost) / 120);

        SDL_SetTextureBlendMode(mod.tex, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(mod.tex, (uint8_t) boost);
        SDL_RenderCopy(renderer, mod.tex, NULL, &mod.rect);

        SDL_SetTextureBlendMode(mod.tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(mod.tex, 255);
    }
}

int dvd_active(void) {
    return saver_active_base(&mod.base);
}

void dvd_stop(void) {
    saver_stop_base(&mod.base);
}

void dvd_shutdown(void) {
    if (mod.tex) {
        SDL_DestroyTexture(mod.tex);
        mod.tex = NULL;
    }

    saver_shutdown_base(&mod.base);
}
