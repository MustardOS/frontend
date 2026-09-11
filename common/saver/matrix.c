#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <common/runtime/log.h>
#include <common/saver/saver.h>
#include <common/saver/matrix.h>

#define MATRIX_MIN_CELL 8
#define MATRIX_MAX_CELL 22

typedef struct {
    int32_t y;
    int32_t speed;
    int x;
    int len;
    uint8_t r, g, b;
} drop_t;

typedef struct {
    saver_state_t base;
    drop_t *drop;
    int cell;
    int columns;
    SDL_Rect *rect_batch;
    size_t batch_capacity;
} matrix_module_t;

static matrix_module_t mod = {0};

static void apply_drop_colour(drop_t *d) {
    if (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) {
        saver_pastel_pick((int) saver_rand_range(SAVER_PASTEL_COUNT), &d->r, &d->g, &d->b);
    } else {
        d->r = mod.base.colour_r;
        d->g = mod.base.colour_g;
        d->b = mod.base.colour_b;
    }
}

static void reset_drop(drop_t *d, int column, int randomise_y) {
    d->x = column * mod.cell;

    d->len = 6 + (int) saver_rand_range(18);
    if (mod.base.screen_h > 0) {
        int max_len = mod.base.screen_h / mod.cell;
        if (max_len > 4 && d->len > max_len) d->len = max_len;
    }

    int speed = mod.base.speed;
    if (speed < 1) speed = 1;

    int varied = speed + (int) saver_rand_range(speed + 1);

    d->speed = (int32_t) ((varied * SAVER_FRAME_ONE) / 1000);
    if (d->speed < (SAVER_FRAME_ONE / 32)) d->speed = SAVER_FRAME_ONE / 32;

    if (randomise_y) {
        int start = mod.base.screen_h + (d->len * mod.cell);
        d->y = ((int32_t) saver_rand_range(start > 1 ? start : 1) - (d->len * mod.cell)) << SAVER_FRAME_SHF;
    } else {
        d->y = -((int32_t) (d->len * mod.cell) << SAVER_FRAME_SHF);
    }

    apply_drop_colour(d);
}

static void seed_all(void) {
    if (!mod.drop) return;
    for (int i = 0; i < mod.columns; i++)
        reset_drop(&mod.drop[i], i, 1);
}

static void rebuild_columns(void) {
    free(mod.drop);
    mod.drop = NULL;

    int shortest = mod.base.screen_w < mod.base.screen_h ? mod.base.screen_w : mod.base.screen_h;
    mod.cell = shortest / 48;

    if (mod.cell < MATRIX_MIN_CELL) mod.cell = MATRIX_MIN_CELL;
    if (mod.cell > MATRIX_MAX_CELL) mod.cell = MATRIX_MAX_CELL;

    mod.columns = (mod.base.screen_w + mod.cell - 1) / mod.cell;
    if (mod.columns < 1) mod.columns = 1;

    mod.drop = calloc((size_t) mod.columns, sizeof(drop_t));
    if (!mod.drop) {
        mod.columns = 0;
        mod.base.enabled = 0;

        LOG_ERROR("saver", "Matrix allocation failed");

        return;
    }

    free(mod.rect_batch);
    mod.batch_capacity = (size_t) mod.columns * 32u;
    mod.rect_batch = malloc(mod.batch_capacity * 12u * sizeof(*mod.rect_batch));

    seed_all();
}

static void on_speed_changed(void *user) {
    (void) user;
}

static void on_idle_enter(void *user) {
    (void) user;
    seed_all();
}

int matrix_init(SDL_Renderer *renderer, int screen_w, int screen_h) {
    (void) renderer;

    saver_init_base(&mod.base, screen_w, screen_h, "Matrix", 153, 255, 220, on_speed_changed, on_idle_enter, &mod);

    rebuild_columns();

    LOG_INFO(
        "saver", "Matrix Initialised (%dx%d, columns=%d, cell=%d, speed=%d)", screen_w, screen_h, mod.columns, mod.cell,
        mod.base.speed
    );

    return mod.base.enabled;
}

void matrix_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    for (int i = 0; i < mod.columns; i++) {
        drop_t *d = &mod.drop[i];
        d->y += d->speed * (int32_t) elapsed;

        int y = d->y >> SAVER_FRAME_SHF;
        if (y - (d->len * mod.cell) > mod.base.screen_h) reset_drop(d, i, 0);
    }
}

