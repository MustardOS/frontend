#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "../log.h"
#include "../saver.h"
#include "trace.h"

#define TRACE_HEAD_COUNT 12
#define TRACE_SEG_COUNT  192

#define TRACE_MIN_GRID 12
#define TRACE_MAX_GRID 28

#define TRACE_DIR_RIGHT 0
#define TRACE_DIR_DOWN  1
#define TRACE_DIR_LEFT  2

typedef struct {
    int active;
    int x1, y1, x2, y2;
    uint16_t life, max_life;
    uint8_t r, g, b;
} seg_t;

typedef struct {
    int active;
    int x, y, dir;
    int target_x, target_y;
    int32_t fx, fy;
    int32_t speed;
    uint8_t r, g, b;
} head_t;

typedef struct {
    saver_state_t base;
    head_t head[TRACE_HEAD_COUNT];
    seg_t seg[TRACE_SEG_COUNT];
    int grid;
    int cols, rows;
    uint32_t last_spawn;
} trace_module_t;

static trace_module_t mod = {0};

static void apply_trace_colour(uint8_t *r, uint8_t *g, uint8_t *b) {
    if (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) {
        saver_pastel_pick(saver_rand_range(SAVER_PASTEL_COUNT), r, g, b);
    } else {
        *r = mod.base.colour_r;
        *g = mod.base.colour_g;
        *b = mod.base.colour_b;
    }
}

static int opposite_dir(const int dir) {
    return (dir + 2) & 3;
}

static void dir_step(const int dir, int *dx, int *dy) {
    *dx = 0;
    *dy = 0;

    if (dir == TRACE_DIR_RIGHT)
        *dx = 1;
    else if (dir == TRACE_DIR_DOWN)
        *dy = 1;
    else if (dir == TRACE_DIR_LEFT)
        *dx = -1;
    else
        *dy = -1;
}

static int choose_dir(const int current) {
    if (saver_rand_range(100) < 80) return current;

    for (int i = 0; i < 8; i++) {
        const int dir = saver_rand_range(4);
        if (dir != opposite_dir(current)) return dir;
    }

    return current;
}

static void make_target(head_t *h) {
    int dx, dy;
    for (int tries = 0; tries < 8; tries++) {
        h->dir = choose_dir(h->dir);
        dir_step(h->dir, &dx, &dy);

        const int length = 1 + saver_rand_range(2);

        const int nx = h->x + dx * length;
        const int ny = h->y + dy * length;

        if (nx < 0 || nx >= mod.cols || ny < 0 || ny >= mod.rows) continue;
        h->target_x = nx;
        h->target_y = ny;

        return;
    }

    h->dir = saver_rand_range(4);
    dir_step(h->dir, &dx, &dy);

    h->target_x = h->x + dx;
    h->target_y = h->y + dy;

    if (h->target_x < 0) h->target_x = 0;
    if (h->target_x >= mod.cols) h->target_x = mod.cols - 1;

    if (h->target_y < 0) h->target_y = 0;
    if (h->target_y >= mod.rows) h->target_y = mod.rows - 1;
}

static seg_t *get_seg_slot(void) {
    seg_t *oldest = &mod.seg[0];

    for (int i = 0; i < TRACE_SEG_COUNT; i++) {
        if (!mod.seg[i].active) return &mod.seg[i];
        if (mod.seg[i].life < oldest->life) oldest = &mod.seg[i];
    }

    return oldest;
}

static void
add_segment(const int x1, const int y1, const int x2, const int y2, const uint8_t r, const uint8_t g, const uint8_t b) {
    seg_t *s = get_seg_slot();
    s->active = 1;

    s->x1 = x1;
    s->y1 = y1;
    s->x2 = x2;
    s->y2 = y2;

    s->max_life = 5500 + (uint16_t) saver_rand_range(4000);
    s->life = s->max_life;

    s->r = r;
    s->g = g;
    s->b = b;
}

