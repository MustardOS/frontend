#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "../log.h"
#include "../saver.h"
#include "star.h"

#define STAR_Z_SCALE 8
#define STAR_Z_NEAR  32
#define STAR_Z_FAR   512

#define STAR_MAX_SIZE  6
#define STAR_TRAIL_LEN 8
#define STAR_COUNT     192
#define STAR_SPREAD    256

#define STAR_DRIFTER_COUNT 1024

typedef struct {
    int32_t z;
    int32_t dx, dy;
    uint8_t r, g, b;
    int16_t trail_x[STAR_TRAIL_LEN];
    int16_t trail_y[STAR_TRAIL_LEN];
    uint8_t trail_head;
    uint8_t trail_count;
} star_t;

typedef struct {
    int32_t fx, fy;
    int32_t vx, vy;
    uint8_t size;
    uint8_t alpha;
    uint8_t r, g, b;
} drifter_t;

typedef struct {
    saver_state_t base;
    star_t stars[STAR_COUNT];
    drifter_t drifter[STAR_DRIFTER_COUNT];
    int focal;
    int32_t z_step_per_ms;
    int32_t drifter_speed_fp_per_ms;
} star_module_t;

static star_module_t mod = {0};

static void apply_star_colour(star_t *s) {
    if (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) {
        saver_pastel_pick((int) saver_rand_range(SAVER_PASTEL_COUNT), &s->r, &s->g, &s->b);
    } else {
        s->r = mod.base.colour_r;
        s->g = mod.base.colour_g;
        s->b = mod.base.colour_b;
    }
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

static void star_reset(star_t *s, int randomise_z) {
    int32_t spread = STAR_SPREAD;
    int32_t rx, ry;
    int tries = 0;

    do {
        rx = saver_rand_range(spread * 2) - spread;
        ry = saver_rand_range(spread * 2) - spread;
        tries++;
    } while ((rx * rx + ry * ry) < (spread / 8) * (spread / 8) && tries < 4);

    if (rx == 0 && ry == 0) rx = spread / 8;

    s->dx = rx;
    s->dy = ry;

    if (randomise_z) {
        s->z = ((int32_t) (STAR_Z_NEAR + saver_rand_range(STAR_Z_FAR - STAR_Z_NEAR))) << SAVER_FRAME_SHF;
    } else {
        s->z = ((int32_t) STAR_Z_FAR) << SAVER_FRAME_SHF;
    }

    apply_star_colour(s);
    trail_clear(s);
}

static void apply_drifter_colour(drifter_t *d) {
    if (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) {
        saver_pastel_pick((int) saver_rand_range(SAVER_PASTEL_COUNT), &d->r, &d->g, &d->b);
    } else {
        d->r = mod.base.colour_r;
        d->g = mod.base.colour_g;
        d->b = mod.base.colour_b;
    }
}

static void drifter_spawn(drifter_t *d, int interior) {
    int cx = mod.base.screen_w / 2;
    int cy = mod.base.screen_h / 2;

    int half_w = mod.base.screen_w / 2;
    int half_h = mod.base.screen_h / 2;

    int shorter = mod.base.screen_w < mod.base.screen_h ? mod.base.screen_w : mod.base.screen_h;

    int x = cx;
    int y = cy;

    int min_dist;
    int max_dist;

#define DRIFTER_RAND_INT(LIMIT)                                   \
    (                                                             \
        (LIMIT) <= 1 ? 0 :                                        \
        ({                                                        \
            int _limit = (LIMIT);                                 \
            int _value = 0;                                       \
            uint32_t _pick = saver_rand_range((uint32_t) _limit); \
            while (_pick > 0U) {                                  \
                _value++;                                         \
                _pick--;                                          \
            }                                                     \
            _value;                                               \
        })                                                        \
    )

    if (interior) {
        min_dist = 64;
        max_dist = 1500;
    } else {
        min_dist = 48 + DRIFTER_RAND_INT(160);
        max_dist = 280 + DRIFTER_RAND_INT(360);
    }

    for (int tries = 0; tries < 48; tries++) {
        int px = DRIFTER_RAND_INT(mod.base.screen_w);
        int py = DRIFTER_RAND_INT(mod.base.screen_h);

        int dx = px - cx;
        int dy = py - cy;

        int nx = half_w > 0 ? (dx * 1024) / half_w : 0;
        int ny = half_h > 0 ? (dy * 1024) / half_h : 0;

        int dist = (nx * nx + ny * ny) >> 10;

        if (dist >= min_dist && dist <= max_dist) {
            x = px;
            y = py;
            break;
        }

        if (tries == 47) {
            x = px;
            y = py;
        }
    }

    d->fx = ((int32_t) x) << SAVER_FRAME_SHF;
    d->fy = ((int32_t) y) << SAVER_FRAME_SHF;

    int dx = x - cx;
    int dy = y - cy;

    if (dx == 0 && dy == 0) {
        int range = shorter > 1 ? shorter : 2;
        int offset = range / 2;

        dx = DRIFTER_RAND_INT(range) - offset;
        dy = DRIFTER_RAND_INT(range) - offset;

        if (dx == 0 && dy == 0) dx = 1;
    }

    int ax = dx < 0 ? -dx : dx;
    int ay = dy < 0 ? -dy : dy;

    int norm = ax > ay ? ax : ay;
    if (norm < 1) norm = 1;

    int speed_mag = 5 + DRIFTER_RAND_INT(18);

    d->vx = (int32_t) ((dx * speed_mag) / norm);
    d->vy = (int32_t) ((dy * speed_mag) / norm);

    d->vx += (int32_t) (DRIFTER_RAND_INT(7) - 3);
    d->vy += (int32_t) (DRIFTER_RAND_INT(7) - 3);

    if (d->vx == 0 && d->vy == 0) d->vx = dx < 0 ? -1 : 1;

    d->size = (uint8_t) (1 + (DRIFTER_RAND_INT(5) == 0 ? 1 : 0));
    d->alpha = (uint8_t) (48 + DRIFTER_RAND_INT(96));

    apply_drifter_colour(d);

#undef DRIFTER_RAND_INT
}

static void seed_drifters(void) {
    for (int i = 0; i < STAR_DRIFTER_COUNT; i++) drifter_spawn(&mod.drifter[i], 1);
}

static void seed_all(void) {
    for (int i = 0; i < STAR_COUNT; i++) star_reset(&mod.stars[i], 1);
    seed_drifters();
}

static void on_speed_changed(void *user) {
    (void) user;

    int speed = mod.base.speed;
    if (speed < 1) speed = 1;

    mod.z_step_per_ms = (int32_t) ((speed * STAR_Z_SCALE * SAVER_FRAME_ONE) / 1000);
    mod.drifter_speed_fp_per_ms = (int32_t) ((speed * SAVER_FRAME_ONE) / 4000);

    if (mod.drifter_speed_fp_per_ms < 1) mod.drifter_speed_fp_per_ms = 1;
}

static void on_idle_enter(void *user) {
    (void) user;
    seed_all();
}

int star_init(SDL_Renderer *renderer, int screen_w, int screen_h) {
    (void) renderer;

    int focal = (screen_w < screen_h) ? screen_w : screen_h;
    mod.focal = focal / 2;
    if (mod.focal < 1) mod.focal = 1;

    saver_init_base(&mod.base, screen_w, screen_h, "Starfield", 255, 255, 255, on_speed_changed, on_idle_enter, &mod);

    on_speed_changed(NULL);
    seed_all();

    LOG_INFO("saver", "Starfield Initialised (%dx%d, focal=%d, speed=%d)",
             screen_w, screen_h, mod.focal, mod.base.speed);

    return 1;
}

void star_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    if (mod.z_step_per_ms == 0) return;
    int32_t dz = mod.z_step_per_ms * (int32_t) elapsed;

    int32_t z_near_fp = ((int32_t) STAR_Z_NEAR) << SAVER_FRAME_SHF;
    int half_w = mod.base.screen_w / 2;
    int half_h = mod.base.screen_h / 2;

    for (int i = 0; i < STAR_COUNT; i++) {
        star_t *s = &mod.stars[i];

        s->z -= dz;
        if (s->z <= z_near_fp) {
            star_reset(s, 0);
            continue;
        }

        int32_t z_world = s->z >> SAVER_FRAME_SHF;
        if (z_world <= 0) {
            star_reset(s, 0);
            continue;
        }

        int sx = half_w + (int) ((int64_t) s->dx * mod.focal / z_world);
        int sy = half_h + (int) ((int64_t) s->dy * mod.focal / z_world);

        if (sx < -STAR_MAX_SIZE || sx > mod.base.screen_w + STAR_MAX_SIZE || sy < -STAR_MAX_SIZE || sy > mod.base.screen_h + STAR_MAX_SIZE) {
            star_reset(s, 0);
            continue;
        }

        trail_push(s, sx, sy);
    }

    int32_t drift_step = mod.drifter_speed_fp_per_ms * (int32_t) elapsed;

    int margin = STAR_MAX_SIZE * 4;

    int32_t max_fx = ((int32_t) (mod.base.screen_w + margin)) << SAVER_FRAME_SHF;
    int32_t max_fy = ((int32_t) (mod.base.screen_h + margin)) << SAVER_FRAME_SHF;

    int32_t min_fx = ((int32_t) -margin) << SAVER_FRAME_SHF;
    int32_t min_fy = ((int32_t) -margin) << SAVER_FRAME_SHF;

    for (int i = 0; i < STAR_DRIFTER_COUNT; i++) {
        drifter_t *d = &mod.drifter[i];
        d->fx += (d->vx * drift_step) / 100;
        d->fy += (d->vy * drift_step) / 100;

        if (d->fx < min_fx || d->fx > max_fx ||
            d->fy < min_fy || d->fy > max_fy) {
            drifter_spawn(d, 0);
        }
    }
}

