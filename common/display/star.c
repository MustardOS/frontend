#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <SDL2/SDL.h>
#include "../log.h"
#include "../common.h"
#include "star.h"

#define FRAME_SHF 16
#define FRAME_ONE (1 << FRAME_SHF)

#define STAR_Z_SCALE 4
#define STAR_Z_NEAR  16
#define STAR_Z_FAR   1024

#define STAR_MAX_SIZE  6
#define STAR_TRAIL_LEN 8
#define STAR_COUNT     192
#define STAR_SPREAD    256

#define STAR_SPEED_PATH      "/opt/muos/config/settings/power/saver_speed"
#define STAR_COLOUR_FILE_FMT "%s/colour/screensaver_star"

#define STAR_SPEED_DEFAULT   90
#define STAR_SPEED_COLOUR    600

static uint32_t suppress_until = 0;

typedef struct {
    int32_t z;

    int32_t dx;
    int32_t dy;

    uint8_t r;
    uint8_t g;
    uint8_t b;

    int16_t trail_x[STAR_TRAIL_LEN];
    int16_t trail_y[STAR_TRAIL_LEN];

    uint8_t trail_head;
    uint8_t trail_count;
} star_t;

typedef struct {
    star_t stars[STAR_COUNT];

    int screen_w;
    int screen_h;
    int focal;

    int enabled;
    int idle_active;

    uint32_t last_tick;
    uint32_t last_idle_poll;
    int was_idle_active;

    int star_speed;
    int32_t z_step_per_ms;

    uint8_t colour_r;
    uint8_t colour_g;
    uint8_t colour_b;
} star_state_t;

static star_state_t saver = {0};

static int read_star_speed(void) {
    if (!file_exist(STAR_SPEED_PATH)) return STAR_SPEED_DEFAULT;
    return read_line_int_from(STAR_SPEED_PATH, 1);
}

static void load_theme_colour(void) {
    saver.colour_r = 255;
    saver.colour_g = 255;
    saver.colour_b = 255;

    if (!theme_base || !*theme_base) return;

    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), STAR_COLOUR_FILE_FMT, theme_base);

    if (!file_exist(path)) return;

    char *line = read_line_char_from(path, 1);
    if (!line) return;

    const char *s = line;
    if (*s == '#') s++;
    if (*s == '\0') return;

    if (*s == '+' || *s == '-') return;

    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(s, &end, 16);

    if (end == s) return;
    if (errno == ERANGE) return;
    if (parsed > 0xFFFFFFul) return;

    while (*end != '\0') {
        if (!isspace((unsigned char) *end)) return;
        end++;
    }

    saver.colour_r = (uint8_t) ((parsed >> 16) & 0xFF);
    saver.colour_g = (uint8_t) ((parsed >> 8) & 0xFF);
    saver.colour_b = (uint8_t) (parsed & 0xFF);
    LOG_INFO("saver", "Starfield colour from theme: #%02X%02X%02X", saver.colour_r, saver.colour_g, saver.colour_b);
}

static int32_t rand_range(int32_t range) {
    if (range <= 0) return 0;
    return (int32_t) (random() % (uint32_t) range);
}

static void trail_clear(star_t *s) {
    s->trail_head = 0;
    s->trail_count = 0;
}

static void trail_push(star_t *s, int x, int y) {
    s->trail_head = (uint8_t) ((s->trail_head + 1) % STAR_TRAIL_LEN);
    s->trail_x[s->trail_head] = (int16_t) x;
    s->trail_y[s->trail_head] = (int16_t) y;
    if (s->trail_count < STAR_TRAIL_LEN) s->trail_count++;
}

static void apply_star_colour(star_t *s) {
    if (saver.star_speed >= STAR_SPEED_COLOUR) {
        static const struct {
            uint8_t r, g, b;
        } palette[] = {
                {255, 255, 255}, // White
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
        const int palette_len = (int) (sizeof palette / sizeof palette[0]);

        int idx = (int) rand_range(palette_len);
        s->r = palette[idx].r;
        s->g = palette[idx].g;
        s->b = palette[idx].b;
    } else {
        s->r = saver.colour_r;
        s->g = saver.colour_g;
        s->b = saver.colour_b;
    }
}

static void star_reset(star_t *s, int randomise_z) {
    int32_t spread = STAR_SPREAD;

    int32_t rx;
    int32_t ry;

    int tries = 0;

    do {
        rx = rand_range(spread * 2) - spread;
        ry = rand_range(spread * 2) - spread;
        tries++;
    } while ((rx * rx + ry * ry) < (spread / 8) * (spread / 8) && tries < 4);

    if (rx == 0 && ry == 0) rx = spread / 8;

    s->dx = rx;
    s->dy = ry;

    if (randomise_z) {
        s->z = ((int32_t) (STAR_Z_NEAR + rand_range(STAR_Z_FAR - STAR_Z_NEAR))) << FRAME_SHF;
    } else {
        s->z = ((int32_t) STAR_Z_FAR) << FRAME_SHF;
    }

    apply_star_colour(s);
    trail_clear(s);
}

static void stars_seed_all(void) {
    for (int i = 0; i < STAR_COUNT; i++) star_reset(&saver.stars[i], 1);
}

int star_init(SDL_Renderer *renderer, int screen_w, int screen_h) {
    (void) renderer;

    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
        seeded = 1;
    }

    saver.screen_w = screen_w;
    saver.screen_h = screen_h;

    int focal = (screen_w < screen_h) ? screen_w : screen_h;
    saver.focal = focal / 2;
    if (saver.focal < 1) saver.focal = 1;

    saver.enabled = true;
    saver.idle_active = false;

    saver.last_tick = SDL_GetTicks();
    saver.last_idle_poll = 0;
    saver.was_idle_active = false;

    saver.star_speed = read_star_speed();
    saver.z_step_per_ms = (int32_t) ((saver.star_speed * STAR_Z_SCALE * FRAME_ONE) / 1000);

    load_theme_colour();
    stars_seed_all();

    LOG_INFO("saver", "Starfield Initialised (%dx%d, focal=%d, speed=%d)", screen_w, screen_h, saver.focal, saver.star_speed);
    return true;
}