static void start_head(head_t *h) {
    int speed = mod.base.speed;
    if (speed < 1) speed = 1;

    h->active = 1;

    h->x = saver_rand_range(mod.cols);
    h->y = saver_rand_range(mod.rows);

    h->dir = saver_rand_range(4);

    h->fx = (h->x * mod.grid) << SAVER_FRAME_SHF;
    h->fy = (h->y * mod.grid) << SAVER_FRAME_SHF;

    const int varied = speed + saver_rand_range(speed + 1);
    h->speed = varied * SAVER_FRAME_ONE / 1000;
    if (h->speed < SAVER_FRAME_ONE / 32) h->speed = SAVER_FRAME_ONE / 32;

    apply_trace_colour(&h->r, &h->g, &h->b);
    make_target(h);
}

static void spawn_head(void) {
    head_t *slot = NULL;

    for (int i = 0; i < TRACE_HEAD_COUNT; i++) {
        if (!mod.head[i].active) {
            slot = &mod.head[i];
            break;
        }
    }

    if (!slot) slot = &mod.head[saver_rand_range(TRACE_HEAD_COUNT)];
    start_head(slot);
}

static void seed_all(void) {
    for (int i = 0; i < TRACE_HEAD_COUNT; i++)
        mod.head[i].active = 0;
    for (int i = 0; i < TRACE_SEG_COUNT; i++)
        mod.seg[i].active = 0;

    const int start_count = TRACE_HEAD_COUNT / 2;
    for (int i = 0; i < start_count; i++)
        spawn_head();
}

static void rebuild_grid(void) {
    const int shortest = mod.base.screen_w < mod.base.screen_h ? mod.base.screen_w : mod.base.screen_h;
    mod.grid = shortest / 42;

    if (mod.grid < TRACE_MIN_GRID) mod.grid = TRACE_MIN_GRID;
    if (mod.grid > TRACE_MAX_GRID) mod.grid = TRACE_MAX_GRID;

    mod.cols = mod.base.screen_w / mod.grid;
    mod.rows = mod.base.screen_h / mod.grid;

    if (mod.cols < 2) mod.cols = 2;
    if (mod.rows < 2) mod.rows = 2;
}

static void on_speed_changed(void *user) {
    (void) user;
}

static void on_idle_enter(void *user) {
    (void) user;
    seed_all();
}

int trace_init(const SDL_Renderer *renderer, const int screen_w, const int screen_h) {
    (void) renderer;

    saver_init_base(
        &mod.base, screen_w, screen_h, "Circuit Trace", 153, 255, 220, on_speed_changed, on_idle_enter, &mod
    );

    rebuild_grid();
    seed_all();
    mod.last_spawn = SDL_GetTicks();

    LOG_INFO(
        "saver", "Circuit Trace Initialised (%dx%d, grid=%d, nodes=%dx%d, speed=%d)", screen_w, screen_h, mod.grid,
        mod.cols, mod.rows, mod.base.speed
    );

    return 1;
}

