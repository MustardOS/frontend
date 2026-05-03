#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../log.h"
#include "../saver.h"
#include "blockfall.h"

#define BLOCKFALL_TARGET_CELL 36
#define BLOCKFALL_SHAPE_COUNT 7

typedef struct {
    int x;
    int y;
    int shape;
    int rot;
    int target_x;
    int32_t fall_fp;
} piece_t;

typedef struct {
    saver_state_t base;

    int cols, rows;

    uint8_t *grid;
    piece_t piece;

    int cell, board_x, board_y, board_w, board_h;

    int32_t fall_step_per_ms;
    uint32_t last_ai_tick;

    uint32_t lock_flash_until;
    int lock_flash_row_min;
    int lock_flash_row_max;
} blockfall_module_t;

static blockfall_module_t mod = {0};

static const uint16_t shape_table[BLOCKFALL_SHAPE_COUNT][4] = {
        {0x0F00, 0x2222, 0x00F0, 0x4444},
        {0x6600, 0x6600, 0x6600, 0x6600},
        {0x0E40, 0x4C40, 0x4E00, 0x4640},
        {0x06C0, 0x4620, 0x06C0, 0x4620},
        {0x0C60, 0x2640, 0x0C60, 0x2640},
        {0x08E0, 0x6440, 0x0E20, 0x44C0},
        {0x02E0, 0x4460, 0x0E80, 0xC440},
};

static void colour_for_id(int id, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (mod.base.speed >= SAVER_SPEED_COLOUR_THRESHOLD) {
        int p = ((id - 1) % 7) + 1;
        saver_pastel_pick(p, r, g, b);
        return;
    }

    int boost = id * 12;
    *r = saver_clamp_u8(mod.base.colour_r + boost);
    *g = saver_clamp_u8(mod.base.colour_g + boost);
    *b = saver_clamp_u8(mod.base.colour_b + boost);
}

static int shape_has_cell(int shape, int rot, int x, int y) {
    uint16_t mask = shape_table[shape][rot & 3];

    int bit = 15 - (y * 4 + x);
    return (mask & (1u << bit)) != 0;
}

static int piece_collides(const piece_t *p, int ox, int oy, int rot) {
    if (!mod.grid) return 1;

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (!shape_has_cell(p->shape, rot, x, y)) continue;

            int bx = p->x + ox + x;
            int by = p->y + oy + y;

            if (bx < 0 || bx >= mod.cols) return 1;
            if (by >= mod.rows) return 1;
            if (by >= 0 && mod.grid[by * mod.cols + bx]) return 1;
        }
    }

    return 0;
}

static void refresh_speed_factor(void) {
    int speed = mod.base.speed;
    if (speed < 1) speed = 1;

    mod.fall_step_per_ms = (int32_t) ((speed * SAVER_FRAME_ONE) / 7800);

    if (mod.fall_step_per_ms < SAVER_FRAME_ONE / 256) {
        mod.fall_step_per_ms = SAVER_FRAME_ONE / 256;
    }
}

static void clear_board(void) {
    if (mod.grid) memset(mod.grid, 0, (size_t) mod.cols * mod.rows);
}

static void spawn_piece(void) {
    piece_t *p = &mod.piece;

    p->shape = (int) saver_rand_range(BLOCKFALL_SHAPE_COUNT);
    p->rot = (int) saver_rand_range(4);

    p->x = (mod.cols - 4) / 2;
    p->y = -2;

    p->fall_fp = 0;
    p->target_x = (int) saver_rand_range(mod.cols - 3);

    if (p->target_x < 0) p->target_x = 0;
    if (piece_collides(p, 0, 0, p->rot)) clear_board();
}

static void lock_piece(void) {
    const piece_t *p = &mod.piece;
    int row_min = mod.rows;
    int row_max = -1;

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (!shape_has_cell(p->shape, p->rot, x, y)) continue;

            int bx = p->x + x;
            int by = p->y + y;

            if (bx < 0 || bx >= mod.cols || by < 0 || by >= mod.rows) continue;
            mod.grid[by * mod.cols + bx] = (uint8_t) (p->shape + 1);

            if (by < row_min) row_min = by;
            if (by > row_max) row_max = by;
        }
    }

    if (row_max >= 0) {
        mod.lock_flash_until = SDL_GetTicks() + 120;
        mod.lock_flash_row_min = row_min;
        mod.lock_flash_row_max = row_max;
    }
}

static void clear_lines(void) {
    if (!mod.grid) return;
    int row_bytes = mod.cols;

    for (int y = mod.rows - 1; y >= 0; y--) {
        int full = 1;

        for (int x = 0; x < mod.cols; x++) {
            if (!mod.grid[y * mod.cols + x]) {
                full = 0;
                break;
            }
        }

        if (!full) continue;

        for (int yy = y; yy > 0; yy--) {
            memcpy(&mod.grid[yy * mod.cols], &mod.grid[(yy - 1) * mod.cols], (size_t) row_bytes);
        }

        memset(&mod.grid[0], 0, (size_t) row_bytes);
        y++;
    }
}

static void ai_step(void) {
    piece_t *p = &mod.piece;

    if ((random() & 7) == 0) {
        int new_rot = (p->rot + 1) & 3;
        if (!piece_collides(p, 0, 0, new_rot)) p->rot = new_rot;
    }

    if (p->x < p->target_x) {
        if (!piece_collides(p, 1, 0, p->rot)) p->x++;
    } else if (p->x > p->target_x) {
        if (!piece_collides(p, -1, 0, p->rot)) p->x--;
    } else if ((random() & 15) == 0) {
        p->target_x = (int) saver_rand_range(mod.cols - 3);
        if (p->target_x < 0) p->target_x = 0;
    }
}