void matrix_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.drop) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (mod.base.speed < SAVER_SPEED_COLOUR_THRESHOLD && mod.rect_batch) {
        int counts[12] = {0};
        const uint8_t mid_r = (uint8_t) ((255 + mod.base.colour_r) / 2);
        const uint8_t mid_g = (uint8_t) ((255 + mod.base.colour_g) / 2);
        const uint8_t mid_b = (uint8_t) ((255 + mod.base.colour_b) / 2);

        for (int i = 0; i < mod.columns; i++) {
            const drop_t *d = &mod.drop[i];
            const int head_y = d->y >> SAVER_FRAME_SHF;
            int first_j = head_y >= mod.base.screen_h ? (head_y - (mod.base.screen_h - 1)) / mod.cell : 0;
            if (first_j < 0) first_j = 0;
            int last_j = d->len - 1;
            const int max_visible = head_y / mod.cell + 1;
            if (last_j > max_visible) last_j = max_visible;

            for (int j = first_j; j <= last_j; j++) {
                const int y = head_y - j * mod.cell;
                if (y + mod.cell < 0 || y >= mod.base.screen_h) continue;
                int group;
                if (j == 0)
                    group = 8;
                else if (j == 1)
                    group = 9;
                else {
                    const int t = (j * 1024) / d->len;
                    int alpha = 16 + ((1024 - t * t / 1024) * 160) / 1024;
                    if (alpha < 16) alpha = 16;
                    if (alpha > 200) alpha = 200;
                    group = alpha >> 5;
                    if (group > 7) group = 7;
                }
                if ((size_t) counts[group] >= mod.batch_capacity) continue;
                SDL_Rect rect = {d->x + 1, y + 1, mod.cell - 2, mod.cell - 2};
                if (rect.w < 1) rect.w = 1;
                if (rect.h < 1) rect.h = 1;
                mod.rect_batch[(size_t) group * mod.batch_capacity + (size_t) counts[group]++] = rect;
            }

            if (head_y >= 0 && head_y < mod.base.screen_h && (size_t) counts[10] < mod.batch_capacity) {
                const int bloom_size = mod.cell + 4;
                mod.rect_batch[10u * mod.batch_capacity + (size_t) counts[10]++] =
                    (SDL_Rect) {d->x + (mod.cell - bloom_size) / 2, head_y + (mod.cell - bloom_size) / 2, bloom_size,
                                bloom_size};
            }
        }

        unsigned calls = 0;
        for (int group = 0; group < 8; group++) {
            if (!counts[group]) continue;
            SDL_SetRenderDrawColor(
                renderer, mod.base.colour_r, mod.base.colour_g, mod.base.colour_b, (uint8_t) (group * 32 + 16)
            );
            SDL_RenderFillRects(renderer, &mod.rect_batch[(size_t) group * mod.batch_capacity], counts[group]);
            calls++;
        }
        if (counts[8]) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRects(renderer, &mod.rect_batch[8u * mod.batch_capacity], counts[8]);
            calls++;
        }
        if (counts[9]) {
            SDL_SetRenderDrawColor(renderer, mid_r, mid_g, mid_b, 220);
            SDL_RenderFillRects(renderer, &mod.rect_batch[9u * mod.batch_capacity], counts[9]);
            calls++;
        }
        if (counts[10]) {
            SDL_SetRenderDrawColor(renderer, mod.base.colour_r, mod.base.colour_g, mod.base.colour_b, 90);
            SDL_RenderFillRects(renderer, &mod.rect_batch[10u * mod.batch_capacity], counts[10]);
            calls++;
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        saver_perf_note_draw_calls(calls);
        return;
    }

    unsigned draw_calls = 0;

    for (int i = 0; i < mod.columns; i++) {
        const drop_t *d = &mod.drop[i];
        int head_y = d->y >> SAVER_FRAME_SHF;

        if (head_y - d->len * mod.cell > mod.base.screen_h) continue;

        int first_j = 0;
        if (head_y >= mod.base.screen_h) {
            first_j = (head_y - (mod.base.screen_h - 1)) / mod.cell;
            if (first_j < 0) first_j = 0;
        }

        int last_j = d->len - 1;
        int max_visible = head_y / mod.cell + 1;

        if (last_j > max_visible) last_j = max_visible;
        if (last_j < 0) last_j = 0;

        if (first_j > last_j) continue;

        for (int j = first_j; j <= last_j; j++) {
            int y = head_y - (j * mod.cell);
            if (y + mod.cell < 0 || y >= mod.base.screen_h) continue;

            uint8_t r, g, b;
            int alpha;

            if (j == 0) {
                r = 255;
                g = 255;
                b = 255;

                alpha = 255;
            } else if (j == 1) {
                r = (uint8_t) ((255 + d->r) / 2);
                g = (uint8_t) ((255 + d->g) / 2);
                b = (uint8_t) ((255 + d->b) / 2);

                alpha = 220;
            } else {
                r = d->r;
                g = d->g;
                b = d->b;
                int t = (j * 1024) / d->len;
                int fade = 1024 - (t * t / 1024);

                alpha = 16 + (fade * 160) / 1024;

                if (alpha < 16) alpha = 16;
                if (alpha > 200) alpha = 200;
            }

            SDL_Rect rect = {d->x + 1, y + 1, mod.cell - 2, mod.cell - 2};

            if (rect.w < 1) rect.w = 1;
            if (rect.h < 1) rect.h = 1;

            SDL_SetRenderDrawColor(renderer, r, g, b, (uint8_t) alpha);
            SDL_RenderFillRect(renderer, &rect);
            draw_calls++;
        }

        if (head_y >= 0 && head_y < mod.base.screen_h) {
            int bloom_size = mod.cell + 4;
            SDL_Rect bloom = {
                d->x + (mod.cell - bloom_size) / 2, head_y + (mod.cell - bloom_size) / 2, bloom_size, bloom_size
            };

            SDL_SetRenderDrawColor(renderer, d->r, d->g, d->b, 90);
            SDL_RenderFillRect(renderer, &bloom);
            draw_calls++;
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    saver_perf_note_draw_calls(draw_calls);
}

int matrix_active(void) {
    return saver_active_base(&mod.base);
}

void matrix_stop(void) {
    saver_stop_base(&mod.base);
}

void matrix_shutdown(void) {
    free(mod.drop);
    mod.drop = NULL;
    mod.columns = 0;
    free(mod.rect_batch);
    mod.rect_batch = NULL;
    mod.batch_capacity = 0;
    saver_shutdown_base(&mod.base);
}