void trace_update(void) {
    const uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) {
        mod.last_spawn = now;
        return;
    }

    const uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    for (int i = 0; i < TRACE_SEG_COUNT; i++) {
        seg_t *s = &mod.seg[i];
        if (!s->active) continue;
        if (elapsed >= s->life)
            s->active = 0;
        else
            s->life = (uint16_t) (s->life - elapsed);
    }

    for (int i = 0; i < TRACE_HEAD_COUNT; i++) {
        head_t *h = &mod.head[i];
        if (!h->active) continue;

        const int tx = h->target_x * mod.grid;
        const int ty = h->target_y * mod.grid;

        const int hx = h->fx >> SAVER_FRAME_SHF;
        const int hy = h->fy >> SAVER_FRAME_SHF;

        const int dx = tx - hx;
        const int dy = ty - hy;

        const int dist = saver_int_abs(dx) + saver_int_abs(dy);
        int step = (h->speed * (int32_t) elapsed) >> SAVER_FRAME_SHF;
        if (step < 1) step = 1;

        if (dist <= step) {
            const int old_x = h->x * mod.grid;
            const int old_y = h->y * mod.grid;

            h->x = h->target_x;
            h->y = h->target_y;

            h->fx = (int32_t) tx << SAVER_FRAME_SHF;
            h->fy = (int32_t) ty << SAVER_FRAME_SHF;

            add_segment(old_x, old_y, tx, ty, h->r, h->g, h->b);

            if (saver_rand_range(100) < 8) spawn_head();
            if (saver_rand_range(100) < 3)
                h->active = 0;
            else
                make_target(h);
        } else {
            if (dx > 0)
                h->fx += step << SAVER_FRAME_SHF;
            else if (dx < 0)
                h->fx -= step << SAVER_FRAME_SHF;
            else if (dy > 0)
                h->fy += step << SAVER_FRAME_SHF;
            else if (dy < 0)
                h->fy -= step << SAVER_FRAME_SHF;
        }
    }

    uint32_t spawn_delay = 1100;
    if (mod.base.speed > 0) {
        int scaled = 90000 / mod.base.speed;
        if (scaled < 240) scaled = 240;
        if (scaled > 1600) scaled = 1600;
        spawn_delay = (uint32_t) scaled;
    }

    if (now - mod.last_spawn >= spawn_delay) {
        spawn_head();
        mod.last_spawn = now;
    }
}

static void draw_pad(
    SDL_Renderer *renderer, const int x, const int y, const uint8_t r, const uint8_t g, const uint8_t b,
    const uint8_t alpha
) {
    int size = mod.grid / 3;
    if (size < 3) size = 3;
    const SDL_Rect pad = {x - size / 2, y - size / 2, size, size};
    SDL_SetRenderDrawColor(renderer, r, g, b, alpha);
    SDL_RenderFillRect(renderer, &pad);
}

void trace_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < TRACE_SEG_COUNT; i++) {
        const seg_t *s = &mod.seg[i];
        if (!s->active || s->max_life == 0) continue;

        int alpha = (int) ((uint32_t) s->life * 200u / s->max_life);
        if (alpha < 8) alpha = 8;

        SDL_SetRenderDrawColor(renderer, s->r, s->g, s->b, (uint8_t) alpha);
        SDL_RenderDrawLine(renderer, s->x1, s->y1, s->x2, s->y2);

        const int halo = alpha / 3;
        SDL_SetRenderDrawColor(renderer, s->r, s->g, s->b, (uint8_t) halo);
        SDL_RenderDrawLine(renderer, s->x1 + 1, s->y1, s->x2 + 1, s->y2);
        SDL_RenderDrawLine(renderer, s->x1 - 1, s->y1, s->x2 - 1, s->y2);
        SDL_RenderDrawLine(renderer, s->x1, s->y1 + 1, s->x2, s->y2 + 1);
        SDL_RenderDrawLine(renderer, s->x1, s->y1 - 1, s->x2, s->y2 - 1);

        if (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) {
            draw_pad(renderer, s->x2, s->y2, s->r, s->g, s->b, (uint8_t) alpha);
        }
    }

    for (int i = 0; i < TRACE_HEAD_COUNT; i++) {
        const head_t *h = &mod.head[i];
        if (!h->active) continue;

        const int x = h->fx >> SAVER_FRAME_SHF;
        const int y = h->fy >> SAVER_FRAME_SHF;

        draw_pad(renderer, x, y, 255, 255, 255, 220);

        SDL_SetRenderDrawColor(renderer, h->r, h->g, h->b, 96);

        int glow = mod.grid / 2;
        if (glow < 4) glow = 4;

        SDL_Rect rect = {x - glow / 2, y - glow / 2, glow, glow};
        SDL_RenderDrawRect(renderer, &rect);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int trace_active(void) {
    return saver_active_base(&mod.base);
}

void trace_stop(void) {
    saver_stop_base(&mod.base);
}

void trace_shutdown(void) {
    saver_shutdown_base(&mod.base);
}
