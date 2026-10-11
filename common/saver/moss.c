#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <common/runtime/log.h>
#include <common/saver/saver.h>
#include <common/saver/moss.h>

#define MOSS_SHADES     4
#define MOSS_TICK_MS    50
#define MOSS_BLOOM_MS   7000
#define MOSS_FLOWERS    64
#define MOSS_MAX_CELLS  20000

enum moss_phase {
    moss_grow = 0,
    moss_bloom,
    moss_wilt,
};

typedef struct {
    saver_state_t base;
    SDL_Renderer *renderer;
    saver_snapshot_t snapshot;
    SDL_Texture *texture;
    int cell;
    int cols;
    int rows;
    uint8_t *food;
    uint8_t *level;
    SDL_Rect *rects[2][MOSS_SHADES];
    int rect_count[2][MOSS_SHADES];
    int flower[MOSS_FLOWERS];
    uint8_t flower_colour[MOSS_FLOWERS];
    enum moss_phase phase;
    uint32_t phase_start;
    uint32_t tick_carry;
    uint8_t live[MOSS_SHADES][3];
    uint8_t dead[MOSS_SHADES][3];
    int covered;
} moss_module_t;

static moss_module_t mod = {0};

static void free_buffers(void) {
    free(mod.food);
    free(mod.level);
    mod.food = NULL;
    mod.level = NULL;
    for (int k = 0; k < 2; k++) {
        for (int s = 0; s < MOSS_SHADES; s++) {
            free(mod.rects[k][s]);
            mod.rects[k][s] = NULL;
        }
    }
}

static void measure_food(void) {
    const int count = mod.cols * mod.rows;
    if (!mod.snapshot.rgb) {
        for (int i = 0; i < count; i++)
            mod.food[i] = (uint8_t) (80 + saver_rand_range(80));
        return;
    }

    for (int row = 0; row < mod.rows; row++) {
        for (int col = 0; col < mod.cols; col++) {
            const int x0 = mod.snapshot.w * col / mod.cols;
            const int y0 = mod.snapshot.h * row / mod.rows;
            const int x1 = mod.snapshot.w * (col + 1) / mod.cols;
            const int y1 = mod.snapshot.h * (row + 1) / mod.rows;
            mod.food[row * mod.cols + col] = (uint8_t) saver_snapshot_luma(&mod.snapshot, x0, y0, x1, y1);
        }
    }
}

static void pick_colours(void) {
    const float hue = (float) saver_rand_range(1000) / 1000.0f;
    for (int s = 0; s < MOSS_SHADES; s++) {
        const float step = (float) s / (float) (MOSS_SHADES - 1);
        saver_hsv(hue, 0.75f - step * 0.15f, 0.32f + step * 0.38f, mod.live[s]);
        saver_hsv(hue, 0.25f, 0.24f + step * 0.22f, mod.dead[s]);
    }
}

static void seed_moss(void) {
    pick_colours();
    memset(mod.level, 0, (size_t) mod.cols * mod.rows);
    const int base_col = (int) saver_rand_range(mod.cols);
    const int base_row = (int) saver_rand_range(mod.rows);
    for (int s = 0; s < 5; s++) {
        int col = base_col + (int) saver_rand_range(7) - 3;
        int row = base_row + (int) saver_rand_range(7) - 3;
        if (col < 0) col = 0;
        if (row < 0) row = 0;
        if (col >= mod.cols) col = mod.cols - 1;
        if (row >= mod.rows) row = mod.rows - 1;
        mod.level[row * mod.cols + col] = 120;
    }
    mod.covered = 0;
    mod.phase = moss_grow;
    mod.phase_start = SDL_GetTicks();
}

static void on_idle_enter(void *user) {
    (void) user;
    if (mod.texture) {
        SDL_DestroyTexture(mod.texture);
        mod.texture = NULL;
    }
    if (!saver_snapshot_take(&mod.snapshot)) LOG_INFO("saver", "Pixel Moss: no screen capture, growing on bare ground");
    measure_food();
    seed_moss();
}

int moss_init(SDL_Renderer *renderer, const int screen_w, const int screen_h) {
    mod.renderer = renderer;
    saver_init_base(&mod.base, screen_w, screen_h, "Pixel Moss", 80, 150, 60, NULL, on_idle_enter, &mod);

    mod.cell = screen_w / 64;
    if (mod.cell < 4) mod.cell = 4;
    mod.cols = (screen_w + mod.cell - 1) / mod.cell;
    mod.rows = (screen_h + mod.cell - 1) / mod.cell;
    while (mod.cols * mod.rows > MOSS_MAX_CELLS) {
        mod.cell++;
        mod.cols = (screen_w + mod.cell - 1) / mod.cell;
        mod.rows = (screen_h + mod.cell - 1) / mod.cell;
    }

    free_buffers();
    const size_t count = (size_t) mod.cols * mod.rows;
    mod.food = malloc(count);
    mod.level = calloc(count, 1);
    for (int k = 0; k < 2; k++)
        for (int s = 0; s < MOSS_SHADES; s++)
            mod.rects[k][s] = malloc(sizeof(SDL_Rect) * count);
    for (int k = 0; k < 2; k++) {
        for (int s = 0; s < MOSS_SHADES; s++) {
            if (!mod.rects[k][s]) {
                free_buffers();
                return 0;
            }
        }
    }
    if (!mod.food || !mod.level) {
        free_buffers();
        return 0;
    }

    measure_food();
    seed_moss();
    LOG_INFO("saver", "Pixel Moss Initialised (%dx%d, %dx%d cells)", screen_w, screen_h, mod.cols, mod.rows);
    return 1;
}

