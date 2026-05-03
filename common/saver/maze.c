#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "../log.h"
#include "../saver.h"
#include "maze.h"

#define MAZE_MIN_CELL 10
#define MAZE_MAX_CELL 24

#define MAZE_DIR_RIGHT 0
#define MAZE_DIR_DOWN  1
#define MAZE_DIR_LEFT  2
#define MAZE_DIR_UP    3

#define MAZE_VISITED 0x01
#define MAZE_TRAIL   0x02

#define MAZE_RUNNER_TRAIL_LEN 12

typedef struct {
    uint8_t flags;
    uint8_t walls;
} maze_cell_t;

typedef struct {
    saver_state_t base;

    maze_cell_t *cell;
    uint16_t *stack;

    int cell_size;
    int cols, rows, cell_count;

    int runner_x, runner_y;
    int target_x, target_y;
    int32_t fx, fy;

    int active_generation;
    int stack_len;

    int32_t speed_fp_per_ms;

    int16_t trail_x[MAZE_RUNNER_TRAIL_LEN];
    int16_t trail_y[MAZE_RUNNER_TRAIL_LEN];
    uint8_t trail_head;
    uint8_t trail_count;
} maze_module_t;

static maze_module_t mod = {0};

static int maze_index(int x, int y) {
    return y * mod.cols + x;
}

static int maze_in_bounds(int x, int y) {
    return x >= 0 && x < mod.cols && y >= 0 && y < mod.rows;
}

static void maze_dir_step(int dir, int *dx, int *dy) {
    *dx = 0;
    *dy = 0;

    if (dir == MAZE_DIR_RIGHT) *dx = 1;
    else if (dir == MAZE_DIR_DOWN) *dy = 1;
    else if (dir == MAZE_DIR_LEFT) *dx = -1;
    else *dy = -1;
}

static int maze_opposite_dir(int dir) {
    return (dir + 2) & 3;
}

static void maze_remove_wall(int x, int y, int dir) {
    int dx, dy;
    maze_dir_step(dir, &dx, &dy);

    int nx = x + dx;
    int ny = y + dy;

    if (!maze_in_bounds(x, y) || !maze_in_bounds(nx, ny)) return;

    mod.cell[maze_index(x, y)].walls &= (uint8_t) ~(1u << dir);
    mod.cell[maze_index(nx, ny)].walls &= (uint8_t) ~(1u << maze_opposite_dir(dir));
}

static void refresh_speed_factor(void) {
    int speed = mod.base.speed;

    if (speed < 1) speed = 1;
    mod.speed_fp_per_ms = (int32_t) ((speed * SAVER_FRAME_ONE) / 1000);

    if (mod.speed_fp_per_ms < SAVER_FRAME_ONE / 32) {
        mod.speed_fp_per_ms = SAVER_FRAME_ONE / 32;
    }
}

static int maze_alloc(void) {
    int shortest = mod.base.screen_w < mod.base.screen_h ? mod.base.screen_w : mod.base.screen_h;
    mod.cell_size = shortest / 42;

    if (mod.cell_size < MAZE_MIN_CELL) mod.cell_size = MAZE_MIN_CELL;
    if (mod.cell_size > MAZE_MAX_CELL) mod.cell_size = MAZE_MAX_CELL;

    mod.cols = mod.base.screen_w / mod.cell_size;
    mod.rows = mod.base.screen_h / mod.cell_size;

    if (mod.cols < 4) mod.cols = 4;
    if (mod.rows < 4) mod.rows = 4;

    mod.cell_count = mod.cols * mod.rows;

    mod.cell = calloc((size_t) mod.cell_count, sizeof(maze_cell_t));
    mod.stack = calloc((size_t) mod.cell_count, sizeof(uint16_t));

    if (!mod.cell || !mod.stack) {
        free(mod.cell);
        free(mod.stack);

        mod.cell = NULL;
        mod.stack = NULL;

        LOG_ERROR("saver", "Maze Runner allocation failed");

        return 0;
    }

    return 1;
}

