#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "../log.h"
#include "../saver.h"
#include "constellation.h"

#define CONSTELLATION_POINT_COUNT 64
#define CONSTELLATION_LINK_DIST   118
#define CONSTELLATION_MAX_SIZE    4

typedef struct {
    int32_t fx, fy;
    int32_t vx, vy;
    int32_t target_vx, target_vy;
    uint32_t heading_due;
    uint8_t size;
    uint8_t r, g, b;
} cpoint_t;

typedef struct {
    saver_state_t base;
    cpoint_t point[CONSTELLATION_POINT_COUNT];
    int32_t speed_fp_per_ms;
    int edge_margin;
} constellation_module_t;

static constellation_module_t mod = {0};

static int32_t random_velocity(void) {
    int v = 10 + (int) saver_rand_range(42);
    if (saver_rand_range(2)) v = -v;

    return (int32_t) v;
}

static void apply_point_colour(cpoint_t *p) {
    if (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) {
        saver_pastel_pick((int) saver_rand_range(SAVER_PASTEL_COUNT), &p->r, &p->g, &p->b);
    } else {
        p->r = mod.base.colour_r;
        p->g = mod.base.colour_g;
        p->b = mod.base.colour_b;
    }
}

static void roll_target(cpoint_t *p, uint32_t now) {
    int v = 50;

    int tx = (int) saver_rand_range(v * 2 + 1) - v;
    int ty = (int) saver_rand_range(v * 2 + 1) - v;

    int x = p->fx >> SAVER_FRAME_SHF;
    int y = p->fy >> SAVER_FRAME_SHF;

    int M = mod.edge_margin;

    if (x < M && tx < 0) tx = -tx / 2;
    if (x > mod.base.screen_w - M && tx > 0) tx = -tx / 2;

    if (y < M && ty < 0) ty = -ty / 2;
    if (y > mod.base.screen_h - M && ty > 0) ty = -ty / 2;

    p->target_vx = (int32_t) tx;
    p->target_vy = (int32_t) ty;

    p->heading_due = now + 2500 + (uint32_t) saver_rand_range(4000);
}

static void reset_point(cpoint_t *p, uint32_t now) {
    int max_x = mod.base.screen_w > 1 ? mod.base.screen_w : 1;
    int max_y = mod.base.screen_h > 1 ? mod.base.screen_h : 1;

    p->fx = saver_rand_range(max_x) << SAVER_FRAME_SHF;
    p->fy = saver_rand_range(max_y) << SAVER_FRAME_SHF;

    p->vx = random_velocity();
    p->vy = random_velocity();

    p->size = (uint8_t) (1 + saver_rand_range(CONSTELLATION_MAX_SIZE));

    roll_target(p, now);
    apply_point_colour(p);
}

static void seed_all(void) {
    uint32_t now = SDL_GetTicks();
    for (int i = 0; i < CONSTELLATION_POINT_COUNT; i++) reset_point(&mod.point[i], now);
}

static void on_speed_changed(void *user) {
    (void) user;

    int speed = mod.base.speed;
    if (speed < 1) speed = 1;

    mod.speed_fp_per_ms = (int32_t) ((speed * SAVER_FRAME_ONE) / 1000);
}

static void on_idle_enter(void *user) {
    (void) user;
    seed_all();
}

int constellation_init(SDL_Renderer *renderer, int screen_w, int screen_h) {
    (void) renderer;

    int shorter = screen_w < screen_h ? screen_w : screen_h;
    mod.edge_margin = shorter / 10;

    if (mod.edge_margin < 16) mod.edge_margin = 16;
    if (mod.edge_margin > 64) mod.edge_margin = 64;

    saver_init_base(&mod.base, screen_w, screen_h, "Constellation", 179, 204, 255, on_speed_changed, on_idle_enter, &mod);

    on_speed_changed(NULL);
    seed_all();

    LOG_INFO("saver", "Constellation Initialised (%dx%d, points=%d, speed=%d)",
             screen_w, screen_h, CONSTELLATION_POINT_COUNT, mod.base.speed);

    return 1;
}

