#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "../log.h"
#include "../saver.h"
#include "firefly.h"

#define FIREFLY_COUNT       48
#define FIREFLY_SEP_RADIUS  32
#define FIREFLY_CLASS_COUNT 3

static int firefly_edge_margin = 64;

static const int class_radius[FIREFLY_CLASS_COUNT] = {2, 4, 7};
static const int class_max_speed[FIREFLY_CLASS_COUNT] = {130, 95, 65};
static const int class_glow_alpha[FIREFLY_CLASS_COUNT] = {32, 56, 88};
static const int class_agility[FIREFLY_CLASS_COUNT] = {220, 160, 100};

typedef struct {
    int32_t fx, fy;
    int32_t vx, vy;

    int32_t target_vx, target_vy;

    uint32_t heading_due;

    int32_t twinkle_phase;
    int32_t twinkle_step;

    uint8_t size_class;
    uint8_t r, g, b;
} firefly_t;

typedef struct {
    saver_state_t base;

    firefly_t fly[FIREFLY_COUNT];

    int32_t speed_fp_per_ms;
} firefly_module_t;

static firefly_module_t mod = {0};

static int32_t random_velocity_for_class(int cls) {
    int max = class_max_speed[cls];
    int min = max / 4;

    int v = min + (int) saver_rand_range(max - min);

    if (saver_rand_range(2)) v = -v;

    return (int32_t) v;
}

static void apply_fly_colour(firefly_t *f) {
    if (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) {
        saver_pastel_pick((int) saver_rand_range(SAVER_PASTEL_COUNT), &f->r, &f->g, &f->b);
    } else {
        f->r = mod.base.colour_r;
        f->g = mod.base.colour_g;
        f->b = mod.base.colour_b;
    }
}

static void roll_target_heading(firefly_t *f, uint32_t now) {
    int max = class_max_speed[f->size_class];

    int tx = (int) saver_rand_range(max * 2 + 1) - max;
    int ty = (int) saver_rand_range(max * 2 + 1) - max;

    int x = f->fx >> SAVER_FRAME_SHF;
    int y = f->fy >> SAVER_FRAME_SHF;

    if (x < firefly_edge_margin && tx < 0) tx = -tx / 2;
    if (x > mod.base.screen_w - firefly_edge_margin && tx > 0) tx = -tx / 2;

    if (y < firefly_edge_margin && ty < 0) ty = -ty / 2;
    if (y > mod.base.screen_h - firefly_edge_margin && ty > 0) ty = -ty / 2;

    f->target_vx = (int32_t) tx;
    f->target_vy = (int32_t) ty;

    f->heading_due = now + 1500 + (uint32_t) saver_rand_range(3000);
}

static void firefly_reset_one(firefly_t *f, uint32_t now) {
    int max_x = mod.base.screen_w > 1 ? mod.base.screen_w : 1;
    int max_y = mod.base.screen_h > 1 ? mod.base.screen_h : 1;

    /* Spawn somewhere not too close to an edge. */
    int sx = firefly_edge_margin + (int) saver_rand_range(max_x - 2 * firefly_edge_margin > 1 ? max_x - 2 * firefly_edge_margin : 1);
    int sy = firefly_edge_margin + (int) saver_rand_range(max_y - 2 * firefly_edge_margin > 1 ? max_y - 2 * firefly_edge_margin : 1);

    f->fx = ((int32_t) sx) << SAVER_FRAME_SHF;
    f->fy = ((int32_t) sy) << SAVER_FRAME_SHF;

    int roll = (int) saver_rand_range(9);
    if (roll < 3) f->size_class = 0;
    else if (roll < 6) f->size_class = 1;
    else f->size_class = 2;

    f->vx = random_velocity_for_class(f->size_class);
    f->vy = random_velocity_for_class(f->size_class);

    int period_ms = 1500 + (int) saver_rand_range(2000);
    f->twinkle_step = SAVER_FRAME_ONE / period_ms;
    if (f->twinkle_step < 1) f->twinkle_step = 1;
    f->twinkle_phase = saver_rand_range(SAVER_FRAME_ONE);

    roll_target_heading(f, now);
    apply_fly_colour(f);
}