void star_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < STAR_DRIFTER_COUNT; i++) {
        const drifter_t *d = &mod.drifter[i];

        int x = d->fx >> SAVER_FRAME_SHF;
        int y = d->fy >> SAVER_FRAME_SHF;

        if (x < 0 || x >= mod.base.screen_w || y < 0 || y >= mod.base.screen_h) continue;

        SDL_SetRenderDrawColor(renderer, d->r, d->g, d->b, d->alpha);
        if (d->size <= 1) {
            SDL_RenderDrawPoint(renderer, x, y);
        } else {
            SDL_Rect rect = {x, y, d->size, d->size};
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    int32_t z_near_fp = ((int32_t) STAR_Z_NEAR) << SAVER_FRAME_SHF;
    int32_t z_far_fp = ((int32_t) STAR_Z_FAR) << SAVER_FRAME_SHF;
    int32_t z_span_fp = z_far_fp - z_near_fp;
    if (z_span_fp <= 0) z_span_fp = 1;

    int draw_trails = (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD);

    for (int i = 0; i < STAR_COUNT; i++) {
        const star_t *s = &mod.stars[i];
        if (s->trail_count == 0) continue;

        int32_t z_world = s->z >> SAVER_FRAME_SHF;
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

        if (sx + size < 0 || sx >= mod.base.screen_w || sy + size < 0 || sy >= mod.base.screen_h) continue;

        SDL_SetRenderDrawColor(renderer, s->r, s->g, s->b, (uint8_t) alpha);
        SDL_Rect rect = {sx - size / 2, sy - size / 2, size, size};
        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int star_active(void) {
    return saver_active_base(&mod.base);
}

void star_stop(void) {
    saver_stop_base(&mod.base);
}

void star_shutdown(void) {
    saver_shutdown_base(&mod.base);
}