void star_update(void) {
    if (!saver.enabled) return;

    uint32_t now = SDL_GetTicks();

    if (now < suppress_until) {
        saver.idle_active = 0;
        saver.was_idle_active = 0;
        saver.last_tick = now;
        saver.last_idle_poll = now;
        return;
    }

    if (now - saver.last_idle_poll >= TIMER_IDLE) {
        saver.was_idle_active = saver.idle_active;
        saver.idle_active = read_line_int_from(IDLE_STATE, 1);
        saver.last_idle_poll = now;

        if (saver.idle_active && !saver.was_idle_active) {
            int new_speed = read_star_speed();
            if (new_speed != saver.star_speed) {
                saver.star_speed = new_speed;
                saver.z_step_per_ms = (int32_t) ((saver.star_speed * STAR_Z_SCALE * FRAME_ONE) / 1000);
                LOG_INFO("saver", "Starfield Speed Refreshed: %d", saver.star_speed);
            }

            load_theme_colour();
            stars_seed_all();
            saver.last_tick = now;
        }
    }

    if (!saver.idle_active) {
        saver.last_tick = now;
        return;
    }

    uint32_t elapsed = now - saver.last_tick;
    if (!elapsed) return;
    saver.last_tick = now;

    if (saver.z_step_per_ms == 0) return;
    int32_t dz = saver.z_step_per_ms * (int32_t) elapsed;

    int32_t z_near_fp = ((int32_t) STAR_Z_NEAR) << FRAME_SHF;

    for (int i = 0; i < STAR_COUNT; i++) {
        star_t *s = &saver.stars[i];

        s->z -= dz;
        if (s->z <= z_near_fp) {
            star_reset(s, 0);
            continue;
        }

        int32_t z_world = s->z >> FRAME_SHF;
        if (z_world <= 0) {
            star_reset(s, 0);
            continue;
        }

        int sx = saver.screen_w / 2 + (int) ((int64_t) s->dx * saver.focal / z_world);
        int sy = saver.screen_h / 2 + (int) ((int64_t) s->dy * saver.focal / z_world);

        if (sx < -STAR_MAX_SIZE || sx > saver.screen_w + STAR_MAX_SIZE ||
            sy < -STAR_MAX_SIZE || sy > saver.screen_h + STAR_MAX_SIZE) {
            star_reset(s, 0);
            continue;
        }

        trail_push(s, sx, sy);
    }
}

void star_render(SDL_Renderer *renderer) {
    if (!saver.enabled || !saver.idle_active) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int32_t z_near_fp = ((int32_t) STAR_Z_NEAR) << FRAME_SHF;
    int32_t z_far_fp = ((int32_t) STAR_Z_FAR) << FRAME_SHF;
    int32_t z_span_fp = z_far_fp - z_near_fp;
    if (z_span_fp <= 0) z_span_fp = 1;

    int draw_trails = (saver.star_speed >= STAR_SPEED_COLOUR);

    for (int i = 0; i < STAR_COUNT; i++) {
        const star_t *s = &saver.stars[i];

        if (s->trail_count == 0) continue;

        int32_t z_world = s->z >> FRAME_SHF;
        if (z_world <= 0) continue;

        int32_t depth_fp = z_far_fp - s->z;
        if (depth_fp < 0) depth_fp = 0;
        if (depth_fp > z_span_fp) depth_fp = z_span_fp;

        int size = 1 + (int) ((int64_t) depth_fp * (STAR_MAX_SIZE - 1) / z_span_fp);
        int alpha = 32 + (int) ((int64_t) depth_fp * (255 - 32) / z_span_fp);

        int sx = s->trail_x[s->trail_head];
        int sy = s->trail_y[s->trail_head];

        if (draw_trails && s->trail_count > 1) {
            int cur_idx = s->trail_head;

            for (int seg = 0; seg < s->trail_count - 1; seg++) {
                int prev_idx = (cur_idx + STAR_TRAIL_LEN - 1) % STAR_TRAIL_LEN;
                int seg_alpha = alpha - (alpha * 7 * seg / (STAR_TRAIL_LEN * 8));
                if (seg_alpha < 8) seg_alpha = 8;

                int x1 = s->trail_x[cur_idx];
                int y1 = s->trail_y[cur_idx];
                int x2 = s->trail_x[prev_idx];
                int y2 = s->trail_y[prev_idx];

                SDL_SetRenderDrawColor(renderer, s->r, s->g, s->b, (uint8_t) seg_alpha);
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);

                cur_idx = prev_idx;
            }
        }

        if (sx + size < 0 || sx >= saver.screen_w || sy + size < 0 || sy >= saver.screen_h) continue;

        SDL_SetRenderDrawColor(renderer, s->r, s->g, s->b, (uint8_t) alpha);
        SDL_Rect rect = {sx - size / 2, sy - size / 2, size, size};
        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int star_active(void) {
    if (suppress_until && SDL_GetTicks() < suppress_until) return 0;
    return saver.enabled && saver.idle_active;
}

void star_stop(void) {
    saver.idle_active = 0;
    saver.was_idle_active = 0;

    suppress_until = SDL_GetTicks() + SCREENSAVER_DELAY;
}

void star_shutdown(void) {
    saver.enabled = false;
    suppress_until = 0;

    LOG_INFO("saver", "Starfield Shutdown");
}
