#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../log.h"
#include "../saver.h"
#include "mystify.h"

#define MYST_VERT_COUNT   4
#define MYST_TRAIL_LEN    24
#define MYST_POLY_MAX     4
#define MYST_POLY_NORMAL  2
#define MYST_VEL_BASE_DEN 90

#define MYST_COLOUR_HOLD_MIN_MS 4500
#define MYST_COLOUR_HOLD_MAX_MS 9500

typedef struct {
    int32_t fx, fy;
    int32_t vx, vy;
} myst_vert_t;

typedef struct {
    int active;
    myst_vert_t vert[MYST_VERT_COUNT];

    int16_t trail_x[MYST_TRAIL_LEN][MYST_VERT_COUNT];
    int16_t trail_y[MYST_TRAIL_LEN][MYST_VERT_COUNT];

    uint8_t trail_r[MYST_TRAIL_LEN];
    uint8_t trail_g[MYST_TRAIL_LEN];
    uint8_t trail_b[MYST_TRAIL_LEN];

    uint8_t trail_head;
    uint8_t trail_count;

    uint8_t r, g, b;
    int32_t colour_remaining_ms;
} myst_poly_t;

typedef struct {
    saver_state_t base;

    myst_poly_t poly[MYST_POLY_MAX];
    int poly_count;

    int32_t vel_fp_per_ms;

    int32_t snapshot_acc_ms;
    int32_t snapshot_interval_ms;
} mystify_module_t;

static mystify_module_t mod = {0};

static const uint8_t myst_palette[][3] = {
        {255, 64,  255}, // Magenta
        {120, 255, 60},  // Lime
        {255, 100, 100}, // Red
        {80,  200, 255}, // Blue
        {255, 200, 60},  // Gold
        {180, 120, 255}, // Violet
        {0,   255, 200}, // Aqua
        {255, 140, 60},  // Orange
};
#define MYST_PALETTE_COUNT ((int)(sizeof(myst_palette) / sizeof(myst_palette[0])))

static void pick_new_colour(myst_poly_t *p) {
    int idx;

    for (int tries = 0; tries < 8; tries++) {
        idx = (int) saver_rand_range(MYST_PALETTE_COUNT);
        if (myst_palette[idx][0] != p->r ||
            myst_palette[idx][1] != p->g ||
            myst_palette[idx][2] != p->b)
            break;
    }

    p->r = myst_palette[idx][0];
    p->g = myst_palette[idx][1];
    p->b = myst_palette[idx][2];

    int span = MYST_COLOUR_HOLD_MAX_MS - MYST_COLOUR_HOLD_MIN_MS;
    p->colour_remaining_ms = MYST_COLOUR_HOLD_MIN_MS + (int) saver_rand_range(span);
}

static void poly_spawn(myst_poly_t *p, int colour_offset) {
    p->active = 1;
    p->trail_head = 0;
    p->trail_count = 0;

    int margin = 32;

    int max_x = mod.base.screen_w - margin * 2;
    int max_y = mod.base.screen_h - margin * 2;

    if (max_x < 1) max_x = 1;
    if (max_y < 1) max_y = 1;

    for (int i = 0; i < MYST_VERT_COUNT; i++) {
        int x = margin + (int) saver_rand_range(max_x);
        int y = margin + (int) saver_rand_range(max_y);

        p->vert[i].fx = ((int32_t) x) << SAVER_FRAME_SHF;
        p->vert[i].fy = ((int32_t) y) << SAVER_FRAME_SHF;

        int vx_mag = 40 + (int) saver_rand_range(50);
        int vy_mag = 40 + (int) saver_rand_range(50);

        if (saver_rand_range(2)) vx_mag = -vx_mag;
        if (saver_rand_range(2)) vy_mag = -vy_mag;

        p->vert[i].vx = vx_mag;
        p->vert[i].vy = vy_mag;
    }

    int seed_idx = colour_offset % MYST_PALETTE_COUNT;
    p->r = myst_palette[seed_idx][0];
    p->g = myst_palette[seed_idx][1];
    p->b = myst_palette[seed_idx][2];

    int span = MYST_COLOUR_HOLD_MAX_MS - MYST_COLOUR_HOLD_MIN_MS;
    p->colour_remaining_ms = MYST_COLOUR_HOLD_MIN_MS + (int) saver_rand_range(span);
}

static void seed_all(void) {
    int count = (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) ? MYST_POLY_MAX : MYST_POLY_NORMAL;
    mod.poly_count = count;

    for (int i = 0; i < MYST_POLY_MAX; i++) mod.poly[i].active = 0;
    for (int i = 0; i < count; i++) poly_spawn(&mod.poly[i], i);
}

static void on_speed_changed(void *user) {
    (void) user;
    int speed = mod.base.speed;
    if (speed < 1) speed = 1;

    int scaled = (speed * 1024) / MYST_VEL_BASE_DEN;
    if (scaled < 64) scaled = 64;
    if (scaled > 8192) scaled = 8192;
    mod.vel_fp_per_ms = (int32_t) scaled;

    int interval = (80 * MYST_VEL_BASE_DEN) / speed;
    if (interval < 20) interval = 20;
    if (interval > 400) interval = 400;

    mod.snapshot_interval_ms = (int32_t) interval;
}

static void on_idle_enter(void *user) {
    (void) user;
    seed_all();
}