static void maze_reset(void) {
    for (int i = 0; i < mod.cell_count; i++) {
        mod.cell[i].flags = 0;
        mod.cell[i].walls = 0x0F;
    }

    mod.runner_x = (int) saver_rand_range(mod.cols);
    mod.runner_y = (int) saver_rand_range(mod.rows);

    mod.target_x = mod.runner_x;
    mod.target_y = mod.runner_y;

    mod.fx = ((int32_t) mod.runner_x * mod.cell_size + mod.cell_size / 2) << SAVER_FRAME_SHF;
    mod.fy = ((int32_t) mod.runner_y * mod.cell_size + mod.cell_size / 2) << SAVER_FRAME_SHF;

    mod.stack_len = 0;
    mod.stack[mod.stack_len++] = (uint16_t) maze_index(mod.runner_x, mod.runner_y);
    mod.cell[maze_index(mod.runner_x, mod.runner_y)].flags |= MAZE_VISITED | MAZE_TRAIL;
    mod.active_generation = 1;

    mod.trail_head = 0;
    mod.trail_count = 0;
}

static int maze_pick_unvisited_dir(int x, int y) {
    int dirs[4];
    int count = 0;

    for (int dir = 0; dir < 4; dir++) {
        int dx, dy;

        maze_dir_step(dir, &dx, &dy);

        int nx = x + dx;
        int ny = y + dy;

        if (!maze_in_bounds(nx, ny)) continue;
        if (mod.cell[maze_index(nx, ny)].flags & MAZE_VISITED) continue;

        dirs[count++] = dir;
    }

    if (count <= 0) return -1;

    return dirs[(int) saver_rand_range(count)];
}

static void maze_choose_next_target(void) {
    if (!mod.active_generation) return;

    int dir = maze_pick_unvisited_dir(mod.runner_x, mod.runner_y);
    if (dir >= 0) {
        int dx, dy;
        maze_dir_step(dir, &dx, &dy);

        mod.target_x = mod.runner_x + dx;
        mod.target_y = mod.runner_y + dy;

        maze_remove_wall(mod.runner_x, mod.runner_y, dir);

        return;
    }

    if (mod.stack_len > 1) {
        mod.stack_len--;
        int idx = mod.stack[mod.stack_len - 1];

        mod.target_x = idx % mod.cols;
        mod.target_y = idx / mod.cols;

        return;
    }

    mod.active_generation = 0;
}

static void trail_push(int x, int y) {
    mod.trail_head = (uint8_t) ((mod.trail_head + 1) % MAZE_RUNNER_TRAIL_LEN);

    mod.trail_x[mod.trail_head] = (int16_t) x;
    mod.trail_y[mod.trail_head] = (int16_t) y;

    if (mod.trail_count < MAZE_RUNNER_TRAIL_LEN) mod.trail_count++;
}

static void on_speed_changed(void *user) {
    (void) user;
    refresh_speed_factor();
}

static void on_idle_enter(void *user) {
    (void) user;
    maze_reset();
}

int maze_init(SDL_Renderer *renderer, int screen_w, int screen_h) {
    (void) renderer;

    saver_init_base(&mod.base, screen_w, screen_h, "Maze Runner", 153, 255, 220, on_speed_changed, on_idle_enter, &mod);

    if (!maze_alloc()) {
        mod.base.enabled = 0;
        return 0;
    }

    refresh_speed_factor();
    maze_reset();

    LOG_INFO("saver", "Maze Runner Initialised (%dx%d, grid=%dx%d, cell=%d, speed=%d)",
             screen_w, screen_h, mod.cols, mod.rows, mod.cell_size, mod.base.speed);

    return 1;
}

void maze_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    if (!mod.active_generation) {
        if ((random() & 255) == 0) maze_reset();
        return;
    }

    int target_px = mod.target_x * mod.cell_size + mod.cell_size / 2;
    int target_py = mod.target_y * mod.cell_size + mod.cell_size / 2;

    int px = mod.fx >> SAVER_FRAME_SHF;
    int py = mod.fy >> SAVER_FRAME_SHF;

    int dx = target_px - px;
    int dy = target_py - py;

    int step = (mod.speed_fp_per_ms * (int32_t) elapsed) >> SAVER_FRAME_SHF;
    if (step < 1) step = 1;

    if (saver_int_abs(dx) <= step && saver_int_abs(dy) <= step) {
        mod.runner_x = mod.target_x;
        mod.runner_y = mod.target_y;

        mod.fx = ((int32_t) target_px) << SAVER_FRAME_SHF;
        mod.fy = ((int32_t) target_py) << SAVER_FRAME_SHF;

        int idx = maze_index(mod.runner_x, mod.runner_y);
        if (!(mod.cell[idx].flags & MAZE_VISITED)) {
            mod.cell[idx].flags |= MAZE_VISITED | MAZE_TRAIL;
            if (mod.stack_len < mod.cell_count) mod.stack[mod.stack_len++] = (uint16_t) idx;
        } else {
            mod.cell[idx].flags |= MAZE_TRAIL;
        }

        maze_choose_next_target();
    } else {
        if (dx > 0) mod.fx += ((int32_t) step) << SAVER_FRAME_SHF;
        else if (dx < 0) mod.fx -= ((int32_t) step) << SAVER_FRAME_SHF;

        if (dy > 0) mod.fy += ((int32_t) step) << SAVER_FRAME_SHF;
        else if (dy < 0) mod.fy -= ((int32_t) step) << SAVER_FRAME_SHF;
    }

    trail_push(mod.fx >> SAVER_FRAME_SHF, mod.fy >> SAVER_FRAME_SHF);
}

