#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../log.h"
#include "../common.h"
#include "dvd.h"

#define FRAME_SHF 16
#define FRAME_ONE (1 << FRAME_SHF)

#define DVD_SPEED_PATH "/opt/muos/config/settings/power/screensaver"
#define DVD_SPEED_DEFAULT 90
#define DVD_SPEED_COLOUR 600

static uint32_t suppress_until = 0;

typedef struct {
    SDL_Texture *tex;
    SDL_Rect rect;

    int32_t fx;
    int32_t fy;
    int32_t vx;
    int32_t vy;

    int screen_w;
    int screen_h;

    int enabled;
    int idle_active;

    uint32_t last_tick;
    uint32_t last_idle_poll;
    int was_idle_active;

    int dvd_speed;
    int32_t speed_fp_per_ms;

    uint8_t colour_r;
    uint8_t colour_g;
    uint8_t colour_b;
} dvd_state_t;

static dvd_state_t dvd = {0};

static int read_dvd_speed(void) {
    if (!file_exist(DVD_SPEED_PATH)) return DVD_SPEED_DEFAULT;
    return read_line_int_from(DVD_SPEED_PATH, 1);
}

static void dvd_bounce_colour(void) {
    static const struct {
        uint8_t r, g, b;
    } palette[] = {
            {255, 255, 255}, // Original...
            {255, 182, 193}, // Pastel Pink
            {255, 179, 128}, // Pastel Orange
            {255, 255, 153}, // Pastel Yellow
            {178, 255, 178}, // Pastel Green
            {153, 255, 255}, // Pastel Cyan
            {179, 204, 255}, // Pastel Blue
            {204, 153, 255}, // Pastel Purple
            {255, 153, 204}, // Pastel Rose
            {255, 218, 153}, // Pastel Peach
            {153, 255, 220}, // Pastel Mint
    };
    static const int palette_len = (int) (sizeof palette / sizeof palette[0]);

    static int index = 0;
    static int last_index = -1;

    if (index == last_index) index = (index + 1) % palette_len;

    dvd.colour_r = palette[index].r;
    dvd.colour_g = palette[index].g;
    dvd.colour_b = palette[index].b;

    if (dvd.tex) SDL_SetTextureColorMod(dvd.tex, palette[index].r, palette[index].g, palette[index].b);
    LOG_INFO("saver", "DVD Colour Change: #%02X%02X%02X", palette[index].r, palette[index].g, palette[index].b);

    last_index = index;
    index = (index + 1) % palette_len;
}

static void dvd_launch(void) {
    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
        seeded = 1;
    }

    int sx = (random() & 1) ? 1 : -1;
    int sy = (random() & 1) ? 1 : -1;

    dvd.vx = sx;
    dvd.vy = sy;
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

    int max_x = dvd.screen_w - dvd.rect.w;
    int max_y = dvd.screen_h - dvd.rect.h;

    if (max_x < 1) max_x = 1;
    if (max_y < 1) max_y = 1;

    dvd.fx = ((int32_t) (random() % max_x)) << FRAME_SHF;
    dvd.fy = ((int32_t) (random() % max_y)) << FRAME_SHF;

    dvd_launch();

    dvd.rect.x = dvd.fx >> FRAME_SHF;
    dvd.rect.y = dvd.fy >> FRAME_SHF;

    dvd.enabled = true;
    dvd.idle_active = false;

    dvd.colour_r = 255;
    dvd.colour_g = 255;
    dvd.colour_b = 255;

    SDL_SetTextureColorMod(dvd.tex, 255, 255, 255);

    dvd.last_tick = SDL_GetTicks();
    dvd.last_idle_poll = 0;

    dvd.was_idle_active = false;

    dvd.dvd_speed = read_dvd_speed();
    dvd.speed_fp_per_ms = (int32_t) ((dvd.dvd_speed * FRAME_ONE) / 1000);

    LOG_INFO("saver", "Screensaver Initialised (%dx%d), speed=%d", dvd.rect.w, dvd.rect.h, dvd.dvd_speed);
    return true;
}