int mystify_init(SDL_Renderer *renderer, int screen_w, int screen_h) {
    (void) renderer;

    saver_init_base(&mod.base, screen_w, screen_h, "Mystify", 255, 255, 255, on_speed_changed, on_idle_enter, &mod);

    on_speed_changed(NULL);
    seed_all();

    LOG_INFO("saver", "Mystify Initialised (%dx%d, polys=%d, speed=%d)",
             screen_w, screen_h, mod.poly_count, mod.base.speed);

    return 1;
}

static void integrate_poly(myst_poly_t *p, uint32_t elapsed, int push_snapshot) {
    int32_t max_fx = ((int32_t) mod.base.screen_w - 1) << SAVER_FRAME_SHF;
    int32_t max_fy = ((int32_t) mod.base.screen_h - 1) << SAVER_FRAME_SHF;

    if (max_fx < 0) max_fx = 0;
    if (max_fy < 0) max_fy = 0;

    int32_t time_factor = ((int32_t) elapsed * SAVER_FRAME_ONE) / 1000;
    int32_t scale_q10 = mod.vel_fp_per_ms;

    for (int i = 0; i < MYST_VERT_COUNT; i++) {
        myst_vert_t *v = &p->vert[i];

        int32_t dx = (int32_t) (((int64_t) v->vx * time_factor * scale_q10) >> 10);
        int32_t dy = (int32_t) (((int64_t) v->vy * time_factor * scale_q10) >> 10);

        v->fx += dx;
        v->fy += dy;

        if (v->fx < 0) {
            v->fx = -v->fx;
            v->vx = saver_int_abs(v->vx);
        } else if (v->fx > max_fx) {
            v->fx = max_fx - (v->fx - max_fx);
            v->vx = -saver_int_abs(v->vx);
        }

        if (v->fy < 0) {
            v->fy = -v->fy;
            v->vy = saver_int_abs(v->vy);
        } else if (v->fy > max_fy) {
            v->fy = max_fy - (v->fy - max_fy);
            v->vy = -saver_int_abs(v->vy);
        }
    }

    if (push_snapshot) {
        p->trail_head = (uint8_t) ((p->trail_head + 1) % MYST_TRAIL_LEN);

        for (int i = 0; i < MYST_VERT_COUNT; i++) {
            p->trail_x[p->trail_head][i] = (int16_t) (p->vert[i].fx >> SAVER_FRAME_SHF);
            p->trail_y[p->trail_head][i] = (int16_t) (p->vert[i].fy >> SAVER_FRAME_SHF);
        }

        p->trail_r[p->trail_head] = p->r;
        p->trail_g[p->trail_head] = p->g;
        p->trail_b[p->trail_head] = p->b;

        if (p->trail_count < MYST_TRAIL_LEN) p->trail_count++;
    }

    p->colour_remaining_ms -= (int32_t) elapsed;
    if (p->colour_remaining_ms <= 0) pick_new_colour(p);
}

void mystify_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    if (elapsed > 100) elapsed = 100;

    mod.snapshot_acc_ms += (int32_t) elapsed;
    int push_snapshot = 0;
    if (mod.snapshot_acc_ms >= mod.snapshot_interval_ms) {
        push_snapshot = 1;
        mod.snapshot_acc_ms -= mod.snapshot_interval_ms;
        if (mod.snapshot_acc_ms > mod.snapshot_interval_ms) {
            mod.snapshot_acc_ms = mod.snapshot_interval_ms;
        }
    }

    for (int i = 0; i < mod.poly_count; i++) {
        myst_poly_t *p = &mod.poly[i];
        if (!p->active) continue;
        integrate_poly(p, elapsed, push_snapshot);
    }
}

static void render_polygon_outline(SDL_Renderer *renderer, const int16_t *xs, const int16_t *ys, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) {
    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);

    for (int i = 0; i < MYST_VERT_COUNT; i++) {
        int next = (i + 1) % MYST_VERT_COUNT;
        SDL_RenderDrawLine(renderer, xs[i], ys[i], xs[next], ys[next]);
    }
}

void mystify_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int pi = 0; pi < mod.poly_count; pi++) {
        const myst_poly_t *p = &mod.poly[pi];
        if (!p->active || p->trail_count == 0) continue;

        int count = p->trail_count;
        int oldest_idx;

        if (count == MYST_TRAIL_LEN) {
            oldest_idx = (p->trail_head + 1) % MYST_TRAIL_LEN;
        } else {
            oldest_idx = 0;
        }

        for (int t = 0; t < count; t++) {
            int idx = (oldest_idx + t) % MYST_TRAIL_LEN;

            int factor = (t * 1024) / count;
            int alpha = 80 + (factor * 150) / 1024;

            render_polygon_outline(renderer, p->trail_x[idx], p->trail_y[idx], p->trail_r[idx], p->trail_g[idx], p->trail_b[idx], (uint8_t) alpha);
        }

        int16_t live_x[MYST_VERT_COUNT], live_y[MYST_VERT_COUNT];
        for (int i = 0; i < MYST_VERT_COUNT; i++) {
            live_x[i] = (int16_t) (p->vert[i].fx >> SAVER_FRAME_SHF);
            live_y[i] = (int16_t) (p->vert[i].fy >> SAVER_FRAME_SHF);
        }

        render_polygon_outline(renderer, live_x, live_y, p->r, p->g, p->b, 240);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int mystify_active(void) {
    return saver_active_base(&mod.base);
}

void mystify_stop(void) {
    saver_stop_base(&mod.base);
}

void mystify_shutdown(void) {
    saver_shutdown_base(&mod.base);
}