static void firefly_seed_all(void) {
    uint32_t now = SDL_GetTicks();
    for (int i = 0; i < FIREFLY_COUNT; i++) firefly_reset_one(&mod.fly[i], now);
}

static void firefly_on_speed_changed(void *user) {
    (void) user;

    int speed = mod.base.speed;
    if (speed < 1) speed = 1;

    mod.speed_fp_per_ms = (int32_t) ((speed * SAVER_FRAME_ONE) / 1000);
}

static void firefly_on_idle_enter(void *user) {
    (void) user;
    firefly_seed_all();
}

int firefly_init(SDL_Renderer *renderer, int screen_w, int screen_h) {
    (void) renderer;

    int shorter = screen_w < screen_h ? screen_w : screen_h;
    firefly_edge_margin = shorter / 8;

    if (firefly_edge_margin < 24) firefly_edge_margin = 24;
    if (firefly_edge_margin > 96) firefly_edge_margin = 96;

    saver_init_base(&mod.base, screen_w, screen_h, "Firefly", 255, 218, 153, firefly_on_speed_changed, firefly_on_idle_enter, &mod);

    firefly_on_speed_changed(NULL);
    firefly_seed_all();

    LOG_INFO("saver", "Firefly Initialised (%dx%d, count=%d, margin=%d, speed=%d)",
             screen_w, screen_h, FIREFLY_COUNT, firefly_edge_margin, mod.base.speed);

    return 1;
}

static void integrate_fly(firefly_t *f, int32_t step_fp, uint32_t now) {
    int max_speed = class_max_speed[f->size_class];
    int agility = class_agility[f->size_class];

    int32_t turn = ((int32_t) agility * (step_fp >> SAVER_FRAME_SHF)) >> 10;
    if (turn > 256) turn = 256;

    f->vx += ((f->target_vx - f->vx) * turn) >> 8;
    f->vy += ((f->target_vy - f->vy) * turn) >> 8;

    if (now >= f->heading_due) roll_target_heading(f, now);

    int x = f->fx >> SAVER_FRAME_SHF;
    int y = f->fy >> SAVER_FRAME_SHF;

    int M = firefly_edge_margin;

    if (x < M) {
        int s = (M - x);
        f->vx += (s * 4) / M;
    } else if (x > mod.base.screen_w - M) {
        int s = (x - (mod.base.screen_w - M));
        f->vx -= (s * 4) / M;
    }

    if (y < M) {
        int s = (M - y);
        f->vy += (s * 4) / M;
    } else if (y > mod.base.screen_h - M) {
        int s = (y - (mod.base.screen_h - M));
        f->vy -= (s * 4) / M;
    }

    if (f->vx > max_speed) f->vx = max_speed;
    if (f->vx < -max_speed) f->vx = -max_speed;

    if (f->vy > max_speed) f->vy = max_speed;
    if (f->vy < -max_speed) f->vy = -max_speed;

    f->fx += (f->vx * step_fp) / 100;
    f->fy += (f->vy * step_fp) / 100;

    int32_t max_fx = ((int32_t) mod.base.screen_w - 1) << SAVER_FRAME_SHF;
    int32_t max_fy = ((int32_t) mod.base.screen_h - 1) << SAVER_FRAME_SHF;
    if (f->fx < 0) {
        f->fx = 0;
        f->vx = saver_int_abs(f->vx);
    } else if (f->fx > max_fx) {
        f->fx = max_fx;
        f->vx = -saver_int_abs(f->vx);
    }

    if (f->fy < 0) {
        f->fy = 0;
        f->vy = saver_int_abs(f->vy);
    } else if (f->fy > max_fy) {
        f->fy = max_fy;
        f->vy = -saver_int_abs(f->vy);
    }

    f->twinkle_phase += f->twinkle_step * (step_fp >> SAVER_FRAME_SHF);
    while (f->twinkle_phase >= SAVER_FRAME_ONE) f->twinkle_phase -= SAVER_FRAME_ONE;
}