static void recompute_layout(void) {
    int cell = BLOCKFALL_TARGET_CELL;

    int cols = mod.base.screen_w / cell;
    int rows = mod.base.screen_h / cell;

    if (cols < 6) cols = 6;
    if (rows < 10) rows = 10;

    mod.cols = cols;
    mod.rows = rows;
    mod.cell = cell;

    mod.board_w = cell * cols;
    mod.board_h = cell * rows;

    mod.board_x = (mod.base.screen_w - mod.board_w) / 2;
    mod.board_y = (mod.base.screen_h - mod.board_h) / 2;

    if (mod.board_x < 0) mod.board_x = 0;
    if (mod.board_y < 0) mod.board_y = 0;

    free(mod.grid);
    mod.grid = calloc((size_t) cols * rows, 1);

    if (!mod.grid) {
        mod.cols = 0;
        mod.rows = 0;
        mod.base.enabled = 0;
        LOG_ERROR("saver", "Block Fall allocation failed");
    }
}

static void on_speed_changed(void *user) {
    (void) user;
    refresh_speed_factor();
}

static void on_idle_enter(void *user) {
    (void) user;

    clear_board();
    spawn_piece();

    mod.last_ai_tick = SDL_GetTicks();
    mod.lock_flash_until = 0;
}

int blockfall_init(SDL_Renderer *renderer, int screen_w, int screen_h) {
    (void) renderer;

    saver_init_base(&mod.base, screen_w, screen_h, "Block Fall", 153, 255, 220, on_speed_changed, on_idle_enter, &mod);

    refresh_speed_factor();
    recompute_layout();
    clear_board();
    spawn_piece();

    mod.last_ai_tick = SDL_GetTicks();

    LOG_INFO("saver", "Block Fall Initialised (%dx%d, grid=%dx%d, cell=%d, speed=%d)",
             screen_w, screen_h, mod.cols, mod.rows, mod.cell, mod.base.speed);

    return mod.base.enabled;
}

void blockfall_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) {
        mod.last_ai_tick = now;
        return;
    }

    uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    uint32_t ai_delay = 180;

    if (mod.base.speed >= 240) ai_delay = 90;
    else if (mod.base.speed >= 120) ai_delay = 130;

    if (now - mod.last_ai_tick >= ai_delay) {
        ai_step();
        mod.last_ai_tick = now;
    }

    mod.piece.fall_fp += mod.fall_step_per_ms * (int32_t) elapsed;

    while (mod.piece.fall_fp >= SAVER_FRAME_ONE) {
        mod.piece.fall_fp -= SAVER_FRAME_ONE;
        if (!piece_collides(&mod.piece, 0, 1, mod.piece.rot)) {
            mod.piece.y++;
        } else {
            lock_piece();
            clear_lines();
            spawn_piece();
            break;
        }
    }
}

static void draw_cell(SDL_Renderer *renderer, int gx, int gy, int id, int alpha) {
    if (id <= 0) return;
    uint8_t r, g, b;
    colour_for_id(id, &r, &g, &b);

    SDL_Rect rect = {mod.board_x + gx * mod.cell + 1, mod.board_y + gy * mod.cell + 1, mod.cell - 2, mod.cell - 2};

    if (rect.w < 1) rect.w = 1;
    if (rect.h < 1) rect.h = 1;

    SDL_SetRenderDrawColor(renderer, r, g, b, (uint8_t) alpha);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, (uint8_t) (alpha / 4));
    SDL_RenderDrawLine(renderer, rect.x, rect.y, rect.x + rect.w, rect.y);
    SDL_RenderDrawLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h);
}

void blockfall_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.grid) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    uint32_t now = SDL_GetTicks();
    int flash_active = (now < mod.lock_flash_until);
    int flash_alpha_boost = 0;

    if (flash_active) {
        int remaining = (int) (mod.lock_flash_until - now);
        flash_alpha_boost = (remaining * 60) / 120;
    }

    for (int y = 0; y < mod.rows; y++) {
        for (int x = 0; x < mod.cols; x++) {
            uint8_t id = mod.grid[y * mod.cols + x];
            if (id) {
                int alpha = 180;
                if (flash_active && y >= mod.lock_flash_row_min && y <= mod.lock_flash_row_max) {
                    alpha += flash_alpha_boost;
                    if (alpha > 255) alpha = 255;
                }

                draw_cell(renderer, x, y, id, alpha);
            } else if (mod.cell >= 10) {
                SDL_SetRenderDrawColor(renderer, mod.base.colour_r, mod.base.colour_g, mod.base.colour_b, 10);
                SDL_Rect dot = {mod.board_x + x * mod.cell + mod.cell / 2, mod.board_y + y * mod.cell + mod.cell / 2, 1, 1};
                SDL_RenderFillRect(renderer, &dot);
            }
        }
    }

    const piece_t *p = &mod.piece;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (!shape_has_cell(p->shape, p->rot, x, y)) continue;
            int gx = p->x + x;
            int gy = p->y + y;
            if (gx < 0 || gx >= mod.cols || gy < 0 || gy >= mod.rows) continue;
            draw_cell(renderer, gx, gy, p->shape + 1, 230);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int blockfall_active(void) {
    return saver_active_base(&mod.base);
}

void blockfall_stop(void) {
    saver_stop_base(&mod.base);
}

void blockfall_shutdown(void) {
    free(mod.grid);
    mod.grid = NULL;
    mod.cols = 0;
    mod.rows = 0;
    saver_shutdown_base(&mod.base);
}
