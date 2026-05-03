#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../log.h"
#include "../saver.h"
#include "pulse.h"

#define PULSE_RING_COUNT 8
#define PULSE_MIN_CELL   12
#define PULSE_MAX_CELL   36

typedef struct {
    int active;
    int origin_x, origin_y;
    int32_t radius;
    int32_t max_radius;
    int32_t speed;
    uint8_t r, g, b;
} ring_t;

typedef struct {
    saver_state_t base;
    ring_t ring[PULSE_RING_COUNT];
    int cell;
    int cols;
    int rows;

    uint8_t *shimmer_phase;

    uint32_t last_spawn;
    int32_t shimmer_clock;
} pulse_module_t;

static pulse_module_t mod = {0};

static int approx_distance(int x1, int y1, int x2, int y2) {
    int dx = saver_int_abs(x1 - x2);
    int dy = saver_int_abs(y1 - y2);

    int hi = dx > dy ? dx : dy;
    int lo = dx > dy ? dy : dx;

    return hi + ((lo * 3) >> 3);
}

static void apply_ring_colour(ring_t *r) {
    saver_pastel_pick((int) saver_rand_range(SAVER_PASTEL_COUNT), &r->r, &r->g, &r->b);
}

static void spawn_ring(void) {
    int slot = -1;
    for (int i = 0; i < PULSE_RING_COUNT; i++) {
        if (!mod.ring[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        int32_t largest = -1;
        for (int i = 0; i < PULSE_RING_COUNT; i++) {
            if (mod.ring[i].radius > largest) {
                largest = mod.ring[i].radius;
                slot = i;
            }
        }
    }

    ring_t *r = &mod.ring[slot];
    int max_x = mod.base.screen_w > 1 ? mod.base.screen_w : 1;
    int max_y = mod.base.screen_h > 1 ? mod.base.screen_h : 1;

    r->active = 1;
    r->origin_x = (int) saver_rand_range(max_x);
    r->origin_y = (int) saver_rand_range(max_y);
    r->radius = 0;

    int corner_a = approx_distance(r->origin_x, r->origin_y, 0, 0);
    int corner_b = approx_distance(r->origin_x, r->origin_y, mod.base.screen_w, 0);
    int corner_c = approx_distance(r->origin_x, r->origin_y, 0, mod.base.screen_h);
    int corner_d = approx_distance(r->origin_x, r->origin_y, mod.base.screen_w, mod.base.screen_h);

    int max_radius = corner_a;
    if (corner_b > max_radius) max_radius = corner_b;
    if (corner_c > max_radius) max_radius = corner_c;
    if (corner_d > max_radius) max_radius = corner_d;

    r->max_radius = (max_radius + mod.cell * 2) << SAVER_FRAME_SHF;

    int speed = mod.base.speed;
    if (speed < 1) speed = 1;
    int varied = speed + (int) saver_rand_range(speed + 1);

    r->speed = (int32_t) ((varied * SAVER_FRAME_ONE) / 1000);
    if (r->speed < (SAVER_FRAME_ONE / 32)) r->speed = SAVER_FRAME_ONE / 32;

    apply_ring_colour(r);
}

static void seed_all(void) {
    for (int i = 0; i < PULSE_RING_COUNT; i++) mod.ring[i].active = 0;
    spawn_ring();
    spawn_ring();
}

static void rebuild_grid(void) {
    int shortest = mod.base.screen_w < mod.base.screen_h ? mod.base.screen_w : mod.base.screen_h;
    mod.cell = shortest / 36;

    if (mod.cell < PULSE_MIN_CELL) mod.cell = PULSE_MIN_CELL;
    if (mod.cell > PULSE_MAX_CELL) mod.cell = PULSE_MAX_CELL;

    mod.cols = (mod.base.screen_w + mod.cell - 1) / mod.cell;
    mod.rows = (mod.base.screen_h + mod.cell - 1) / mod.cell;

    if (mod.cols < 1) mod.cols = 1;
    if (mod.rows < 1) mod.rows = 1;

    free(mod.shimmer_phase);
    size_t cell_total = (size_t) mod.cols * (size_t) mod.rows;
    mod.shimmer_phase = malloc(cell_total);

    if (mod.shimmer_phase) {
        for (size_t i = 0; i < cell_total; i++) {
            mod.shimmer_phase[i] = (uint8_t) saver_rand_range(256);
        }
    }
}

static void on_speed_changed(void *user) { (void) user; }

static void on_idle_enter(void *user) {
    (void) user;
    seed_all();
}

int pulse_init(SDL_Renderer *renderer, int screen_w, int screen_h) {
    (void) renderer;

    saver_init_base(&mod.base, screen_w, screen_h, "Pulse Grid", 153, 255, 220, on_speed_changed, on_idle_enter, &mod);

    rebuild_grid();
    seed_all();

    mod.last_spawn = SDL_GetTicks();
    mod.shimmer_clock = 0;

    LOG_INFO("saver", "Pulse Grid Initialised (%dx%d, grid=%dx%d, cell=%d, speed=%d)",
             screen_w, screen_h, mod.cols, mod.rows, mod.cell, mod.base.speed);
    return 1;
}

void pulse_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) {
        mod.last_spawn = now;
        return;
    }

    uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;

    mod.base.last_tick = now;
    mod.shimmer_clock += (int32_t) elapsed;

    for (int i = 0; i < PULSE_RING_COUNT; i++) {
        ring_t *r = &mod.ring[i];
        if (!r->active) continue;

        r->radius += r->speed * (int32_t) elapsed;
        if (r->radius > r->max_radius) r->active = 0;
    }

    uint32_t spawn_delay = 1400;
    if (mod.base.speed > 0) {
        int scaled = 100000 / mod.base.speed;
        if (scaled < 280) scaled = 280;
        if (scaled > 1800) scaled = 1800;
        spawn_delay = (uint32_t) scaled;
    }

    if (now - mod.last_spawn >= spawn_delay) {
        spawn_ring();
        mod.last_spawn = now;
    }
}