void dvd_update(void) {
    if (!dvd.enabled) return;

    static int32_t cached_max_fx = 0;
    static int32_t cached_max_fy = 0;

    static int cached_w = 0;
    static int cached_h = 0;
    static int cached_rw = 0;
    static int cached_rh = 0;

    if (dvd.screen_w != cached_w || dvd.screen_h != cached_h ||
        dvd.rect.w != cached_rw || dvd.rect.h != cached_rh) {

        cached_w = dvd.screen_w;
        cached_h = dvd.screen_h;
        cached_rw = dvd.rect.w;
        cached_rh = dvd.rect.h;

        int mx = dvd.screen_w - dvd.rect.w;
        int my = dvd.screen_h - dvd.rect.h;

        if (mx < 0) mx = 0;
        if (my < 0) my = 0;

        cached_max_fx = (int32_t) mx << FRAME_SHF;
        cached_max_fy = (int32_t) my << FRAME_SHF;
    }

    uint32_t now = SDL_GetTicks();

    if (now < suppress_until) {
        dvd.idle_active = 0;
        dvd.was_idle_active = 0;
        dvd.last_tick = now;
        dvd.last_idle_poll = now;

        return;
    }

    if (now - dvd.last_idle_poll >= TIMER_IDLE) {
        dvd.was_idle_active = dvd.idle_active;
        dvd.idle_active = read_line_int_from(IDLE_STATE, 1);
        dvd.last_idle_poll = now;

        if (dvd.idle_active && !dvd.was_idle_active) {
            int new_speed = read_dvd_speed();
            if (new_speed != dvd.dvd_speed) {
                dvd.dvd_speed = new_speed;
                dvd.speed_fp_per_ms = (int32_t) ((dvd.dvd_speed * FRAME_ONE) / 1000);

                LOG_INFO("saver", "DVD Speed Refreshed: %d", dvd.dvd_speed);
            }

            int max_x = dvd.screen_w - dvd.rect.w;
            int max_y = dvd.screen_h - dvd.rect.h;

            if (max_x < 1) max_x = 1;
            if (max_y < 1) max_y = 1;

            dvd.fx = ((int32_t) (random() % max_x)) << FRAME_SHF;
            dvd.fy = ((int32_t) (random() % max_y)) << FRAME_SHF;

            dvd_launch();
            dvd.last_tick = now;

            dvd.colour_r = 255;
            dvd.colour_g = 255;
            dvd.colour_b = 255;

            if (dvd.tex) SDL_SetTextureColorMod(dvd.tex, 255, 255, 255);
        }
    }

    if (!dvd.idle_active) {
        dvd.last_tick = now;
        return;
    }

    uint32_t elapsed = now - dvd.last_tick;
    if (!elapsed) return;

    dvd.last_tick = now;

    if (dvd.speed_fp_per_ms == 0) return;
    int32_t step = dvd.speed_fp_per_ms * (int32_t) elapsed;

    dvd.fx += dvd.vx * step;
    dvd.fy += dvd.vy * step;

    if (dvd.fx <= 0) {
        dvd.fx = 0;
        dvd.vx = -dvd.vx;
        if (dvd.dvd_speed >= DVD_SPEED_COLOUR) dvd_bounce_colour();
    } else if (dvd.fx >= cached_max_fx) {
        dvd.fx = cached_max_fx;
        dvd.vx = -dvd.vx;
        if (dvd.dvd_speed >= DVD_SPEED_COLOUR) dvd_bounce_colour();
    }

    if (dvd.fy <= 0) {
        dvd.fy = 0;
        dvd.vy = -dvd.vy;
        if (dvd.dvd_speed >= DVD_SPEED_COLOUR) dvd_bounce_colour();
    } else if (dvd.fy >= cached_max_fy) {
        dvd.fy = cached_max_fy;
        dvd.vy = -dvd.vy;
        if (dvd.dvd_speed >= DVD_SPEED_COLOUR) dvd_bounce_colour();
    }

    int x = dvd.fx >> FRAME_SHF;
    int y = dvd.fy >> FRAME_SHF;

    if (x != dvd.rect.x) dvd.rect.x = x;
    if (y != dvd.rect.y) dvd.rect.y = y;
}

void dvd_render(SDL_Renderer *renderer) {
    if (!dvd.enabled || !dvd.idle_active || !dvd.tex) return;

    SDL_SetTextureBlendMode(dvd.tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(dvd.tex, 255);
    SDL_RenderCopy(renderer, dvd.tex, NULL, &dvd.rect);

    int boost_alpha = 0;

    if (dvd.dvd_speed >= DVD_SPEED_COLOUR) {
        int range = dvd.dvd_speed - DVD_SPEED_COLOUR;

        const int min_boost = 32;
        const int max_boost = 96;

        if (range > 120) range = 120;
        boost_alpha = min_boost + (range * (max_boost - min_boost) / 120);
    }

    if (boost_alpha > 0) {
        SDL_SetTextureBlendMode(dvd.tex, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(dvd.tex, (uint8_t) boost_alpha);

        SDL_RenderCopy(renderer, dvd.tex, NULL, &dvd.rect);

        SDL_SetTextureBlendMode(dvd.tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(dvd.tex, 255);
    }
}

int dvd_active(void) {
    if (suppress_until && SDL_GetTicks() < suppress_until) return 0;
    return dvd.enabled && dvd.idle_active;
}

void dvd_stop(void) {
    dvd.idle_active = 0;
    dvd.was_idle_active = 0;

    suppress_until = SDL_GetTicks() + SCREENSAVER_DELAY;
}

void dvd_shutdown(void) {
    if (dvd.tex) {
        SDL_DestroyTexture(dvd.tex);
        dvd.tex = NULL;
    }

    dvd.enabled = false;
    suppress_until = 0;

    LOG_INFO("saver", "Screensaver Shutdown");
}