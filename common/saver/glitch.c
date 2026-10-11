#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <common/runtime/log.h>
#include <common/saver/saver.h>
#include <common/saver/glitch.h>

#define GLITCH_COLS       16
#define GLITCH_MAX_ROWS   16
#define GLITCH_TILES      (GLITCH_COLS * GLITCH_MAX_ROWS)
#define GLITCH_HOLD_MS    6000
#define GLITCH_TREMBLE_MS 2200
#define GLITCH_FALL_MS    5500
#define GLITCH_EMPTY_MS   1500
#define GLITCH_RISE_MS    4500

enum glitch_phase {
    glitch_hold = 0,
    glitch_tremble,
    glitch_fall,
    glitch_empty,
    glitch_rise,
};

typedef struct {
    int source;
    int flip;
    float y;
    float vy;
    float angle;
    float spin;
    uint32_t delay;
} glitch_tile_t;

typedef struct {
    saver_state_t base;
    SDL_Renderer *renderer;
    saver_snapshot_t snapshot;
    SDL_Texture *texture;
    int texture_w, texture_h;
    int rows;
    glitch_tile_t tile[GLITCH_TILES];
    enum glitch_phase phase;
    uint32_t phase_start;
    int wrongness;
} glitch_module_t;

static glitch_module_t mod = {0};

static void make_fallback(void) {
    const int w = 320;
    const int h = 240;
    uint8_t *rgb = malloc((size_t) w * h * 3);
    if (!rgb) return;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t *px = rgb + ((size_t) y * w + x) * 3;
            const int band = ((x + y) / 20) % 4;
            px[0] = (uint8_t) (band == 0 ? 217 : band == 1 ? 60 : band == 2 ? 200 : 30);
            px[1] = (uint8_t) (band == 0 ? 164 : band == 1 ? 60 : band == 2 ? 40 : 30);
            px[2] = (uint8_t) (band == 0 ? 14 : band == 1 ? 70 : band == 2 ? 30 : 36);
        }
    }

    mod.snapshot.rgb = rgb;
    mod.snapshot.w = w;
    mod.snapshot.h = h;
}

static void reset_tiles(void) {
    for (int i = 0; i < GLITCH_COLS * mod.rows; i++) {
        glitch_tile_t *t = &mod.tile[i];
        t->source = i;
        t->flip = SDL_FLIP_NONE;
        t->y = 0.0f;
        t->vy = 0.0f;
        t->angle = 0.0f;
        t->spin = 0.0f;
        t->delay = 0;
    }
}

static void start_phase(const enum glitch_phase phase, const uint32_t now) {
    mod.phase = phase;
    mod.phase_start = now;

    const int count = GLITCH_COLS * mod.rows;
    if (phase == glitch_fall) {
        for (int i = 0; i < count; i++) {
            glitch_tile_t *t = &mod.tile[i];
            t->delay = (uint32_t) saver_rand_range(GLITCH_FALL_MS - 1800);
            t->vy = 0.0f;
            t->spin = (float) (saver_rand_range(240) - 120);
        }
    } else if (phase == glitch_rise) {
        mod.wrongness++;
        const int swaps = 1 + mod.wrongness;
        for (int s = 0; s < swaps; s++) {
            const int a = (int) saver_rand_range(count);
            const int b = (int) saver_rand_range(count);
            const int keep = mod.tile[a].source;
            mod.tile[a].source = mod.tile[b].source;
            mod.tile[b].source = keep;
        }
        const int flips = 1 + (int) saver_rand_range(1 + mod.wrongness / 2);
        for (int f = 0; f < flips; f++) {
            glitch_tile_t *t = &mod.tile[saver_rand_range(count)];
            t->flip ^= saver_rand_range(2) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_VERTICAL;
        }
        for (int i = 0; i < count; i++) {
            glitch_tile_t *t = &mod.tile[i];
            t->delay = (uint32_t) saver_rand_range(GLITCH_RISE_MS - 1500);
            t->y = (float) mod.base.screen_h * 1.2f;
            t->angle = (float) (saver_rand_range(360) - 180);
        }
    } else if (phase == glitch_hold) {
        for (int i = 0; i < count; i++) {
            mod.tile[i].y = 0.0f;
            mod.tile[i].angle = 0.0f;
        }
    }
}

static void on_idle_enter(void *user) {
    (void) user;

    if (mod.texture) {
        SDL_DestroyTexture(mod.texture);
        mod.texture = NULL;
    }
    if (!saver_snapshot_take(&mod.snapshot)) {
        LOG_INFO("saver", "Gravity Glitch: no screen capture, using a pattern instead");
        make_fallback();
    }

    mod.wrongness = 0;
    reset_tiles();
    start_phase(glitch_hold, SDL_GetTicks());
}

int glitch_init(SDL_Renderer *renderer, const int screen_w, const int screen_h) {
    mod.renderer = renderer;
    saver_init_base(&mod.base, screen_w, screen_h, "Gravity Glitch", 255, 255, 255, NULL, on_idle_enter, &mod);

    mod.rows = (GLITCH_COLS * screen_h + screen_w / 2) / (screen_w > 0 ? screen_w : 1);
    if (mod.rows < 4) mod.rows = 4;
    if (mod.rows > GLITCH_MAX_ROWS) mod.rows = GLITCH_MAX_ROWS;

    reset_tiles();
    mod.phase = glitch_hold;
    mod.phase_start = SDL_GetTicks();

    LOG_INFO("saver", "Gravity Glitch Initialised (%dx%d, %dx%d tiles)", screen_w, screen_h, GLITCH_COLS, mod.rows);
    return 1;
}