static int neighbours(const int col, const int row) {
    int n = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            const int c = col + dx;
            const int r = row + dy;
            if (c < 0 || r < 0 || c >= mod.cols || r >= mod.rows) continue;
            if (mod.level[r * mod.cols + c] > 40) n++;
        }
    }
    return n;
}

static void grow_tick(void) {
    const int count = mod.cols * mod.rows;
    const int samples = count / 6 + 1;
    for (int s = 0; s < samples; s++) {
        const int i = (int) saver_rand_range(count);
        const int col = i % mod.cols;
        const int row = i / mod.cols;
        uint8_t *cell = &mod.level[i];

        if (*cell) {
            const int gain = 4 + mod.food[i] / 24;
            *cell = saver_clamp_u8(*cell + gain);
            continue;
        }

        const int n = neighbours(col, row);
        if (!n) continue;
        const int chance = n * (20 + mod.food[i] / 3);
        if ((int) saver_rand_range(1000) < chance) *cell = 30;
    }

    int covered = 0;
    for (int i = 0; i < count; i++)
        if (mod.level[i]) covered++;
    mod.covered = covered;
}

static void wilt_tick(void) {
    const int count = mod.cols * mod.rows;
    const int samples = count / 5 + 1;
    for (int s = 0; s < samples; s++) {
        const int i = (int) saver_rand_range(count);
        uint8_t *cell = &mod.level[i];
        if (!*cell) continue;
        const int col = i % mod.cols;
        const int row = i / mod.cols;
        const int loss = 8 + (8 - neighbours(col, row)) * 4;
        *cell = *cell > loss ? (uint8_t) (*cell - loss) : 0;
    }

    int covered = 0;
    for (int i = 0; i < count; i++)
        if (mod.level[i]) covered++;
    mod.covered = covered;
}

void moss_update(void) {
    const uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    const uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    if (!mod.level) return;

    mod.tick_carry += (uint32_t) ((elapsed > 200 ? 200 : elapsed) * saver_speed_scale_q8(&mod.base) / 256);
    const int count = mod.cols * mod.rows;

    while (mod.tick_carry >= MOSS_TICK_MS) {
        mod.tick_carry -= MOSS_TICK_MS;

        if (mod.phase == moss_grow) {
            grow_tick();
            if (mod.covered * 100 >= count * 97) {
                mod.phase = moss_bloom;
                mod.phase_start = now;
                for (int f = 0; f < MOSS_FLOWERS; f++) {
                    mod.flower[f] = (int) saver_rand_range(count);
                    mod.flower_colour[f] = (uint8_t) saver_rand_range(SAVER_PASTEL_COUNT);
                }
            }
        } else if (mod.phase == moss_bloom) {
            if (now - mod.phase_start >= MOSS_BLOOM_MS) {
                mod.phase = moss_wilt;
                mod.phase_start = now;
            }
        } else {
            wilt_tick();
            if (!mod.covered) {
                measure_food();
                seed_moss();
            }
        }
    }
}

void moss_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.level) return;

    if (!mod.texture && mod.snapshot.rgb) mod.texture = saver_snapshot_texture(renderer, &mod.snapshot);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    unsigned draw_calls = 1;

    if (mod.texture) {
        SDL_Rect dst = {0, 0, mod.base.screen_w, mod.base.screen_h};
        SDL_SetTextureColorMod(mod.texture, 90, 90, 90);
        SDL_RenderCopy(renderer, mod.texture, NULL, &dst);
        draw_calls++;
    }

    for (int k = 0; k < 2; k++)
        for (int s = 0; s < MOSS_SHADES; s++)
            mod.rect_count[k][s] = 0;

    const int wilting = mod.phase == moss_wilt;
    for (int row = 0; row < mod.rows; row++) {
        for (int col = 0; col < mod.cols; col++) {
            const uint8_t level = mod.level[row * mod.cols + col];
            if (!level) continue;
            const int shade = level * MOSS_SHADES / 256;
            const int kind = wilting ? 1 : 0;
            SDL_Rect *rect = &mod.rects[kind][shade][mod.rect_count[kind][shade]++];
            rect->x = col * mod.cell;
            rect->y = row * mod.cell;
            rect->w = mod.cell;
            rect->h = mod.cell;
        }
    }

    for (int k = 0; k < 2; k++) {
        for (int s = 0; s < MOSS_SHADES; s++) {
            if (!mod.rect_count[k][s]) continue;
            const uint8_t *c = k ? mod.dead[s] : mod.live[s];
            SDL_SetRenderDrawColor(renderer, c[0], c[1], c[2], 255);
            SDL_RenderFillRects(renderer, mod.rects[k][s], mod.rect_count[k][s]);
            draw_calls++;
        }
    }

    if (mod.phase == moss_bloom) {
        const int count = mod.cols * mod.rows;
        const int dot = mod.cell / 2 > 2 ? mod.cell / 2 : 2;
        for (int f = 0; f < MOSS_FLOWERS; f++) {
            const int i = mod.flower[f] % count;
            uint8_t r, g, b;
            saver_pastel_pick(mod.flower_colour[f], &r, &g, &b);
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            SDL_Rect petal = {(i % mod.cols) * mod.cell + mod.cell / 4, (i / mod.cols) * mod.cell + mod.cell / 4, dot,
                              dot};
            SDL_RenderFillRect(renderer, &petal);
        }
        draw_calls += MOSS_FLOWERS;
    }

    saver_perf_note_draw_calls(draw_calls);
}

int moss_active(void) {
    return saver_active_base(&mod.base);
}

void moss_stop(void) {
    saver_stop_base(&mod.base);
}

void moss_shutdown(void) {
    if (mod.texture) {
        SDL_DestroyTexture(mod.texture);
        mod.texture = NULL;
    }
    saver_snapshot_free(&mod.snapshot);
    free_buffers();
    saver_shutdown_base(&mod.base);
}
