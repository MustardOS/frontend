#include <math.h>
#include <time.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../../../../common/log.h"
#include "../../../../common/options.h"
#include "../../../../common/common.h"
#include "dvd.h"

#define DVD_SPEED 120.0f

typedef struct {
    SDL_Texture *tex;
    SDL_Rect rect;

    float fx;
    float fy;
    float vx;
    float vy;

    int screen_w;
    int screen_h;

    int enabled;
    int idle_active;

    uint32_t last_tick;
    uint32_t last_idle_poll;
    int was_idle_active;
} dvd_state_t;

static dvd_state_t dvd = {0};
static uint32_t energy = 0;

static void dvd_launch(void) {
    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
        seeded = 1;
    }

    float angle = ((float) random() / (float) RAND_MAX) * 2.0f * (float) M_PI;

    dvd.vx = cosf(angle) * DVD_SPEED;
    dvd.vy = sinf(angle) * DVD_SPEED;

    if (fabsf(dvd.vx) < 20.0f) dvd.vx = copysignf(20.0f, dvd.vx);
    if (fabsf(dvd.vy) < 20.0f) dvd.vy = copysignf(20.0f, dvd.vy);
}

int dvd_init(SDL_Renderer *renderer, const char *png_path, int screen_w, int screen_h) {
    SDL_Surface *surf = IMG_Load(png_path);
    if (!surf) {
        LOG_ERROR("saver", "Screensaver PNG load failed: %s", IMG_GetError());
        return false;
    }

    dvd.tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    if (!dvd.tex) return false;

    SDL_QueryTexture(dvd.tex, NULL, NULL, &dvd.rect.w, &dvd.rect.h);

    dvd.screen_w = screen_w;
    dvd.screen_h = screen_h;

    dvd.fx = (float) (random() % (dvd.screen_w - dvd.rect.w));
    dvd.fy = (float) (random() % (dvd.screen_h - dvd.rect.h));

    dvd_launch();

    dvd.rect.x = (int) dvd.fx;
    dvd.rect.y = (int) dvd.fy;

    dvd.enabled = true;
    dvd.idle_active = false;

    dvd.last_tick = SDL_GetTicks();
    dvd.last_idle_poll = 0;

    dvd.idle_active = false;
    dvd.was_idle_active = false;

    LOG_INFO("saver", "Screensaver initialised (%dx%d)", dvd.rect.w, dvd.rect.h);
    return true;
}

void dvd_update(void) {
    if (!dvd.enabled) return;
    uint32_t now = SDL_GetTicks();

    if (now - dvd.last_idle_poll >= TIMER_IDLE) {
        dvd.was_idle_active = dvd.idle_active;
        dvd.idle_active = read_line_int_from(IDLE_STATE, 1);
        dvd.last_idle_poll = now;

        if (dvd.idle_active && !dvd.was_idle_active) {
            dvd.fx = (float) (random() % (dvd.screen_w - dvd.rect.w));
            dvd.fy = (float) (random() % (dvd.screen_h - dvd.rect.h));

            dvd_launch();
            dvd.last_tick = now;
            energy = 0;
        }
    }

    if (!dvd.idle_active) {
        dvd.last_tick = now;
        return;
    }

    uint32_t elapsed = now - dvd.last_tick;
    dvd.last_tick = now;

    energy += elapsed;

    while (energy >= IDLE_MS) {
        float dt = IDLE_MS / 1000.0f;

        dvd.fx += dvd.vx * dt;
        dvd.fy += dvd.vy * dt;

        const float max_x = (float) dvd.screen_w - (float) dvd.rect.w;
        const float max_y = (float) dvd.screen_h - (float) dvd.rect.h;

        if (dvd.fx <= 0.0f) {
            dvd.fx = 0.0f;
            dvd.vx = -dvd.vx;
        } else if (dvd.fx >= max_x) {
            dvd.fx = max_x;
            dvd.vx = -dvd.vx;
        }

        if (dvd.fy <= 0.0f) {
            dvd.fy = 0.0f;
            dvd.vy = -dvd.vy;
        } else if (dvd.fy >= max_y) {
            dvd.fy = max_y;
            dvd.vy = -dvd.vy;
        }

        energy -= IDLE_MS;
    }

    dvd.rect.x = (int) (dvd.fx + 0.5f);
    dvd.rect.y = (int) (dvd.fy + 0.5f);
}

void dvd_render(SDL_Renderer *renderer) {
    if (!dvd.enabled || !dvd.idle_active) return;

    SDL_FRect dvd_rect = {dvd.fx, dvd.fy, (float) dvd.rect.w, (float) dvd.rect.h};
    SDL_RenderCopyF(renderer, dvd.tex, NULL, &dvd_rect);
}

int dvd_active(void) {
    return dvd.enabled && dvd.idle_active;
}

void dvd_shutdown(void) {
    if (dvd.tex) {
        SDL_DestroyTexture(dvd.tex);
        dvd.tex = NULL;
    }

    dvd.enabled = false;
    LOG_INFO("saver", "Screensaver shutdown");
}