static void apply_pair_separation(int32_t step_fp) {
    const int R = FIREFLY_SEP_RADIUS;
    const int R_sq = R * R;
    int turn_scale = step_fp >> SAVER_FRAME_SHF; /* elapsed ms */

    for (int i = 0; i < FIREFLY_COUNT; i++) {
        firefly_t *a = &mod.fly[i];
        int ax = a->fx >> SAVER_FRAME_SHF;
        int ay = a->fy >> SAVER_FRAME_SHF;

        for (int j = i + 1; j < FIREFLY_COUNT; j++) {
            firefly_t *b = &mod.fly[j];

            int bx = b->fx >> SAVER_FRAME_SHF;
            int dx = ax - bx;

            if (dx > R) continue;
            if (dx < -R) continue;

            int by = b->fy >> SAVER_FRAME_SHF;
            int dy = ay - by;

            if (dy > R) continue;
            if (dy < -R) continue;

            int d_sq = dx * dx + dy * dy;
            if (d_sq >= R_sq || d_sq == 0) continue;

            int mag = ((R_sq - d_sq) * turn_scale) >> 12;
            if (mag < 1) mag = 1;

            a->vx += (dx * mag) >> 8;
            a->vy += (dy * mag) >> 8;
            b->vx -= (dx * mag) >> 8;
            b->vy -= (dy * mag) >> 8;
        }
    }
}

void firefly_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    if (mod.speed_fp_per_ms == 0) return;

    int32_t step_fp = mod.speed_fp_per_ms * (int32_t) elapsed;
    apply_pair_separation(step_fp);

    for (int i = 0; i < FIREFLY_COUNT; i++) {
        integrate_fly(&mod.fly[i], step_fp, now);
    }
}

static inline int twinkle_envelope(int32_t phase) {
    int32_t t = phase >> (SAVER_FRAME_SHF - 10);
    return t < 512 ? (t * 2) : ((1024 - t) * 2);
}

void firefly_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < FIREFLY_COUNT; i++) {
        const firefly_t *f = &mod.fly[i];

        int x = f->fx >> SAVER_FRAME_SHF;
        int y = f->fy >> SAVER_FRAME_SHF;

        int radius = class_radius[f->size_class];
        int env = twinkle_envelope(f->twinkle_phase); /* 0..1024 */

        int glow_alpha = (class_glow_alpha[f->size_class] * (512 + env / 2)) >> 10;
        int halo = radius + (radius >> 1);

        SDL_Rect halo_rect = {x - halo, y - halo, halo * 2, halo * 2};
        SDL_SetRenderDrawColor(renderer, f->r, f->g, f->b, (uint8_t) (glow_alpha / 2));
        SDL_RenderFillRect(renderer, &halo_rect);

        SDL_Rect glow = {x - radius, y - radius, radius * 2, radius * 2};
        SDL_SetRenderDrawColor(renderer, f->r, f->g, f->b, (uint8_t) glow_alpha);
        SDL_RenderFillRect(renderer, &glow);

        int core_alpha = 140 + (env * 115) / 1024;
        if (core_alpha > 255) core_alpha = 255;
        int core_size = 1 + (f->size_class > 0 ? f->size_class : 0);

        SDL_Rect core = {x - core_size / 2 - 1, y - core_size / 2 - 1, core_size + 2, core_size + 2};

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, (uint8_t) core_alpha);
        SDL_RenderFillRect(renderer, &core);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int firefly_active(void) {
    return saver_active_base(&mod.base);
}

void firefly_stop(void) {
    saver_stop_base(&mod.base);
}

void firefly_shutdown(void) {
    saver_shutdown_base(&mod.base);
}