void constellation_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    if (mod.speed_fp_per_ms == 0) return;

    int32_t max_fx = ((int32_t) mod.base.screen_w - 1) << SAVER_FRAME_SHF;
    int32_t max_fy = ((int32_t) mod.base.screen_h - 1) << SAVER_FRAME_SHF;

    if (max_fx < 0) max_fx = 0;
    if (max_fy < 0) max_fy = 0;

    int32_t step_fp = mod.speed_fp_per_ms * (int32_t) elapsed;

    for (int i = 0; i < CONSTELLATION_POINT_COUNT; i++) {
        cpoint_t *p = &mod.point[i];

        int32_t turn = step_fp >> (SAVER_FRAME_SHF + 1);
        if (turn > 32) turn = 32;

        p->vx += ((p->target_vx - p->vx) * turn) >> 6;
        p->vy += ((p->target_vy - p->vy) * turn) >> 6;

        if (now >= p->heading_due) roll_target(p, now);

        int x = p->fx >> SAVER_FRAME_SHF;
        int y = p->fy >> SAVER_FRAME_SHF;
        int M = mod.edge_margin;

        if (x < M) p->vx += (M - x) * 2 / M;
        else if (x > mod.base.screen_w - M) p->vx -= (x - (mod.base.screen_w - M)) * 2 / M;
        if (y < M) p->vy += (M - y) * 2 / M;
        else if (y > mod.base.screen_h - M) p->vy -= (y - (mod.base.screen_h - M)) * 2 / M;

        if (p->vx > 64) p->vx = 64;
        if (p->vx < -64) p->vx = -64;
        if (p->vy > 64) p->vy = 64;
        if (p->vy < -64) p->vy = -64;

        p->fx += (p->vx * step_fp) / 100;
        p->fy += (p->vy * step_fp) / 100;

        if (p->fx <= 0) {
            p->fx = 0;
            p->vx = saver_int_abs(p->vx);
        } else if (p->fx >= max_fx) {
            p->fx = max_fx;
            p->vx = -saver_int_abs(p->vx);
        }

        if (p->fy <= 0) {
            p->fy = 0;
            p->vy = saver_int_abs(p->vy);
        } else if (p->fy >= max_fy) {
            p->fy = max_fy;
            p->vy = -saver_int_abs(p->vy);
        }
    }
}

void constellation_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const int link_dist_sq = CONSTELLATION_LINK_DIST * CONSTELLATION_LINK_DIST;

    for (int i = 0; i < CONSTELLATION_POINT_COUNT; i++) {
        const cpoint_t *a = &mod.point[i];

        int ax = a->fx >> SAVER_FRAME_SHF;
        int ay = a->fy >> SAVER_FRAME_SHF;

        for (int j = i + 1; j < CONSTELLATION_POINT_COUNT; j++) {
            const cpoint_t *b = &mod.point[j];

            int bx = b->fx >> SAVER_FRAME_SHF;
            int dx = ax - bx;
            if (dx > CONSTELLATION_LINK_DIST) continue;
            if (dx < -CONSTELLATION_LINK_DIST) continue;

            int by = b->fy >> SAVER_FRAME_SHF;
            int dy = ay - by;
            if (dy > CONSTELLATION_LINK_DIST) continue;
            if (dy < -CONSTELLATION_LINK_DIST) continue;

            int dist_sq = dx * dx + dy * dy;
            if (dist_sq > link_dist_sq) continue;

            int alpha = 200 - ((dist_sq * 176) / link_dist_sq);
            if (alpha < 24) alpha = 24;

            SDL_SetRenderDrawColor(renderer, a->r, a->g, a->b, (uint8_t) alpha);
            SDL_RenderDrawLine(renderer, ax, ay, bx, by);
        }
    }

    for (int i = 0; i < CONSTELLATION_POINT_COUNT; i++) {
        const cpoint_t *p = &mod.point[i];

        int x = p->fx >> SAVER_FRAME_SHF;
        int y = p->fy >> SAVER_FRAME_SHF;

        int size = p->size;

        SDL_Rect glow = {x - size, y - size, size * 2 + 1, size * 2 + 1};
        SDL_SetRenderDrawColor(renderer, p->r, p->g, p->b, 36);
        SDL_RenderFillRect(renderer, &glow);

        SDL_Rect core = {x - 1, y - 1, 3, 3};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        SDL_RenderFillRect(renderer, &core);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int constellation_active(void) {
    return saver_active_base(&mod.base);
}

void constellation_stop(void) {
    saver_stop_base(&mod.base);
}

void constellation_shutdown(void) {
    saver_shutdown_base(&mod.base);
}