void maze_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.cell) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    uint8_t wall_r = saver_clamp_u8(mod.base.colour_r + 24);
    uint8_t wall_g = saver_clamp_u8(mod.base.colour_g + 24);
    uint8_t wall_b = saver_clamp_u8(mod.base.colour_b + 24);

    for (int y = 0; y < mod.rows; y++) {
        for (int x = 0; x < mod.cols; x++) {
            const maze_cell_t *cell = &mod.cell[maze_index(x, y)];
            uint8_t flags = cell->flags;
            uint8_t walls = cell->walls;

            int px = x * mod.cell_size;
            int py = y * mod.cell_size;

            if (flags & MAZE_VISITED) {
                SDL_Rect fill = {px + 2, py + 2, mod.cell_size - 3, mod.cell_size - 3};

                SDL_SetRenderDrawColor(renderer, mod.base.colour_r, mod.base.colour_g, mod.base.colour_b, 18);
                SDL_RenderFillRect(renderer, &fill);
            }

            if (flags & MAZE_TRAIL) {
                SDL_Rect fill = {px + mod.cell_size / 3, py + mod.cell_size / 3, mod.cell_size / 3, mod.cell_size / 3};

                SDL_SetRenderDrawColor(renderer, mod.base.colour_r, mod.base.colour_g, mod.base.colour_b, 48);
                SDL_RenderFillRect(renderer, &fill);
            }

            SDL_SetRenderDrawColor(renderer, wall_r, wall_g, wall_b, 118);

            if (walls & (1u << MAZE_DIR_RIGHT))SDL_RenderDrawLine(renderer, px + mod.cell_size, py, px + mod.cell_size, py + mod.cell_size);
            if (walls & (1u << MAZE_DIR_DOWN))SDL_RenderDrawLine(renderer, px, py + mod.cell_size, px + mod.cell_size, py + mod.cell_size);

            if (x == 0 && (walls & (1u << MAZE_DIR_LEFT)))SDL_RenderDrawLine(renderer, px, py, px, py + mod.cell_size);
            if (y == 0 && (walls & (1u << MAZE_DIR_UP)))SDL_RenderDrawLine(renderer, px, py, px + mod.cell_size, py);
        }
    }

    if (mod.trail_count > 1) {
        int cur_idx = mod.trail_head;
        for (int seg = 0; seg < mod.trail_count - 1; seg++) {
            int prev_idx = (cur_idx + MAZE_RUNNER_TRAIL_LEN - 1) % MAZE_RUNNER_TRAIL_LEN;

            int alpha = 200 - (seg * 200 / MAZE_RUNNER_TRAIL_LEN);
            if (alpha < 16) alpha = 16;

            SDL_SetRenderDrawColor(renderer, mod.base.colour_r, mod.base.colour_g, mod.base.colour_b, (uint8_t) alpha);
            SDL_RenderDrawLine(renderer, mod.trail_x[cur_idx], mod.trail_y[cur_idx], mod.trail_x[prev_idx], mod.trail_y[prev_idx]);

            cur_idx = prev_idx;
        }
    }

    int rx = mod.fx >> SAVER_FRAME_SHF;
    int ry = mod.fy >> SAVER_FRAME_SHF;

    int size = mod.cell_size / 2;
    if (size < 4) size = 4;

    SDL_Rect runner = {rx - size / 2, ry - size / 2, size, size};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 230);
    SDL_RenderFillRect(renderer, &runner);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int maze_active(void) {
    return saver_active_base(&mod.base);
}

void maze_stop(void) {
    saver_stop_base(&mod.base);
}

void maze_shutdown(void) {
    free(mod.cell);
    free(mod.stack);
    mod.cell = NULL;
    mod.stack = NULL;
    saver_shutdown_base(&mod.base);
}