void glitch_update(void) {
    const uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    const uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    const float scale = (float) saver_speed_scale_q8(&mod.base) / 256.0f;
    const float dt = (float) (elapsed > 100 ? 100 : elapsed) / 1000.0f * scale;
    const uint32_t in_phase = (uint32_t) ((float) (now - mod.phase_start) * scale);
    const float h = (float) mod.base.screen_h;
    const int count = GLITCH_COLS * mod.rows;

    switch (mod.phase) {
        case glitch_hold:
            if (in_phase >= GLITCH_HOLD_MS) start_phase(glitch_tremble, now);
            break;
        case glitch_tremble:
            if (in_phase >= GLITCH_TREMBLE_MS) start_phase(glitch_fall, now);
            break;
        case glitch_fall: {
            int remaining = 0;
            for (int i = 0; i < count; i++) {
                glitch_tile_t *t = &mod.tile[i];
                if (in_phase < t->delay) {
                    remaining++;
                    continue;
                }
                t->vy += h * 2.2f * dt;
                t->y += t->vy * dt;
                t->angle += t->spin * dt;
                if (t->y < h * 1.3f) remaining++;
            }
            if (!remaining || in_phase >= GLITCH_FALL_MS + 3000) start_phase(glitch_empty, now);
            break;
        }
        case glitch_empty:
            if (in_phase >= GLITCH_EMPTY_MS) start_phase(glitch_rise, now);
            break;
        case glitch_rise: {
            int moving = 0;
            for (int i = 0; i < count; i++) {
                glitch_tile_t *t = &mod.tile[i];
                if (in_phase < t->delay) {
                    moving++;
                    continue;
                }
                const float pull = dt * 5.0f > 1.0f ? 1.0f : dt * 5.0f;
                t->y += (0.0f - t->y) * pull;
                t->angle += (0.0f - t->angle) * pull;
                if (t->y > 0.5f || t->angle > 0.5f || t->angle < -0.5f)
                    moving++;
                else {
                    t->y = 0.0f;
                    t->angle = 0.0f;
                }
            }
            if (!moving) start_phase(glitch_hold, now);
            break;
        }
    }
}

void glitch_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.snapshot.rgb) return;

    if (!mod.texture) mod.texture = saver_snapshot_texture(renderer, &mod.snapshot);
    if (!mod.texture) return;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const uint32_t now = SDL_GetTicks();
    const int w = mod.base.screen_w;
    const int h = mod.base.screen_h;
    const int count = GLITCH_COLS * mod.rows;
    const int tremble = mod.phase == glitch_tremble;
    const int empty = mod.phase == glitch_empty;
    unsigned draw_calls = 0;

    if (empty) {
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        for (int c = 1; c < GLITCH_COLS; c++)
            SDL_RenderDrawLine(renderer, w * c / GLITCH_COLS, 0, w * c / GLITCH_COLS, h);
        for (int r = 1; r < mod.rows; r++)
            SDL_RenderDrawLine(renderer, 0, h * r / mod.rows, w, h * r / mod.rows);
        saver_perf_note_draw_calls((unsigned) (GLITCH_COLS + mod.rows));
        return;
    }

    for (int i = 0; i < count; i++) {
        const glitch_tile_t *t = &mod.tile[i];
        const int col = i % GLITCH_COLS;
        const int row = i / GLITCH_COLS;
        const int src_col = t->source % GLITCH_COLS;
        const int src_row = t->source / GLITCH_COLS;

        SDL_Rect src = {
            mod.snapshot.w * src_col / GLITCH_COLS, mod.snapshot.h * src_row / mod.rows,
            mod.snapshot.w * (src_col + 1) / GLITCH_COLS - mod.snapshot.w * src_col / GLITCH_COLS,
            mod.snapshot.h * (src_row + 1) / mod.rows - mod.snapshot.h * src_row / mod.rows
        };
        SDL_Rect dst = {
            w * col / GLITCH_COLS, h * row / mod.rows, w * (col + 1) / GLITCH_COLS - w * col / GLITCH_COLS,
            h * (row + 1) / mod.rows - h * row / mod.rows
        };
        dst.y += (int) t->y;
        if (tremble) {
            const int shake = 1 + (int) ((now - mod.phase_start) * 3 / GLITCH_TREMBLE_MS);
            dst.x += (int) saver_rand_range(shake * 2 + 1) - shake;
            dst.y += (int) saver_rand_range(shake * 2 + 1) - shake;
        }
        if (dst.y > h) continue;

        SDL_RenderCopyEx(renderer, mod.texture, &src, &dst, t->angle, NULL, (SDL_RendererFlip) t->flip);
        draw_calls++;
    }

    saver_perf_note_draw_calls(draw_calls + 1);
}

int glitch_active(void) {
    return saver_active_base(&mod.base);
}

void glitch_stop(void) {
    saver_stop_base(&mod.base);
}

void glitch_shutdown(void) {
    if (mod.texture) {
        SDL_DestroyTexture(mod.texture);
        mod.texture = NULL;
    }
    saver_snapshot_free(&mod.snapshot);
    saver_shutdown_base(&mod.base);
}