static inline int tri256(int v) {
    v &= 511;
    return v < 256 ? v : 511 - v;
}

void pulse_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int half_cell = mod.cell / 2;
    int ring_width = mod.cell * 2;
    if (ring_width < 1) ring_width = 1;

    int shimmer_t = (mod.shimmer_clock / 12) & 511; /* slow drift */
    if (mod.shimmer_phase) {
        for (int gy = 0; gy < mod.rows; gy++) {
            int y = gy * mod.cell;
            for (int gx = 0; gx < mod.cols; gx++) {
                int x = gx * mod.cell;

                int phase = mod.shimmer_phase[gy * mod.cols + gx];
                int wave = tri256((shimmer_t + phase * 2));
                int alpha = 4 + (wave * 14) / 256;

                int inset = mod.cell / 5;
                if (inset < 1) inset = 1;

                SDL_Rect rect = {x + inset, y + inset, mod.cell - inset * 2, mod.cell - inset * 2};

                if (rect.w < 1) rect.w = 1;
                if (rect.h < 1) rect.h = 1;

                SDL_SetRenderDrawColor(renderer, mod.base.colour_r, mod.base.colour_g, mod.base.colour_b, (uint8_t) alpha);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    for (int i = 0; i < PULSE_RING_COUNT; i++) {
        const ring_t *r = &mod.ring[i];
        if (!r->active) continue;

        int radius = r->radius >> SAVER_FRAME_SHF;
        int outer = radius + ring_width;
        int inner = radius - ring_width;
        if (inner < 0) inner = 0;

        int x0 = r->origin_x - outer;
        int y0 = r->origin_y - outer;
        int x1 = r->origin_x + outer;
        int y1 = r->origin_y + outer;

        int gx0 = x0 / mod.cell;
        if (gx0 < 0) gx0 = 0;

        int gy0 = y0 / mod.cell;
        if (gy0 < 0) gy0 = 0;

        int gx1 = x1 / mod.cell;
        if (gx1 >= mod.cols) gx1 = mod.cols - 1;

        int gy1 = y1 / mod.cell;
        if (gy1 >= mod.rows) gy1 = mod.rows - 1;

        if (gx1 < gx0 || gy1 < gy0) continue;

        int outer_sq = outer * outer;

        for (int gy = gy0; gy <= gy1; gy++) {
            int cy = gy * mod.cell + half_cell;
            int dy = cy - r->origin_y;
            int dy_sq = dy * dy;
            if (dy_sq > outer_sq) continue;

            for (int gx = gx0; gx <= gx1; gx++) {
                int cx = gx * mod.cell + half_cell;
                int dx = cx - r->origin_x;
                if (dx * dx + dy_sq > outer_sq) continue;

                int dist = approx_distance(cx, cy, r->origin_x, r->origin_y);
                int delta = saver_int_abs(dist - radius);
                if (delta > ring_width) continue;

                int hit = 255 - ((delta * 255) / ring_width);
                if (hit < 1) continue;

                int x = gx * mod.cell;
                int y = gy * mod.cell;

                int inset = mod.cell / 5;
                if (inset < 1) inset = 1;

                SDL_Rect rect = {x + inset, y + inset, mod.cell - inset * 2, mod.cell - inset * 2};

                if (rect.w < 1) rect.w = 1;
                if (rect.h < 1) rect.h = 1;

                SDL_SetRenderDrawColor(renderer, r->r, r->g, r->b, (uint8_t) hit);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int pulse_active(void) {
    return saver_active_base(&mod.base);
}

void pulse_stop(void) {
    saver_stop_base(&mod.base);
}

void pulse_shutdown(void) {
    free(mod.shimmer_phase);
    mod.shimmer_phase = NULL;
    saver_shutdown_base(&mod.base);
}
