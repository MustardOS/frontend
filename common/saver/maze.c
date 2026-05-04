#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
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

#define MAZE_VISITED       0x01
#define MAZE_RUNNER_TRACE  0x02
#define MAZE_HUNTER_TRACE  0x04
#define MAZE_RUNNER_REGION 0x08
#define MAZE_HUNTER_REGION 0x10

#define MAZE_TRAIL_LEN 16

#define MAZE_RUNNER_PANIC_MIN_MS 650u
#define MAZE_RUNNER_PANIC_MAX_MS 1300u
#define MAZE_RUNNER_MOVE_MIN_MS  2800u
#define MAZE_RUNNER_MOVE_MAX_MS  6200u

#define MAZE_HUNTER_THINK_MIN_MS 900u
#define MAZE_HUNTER_THINK_MAX_MS 1800u
#define MAZE_HUNTER_MOVE_MIN_MS  2600u
#define MAZE_HUNTER_MOVE_MAX_MS  5200u

#define MAZE_RADAR_MAX_RADIUS_CELLS 5
#define MAZE_RADAR_RING_COUNT 3

#define MAZE_RUNNER_R 80
#define MAZE_RUNNER_G 255
#define MAZE_RUNNER_B 110

#define MAZE_HUNTER_R 255
#define MAZE_HUNTER_G 72
#define MAZE_HUNTER_B 72

typedef struct {
    uint8_t flags;
    uint8_t walls;
} maze_cell_t;

typedef struct {
    int cell_x;
    int cell_y;
    int from_x;
    int from_y;
    int target_x;
    int target_y;

    int32_t fx;
    int32_t fy;

    int trail_x[MAZE_TRAIL_LEN];
    int trail_y[MAZE_TRAIL_LEN];
    uint8_t trail_head;
    uint8_t trail_count;
} maze_actor_t;

typedef struct {
    saver_state_t base;

    maze_cell_t *cell;

    uint32_t *runner_stack;
    uint32_t *hunter_stack;
    uint32_t *queue;
    int *parent;
    uint8_t *seen;

    int cell_size;
    int cols;
    int rows;
    int cell_count;

    maze_actor_t runner;
    maze_actor_t hunter;

    int active_generation;
    int chase_active;

    int runner_stack_len;
    int hunter_stack_len;
    int hunter_connect_pending;

    int runner_panicking;
    uint32_t runner_panic_until;
    uint32_t runner_next_panic;
    uint32_t runner_panic_start;
    uint32_t runner_panic_duration;

    int hunter_thinking;
    uint32_t hunter_think_until;
    uint32_t hunter_next_think;
    uint32_t hunter_ping_start;
    uint32_t hunter_ping_duration;

    int32_t runner_speed_fp_per_ms;
    int32_t hunter_speed_fp_per_ms;
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

    switch (dir) {
        case MAZE_DIR_RIGHT:
            *dx = 1;
            break;
        case MAZE_DIR_DOWN:
            *dy = 1;
            break;
        case MAZE_DIR_LEFT:
            *dx = -1;
            break;
        default:
            *dy = -1;
            break;
    }
}

static int maze_opposite_dir(int dir) {
    return (dir + 2) & 3;
}

static int maze_can_move(int x, int y, int dir) {
    int dx;
    int dy;

    int nx;
    int ny;

    if (!maze_in_bounds(x, y)) return 0;

    maze_dir_step(dir, &dx, &dy);

    nx = x + dx;
    ny = y + dy;

    if (!maze_in_bounds(nx, ny)) return 0;

    return (mod.cell[maze_index(x, y)].walls & (uint8_t) (1u << dir)) == 0;
}

static void maze_remove_wall(int x, int y, int dir) {
    int dx;
    int dy;

    int nx;
    int ny;

    maze_dir_step(dir, &dx, &dy);

    nx = x + dx;
    ny = y + dy;

    if (!maze_in_bounds(x, y) || !maze_in_bounds(nx, ny)) return;

    mod.cell[maze_index(x, y)].walls &= (uint8_t) ~(1u << dir);
    mod.cell[maze_index(nx, ny)].walls &= (uint8_t) ~(1u << maze_opposite_dir(dir));
}

static int maze_cell_center_x(int x) {
    return x * mod.cell_size + mod.cell_size / 2;
}

static int maze_cell_center_y(int y) {
    return y * mod.cell_size + mod.cell_size / 2;
}

static int maze_rand_int(int limit) {
    if (limit <= 1) return 0;
    return (int) (saver_rand_range((int32_t) limit) % (uint32_t) limit);
}

static uint32_t maze_rand_u32(int32_t limit) {
    return saver_rand_range(limit) % limit;
}

static uint32_t maze_rand_ms(uint32_t min_ms, int32_t span_ms) {
    return min_ms + maze_rand_u32(span_ms);
}

static void maze_actor_clear_trail(maze_actor_t *actor) {
    actor->trail_head = 0;
    actor->trail_count = 0;
}

static void maze_actor_push_trail(maze_actor_t *actor, int px, int py) {
    actor->trail_head = (uint8_t) ((actor->trail_head + 1) % MAZE_TRAIL_LEN);

    actor->trail_x[actor->trail_head] = px;
    actor->trail_y[actor->trail_head] = py;

    if (actor->trail_count < MAZE_TRAIL_LEN) actor->trail_count++;
}

static void maze_actor_snap(maze_actor_t *actor, int x, int y) {
    actor->cell_x = x;
    actor->cell_y = y;

    actor->from_x = x;
    actor->from_y = y;

    actor->target_x = x;
    actor->target_y = y;

    actor->fx = ((int32_t) maze_cell_center_x(x)) << SAVER_FRAME_SHF;
    actor->fy = ((int32_t) maze_cell_center_y(y)) << SAVER_FRAME_SHF;

    maze_actor_clear_trail(actor);
    maze_actor_push_trail(actor, actor->fx >> SAVER_FRAME_SHF, actor->fy >> SAVER_FRAME_SHF);
}

static void maze_actor_set_target(maze_actor_t *actor, int x, int y) {
    actor->from_x = actor->cell_x;
    actor->from_y = actor->cell_y;

    actor->target_x = x;
    actor->target_y = y;
}

static int maze_axis_delta_step(int delta, int step) {
    if (delta > step) return step;
    if (delta < -step) return -step;

    return delta;
}

static int maze_actor_step(maze_actor_t *actor, int32_t speed_fp_per_ms, uint32_t elapsed) {
    int target_px;
    int target_py;

    int px;
    int py;

    int dx;
    int dy;

    int step;

    int move_x;
    int move_y;

    target_px = maze_cell_center_x(actor->target_x);
    target_py = maze_cell_center_y(actor->target_y);

    px = actor->fx >> SAVER_FRAME_SHF;
    py = actor->fy >> SAVER_FRAME_SHF;

    dx = target_px - px;
    dy = target_py - py;

    step = (int) ((speed_fp_per_ms * (int32_t) elapsed) >> SAVER_FRAME_SHF);
    if (step < 1) step = 1;

    if (dx == 0 && dy == 0) {
        maze_actor_push_trail(actor, px, py);
        return 1;
    }

    if (saver_int_abs(dx) <= step && saver_int_abs(dy) <= step) {
        actor->cell_x = actor->target_x;
        actor->cell_y = actor->target_y;

        actor->fx = ((int32_t) target_px) << SAVER_FRAME_SHF;
        actor->fy = ((int32_t) target_py) << SAVER_FRAME_SHF;

        maze_actor_push_trail(actor, target_px, target_py);
        return 1;
    }

    move_x = maze_axis_delta_step(dx, step);
    move_y = maze_axis_delta_step(dy, step);

    actor->fx += ((int32_t) move_x) << SAVER_FRAME_SHF;
    actor->fy += ((int32_t) move_y) << SAVER_FRAME_SHF;

    maze_actor_push_trail(actor, actor->fx >> SAVER_FRAME_SHF, actor->fy >> SAVER_FRAME_SHF);
    return 0;
}

static void maze_mark_runner_cell(int x, int y) {
    int idx;

    if (!maze_in_bounds(x, y)) return;

    idx = maze_index(x, y);
    mod.cell[idx].flags |= (uint8_t) (MAZE_VISITED | MAZE_RUNNER_REGION | MAZE_RUNNER_TRACE);
}

static void maze_mark_trace(int x, int y) {
    if (!maze_in_bounds(x, y)) return;
    mod.cell[maze_index(x, y)].flags |= MAZE_HUNTER_TRACE;
}

static void maze_clear_trace_flags(void) {
    int i;

    for (i = 0; i < mod.cell_count; i++) {
        mod.cell[i].flags &= (uint8_t) ~(MAZE_RUNNER_TRACE | MAZE_HUNTER_TRACE);
    }
}

static void refresh_speed_factor(void) {
    int speed;

    speed = mod.base.speed;
    if (speed < 1) speed = 1;

    mod.runner_speed_fp_per_ms = (int32_t) ((speed * SAVER_FRAME_ONE) / 1000);
    if (mod.runner_speed_fp_per_ms < SAVER_FRAME_ONE / 32) {
        mod.runner_speed_fp_per_ms = SAVER_FRAME_ONE / 32;
    }

    mod.hunter_speed_fp_per_ms = (mod.runner_speed_fp_per_ms * 9) / 8;
    if (mod.hunter_speed_fp_per_ms < mod.runner_speed_fp_per_ms) {
        mod.hunter_speed_fp_per_ms = mod.runner_speed_fp_per_ms;
    }
}

static int maze_alloc(void) {
    int shortest;

    shortest = mod.base.screen_w < mod.base.screen_h ? mod.base.screen_w : mod.base.screen_h;
    mod.cell_size = shortest / 42;

    if (mod.cell_size < MAZE_MIN_CELL) mod.cell_size = MAZE_MIN_CELL;
    if (mod.cell_size > MAZE_MAX_CELL) mod.cell_size = MAZE_MAX_CELL;

    mod.cols = mod.base.screen_w / mod.cell_size;
    mod.rows = mod.base.screen_h / mod.cell_size;

    if (mod.cols < 4) mod.cols = 4;
    if (mod.rows < 4) mod.rows = 4;

    mod.cell_count = mod.cols * mod.rows;

    mod.cell = calloc((size_t) mod.cell_count, sizeof(*mod.cell));
    mod.runner_stack = calloc((size_t) mod.cell_count, sizeof(*mod.runner_stack));
    mod.hunter_stack = calloc((size_t) mod.cell_count, sizeof(*mod.hunter_stack));
    mod.queue = calloc((size_t) mod.cell_count, sizeof(*mod.queue));
    mod.parent = malloc((size_t) mod.cell_count * sizeof(*mod.parent));
    mod.seen = calloc((size_t) mod.cell_count, sizeof(*mod.seen));

    if (!mod.cell || !mod.runner_stack || !mod.hunter_stack || !mod.queue || !mod.parent || !mod.seen) {
        free(mod.cell);
        free(mod.runner_stack);
        free(mod.hunter_stack);
        free(mod.queue);
        free(mod.parent);
        free(mod.seen);

        mod.cell = NULL;
        mod.runner_stack = NULL;
        mod.hunter_stack = NULL;
        mod.queue = NULL;
        mod.parent = NULL;
        mod.seen = NULL;

        LOG_ERROR("saver", "Maze Hunter allocation failed");
        return 0;
    }

    return 1;
}

static void maze_find_far_spawn(int start_x, int start_y, int *out_x, int *out_y) {
    int best_x;
    int best_y;

    int best_score;

    int x;
    int y;

    best_x = start_x;
    best_y = start_y;
    best_score = -1;

    for (y = 0; y < mod.rows; y++) {
        for (x = 0; x < mod.cols; x++) {
            int dx;
            int dy;
            int score;

            dx = x - start_x;
            dy = y - start_y;
            score = dx * dx + dy * dy;

            if (score > best_score) {
                best_score = score;
                best_x = x;
                best_y = y;
            }
        }
    }

    *out_x = best_x;
    *out_y = best_y;
}

static int maze_pick_runner_generation_dir(int x, int y) {
    int dirs[4];
    int count;
    int dir;

    count = 0;

    for (dir = 0; dir < 4; dir++) {
        int dx;
        int dy;

        int nx;
        int ny;
        uint8_t flags;

        maze_dir_step(dir, &dx, &dy);

        nx = x + dx;
        ny = y + dy;

        if (!maze_in_bounds(nx, ny)) continue;

        flags = mod.cell[maze_index(nx, ny)].flags;

        if (flags & MAZE_VISITED) continue;

        dirs[count++] = dir;
    }

    if (count <= 0) return -1;
    return dirs[maze_rand_int(count)];
}

static int maze_pick_hunter_connect_dir(int x, int y) {
    int dirs[4];
    int count;
    int dir;

    count = 0;

    for (dir = 0; dir < 4; dir++) {
        int dx;
        int dy;

        int nx;
        int ny;
        uint8_t flags;

        maze_dir_step(dir, &dx, &dy);

        nx = x + dx;
        ny = y + dy;

        if (!maze_in_bounds(nx, ny)) continue;

        flags = mod.cell[maze_index(nx, ny)].flags;

        if ((flags & MAZE_RUNNER_REGION) == 0) continue;

        dirs[count++] = dir;
    }

    if (count <= 0) return -1;
    return dirs[maze_rand_int(count)];
}

static int maze_pick_hunter_generation_dir(int x, int y) {
    int dirs[4];
    int count;
    int dir;

    count = 0;

    for (dir = 0; dir < 4; dir++) {
        int dx;
        int dy;

        int nx;
        int ny;

        uint8_t flags;

        maze_dir_step(dir, &dx, &dy);

        nx = x + dx;
        ny = y + dy;

        if (!maze_in_bounds(nx, ny)) continue;

        flags = mod.cell[maze_index(nx, ny)].flags;

        if (flags & MAZE_VISITED) continue;

        dirs[count++] = dir;
    }

    if (count <= 0) return -1;
    return dirs[maze_rand_int(count)];
}

static void maze_runner_generation_choose_target(void) {
    int dir;

    if (!mod.active_generation) return;

    dir = maze_pick_runner_generation_dir(mod.runner.cell_x, mod.runner.cell_y);

    if (dir >= 0) {
        int dx;
        int dy;

        maze_dir_step(dir, &dx, &dy);
        maze_remove_wall(mod.runner.cell_x, mod.runner.cell_y, dir);
        maze_actor_set_target(&mod.runner, mod.runner.cell_x + dx, mod.runner.cell_y + dy);

        return;
    }

    if (mod.runner_stack_len > 1) {
        uint32_t idx;

        mod.runner_stack_len--;
        idx = mod.runner_stack[mod.runner_stack_len - 1];

        maze_actor_set_target(&mod.runner, (int) (idx % (uint32_t) mod.cols), (int) (idx / (uint32_t) mod.cols));
    }
}

static void maze_hunter_generation_choose_target(void) {
    int dir;

    if (!mod.active_generation) return;

    dir = maze_pick_hunter_connect_dir(mod.hunter.cell_x, mod.hunter.cell_y);

    if (dir >= 0) {
        int dx;
        int dy;

        maze_dir_step(dir, &dx, &dy);
        maze_remove_wall(mod.hunter.cell_x, mod.hunter.cell_y, dir);
        maze_actor_set_target(&mod.hunter, mod.hunter.cell_x + dx, mod.hunter.cell_y + dy);
        mod.hunter_connect_pending = 1;

        return;
    }

    dir = maze_pick_hunter_generation_dir(mod.hunter.cell_x, mod.hunter.cell_y);

    if (dir >= 0) {
        int dx;
        int dy;

        maze_dir_step(dir, &dx, &dy);
        maze_remove_wall(mod.hunter.cell_x, mod.hunter.cell_y, dir);
        maze_actor_set_target(&mod.hunter, mod.hunter.cell_x + dx, mod.hunter.cell_y + dy);

        return;
    }

    if (mod.hunter_stack_len > 1) {
        uint32_t idx;

        mod.hunter_stack_len--;
        idx = mod.hunter_stack[mod.hunter_stack_len - 1];

        maze_actor_set_target(&mod.hunter, (int) (idx % (uint32_t) mod.cols), (int) (idx / (uint32_t) mod.cols));
    }
}

static int maze_find_first_step_towards(int start_x, int start_y, int goal_x, int goal_y, int *next_x, int *next_y) {
    int head;
    int tail;

    int start_idx;
    int goal_idx;

    int i;

    if (start_x == goal_x && start_y == goal_y) return 0;

    start_idx = maze_index(start_x, start_y);
    goal_idx = maze_index(goal_x, goal_y);

    for (i = 0; i < mod.cell_count; i++) {
        mod.parent[i] = -1;
        mod.seen[i] = 0;
    }

    head = 0;
    tail = 0;

    mod.queue[tail++] = (uint32_t) start_idx;
    mod.seen[start_idx] = 1;

    while (head < tail) {
        int idx;

        int x;
        int y;

        int dir;

        idx = (int) mod.queue[head++];
        x = idx % mod.cols;
        y = idx / mod.cols;

        if (idx == goal_idx) break;

        for (dir = 0; dir < 4; dir++) {
            int dx;
            int dy;

            int nx;
            int ny;

            int nidx;

            if (!maze_can_move(x, y, dir)) continue;

            maze_dir_step(dir, &dx, &dy);

            nx = x + dx;
            ny = y + dy;
            nidx = maze_index(nx, ny);

            if (mod.seen[nidx]) continue;

            mod.seen[nidx] = 1;
            mod.parent[nidx] = idx;
            mod.queue[tail++] = (uint32_t) nidx;
        }
    }

    if (!mod.seen[goal_idx]) return 0;

    i = goal_idx;
    while (mod.parent[i] != -1 && mod.parent[i] != start_idx) {
        i = mod.parent[i];
    }

    if (mod.parent[i] == -1) return 0;

    *next_x = i % mod.cols;
    *next_y = i / mod.cols;

    return 1;
}

static void maze_choose_runner_target(void) {
    int best_x;
    int best_y;

    int best_dir;
    int best_score;

    int found;
    int dir;

    best_x = mod.runner.cell_x;
    best_y = mod.runner.cell_y;

    best_dir = -1;
    best_score = INT_MIN;

    found = 0;

    for (dir = 0; dir < 4; dir++) {
        int dx;
        int dy;

        int nx;
        int ny;

        int nidx;

        int open_path;
        int unvisited;

        int ddx;
        int ddy;

        int score;

        maze_dir_step(dir, &dx, &dy);

        nx = mod.runner.cell_x + dx;
        ny = mod.runner.cell_y + dy;

        if (!maze_in_bounds(nx, ny)) continue;
        if (nx == mod.hunter.cell_x && ny == mod.hunter.cell_y) continue;

        nidx = maze_index(nx, ny);
        open_path = maze_can_move(mod.runner.cell_x, mod.runner.cell_y, dir);
        unvisited = (mod.cell[nidx].flags & MAZE_VISITED) == 0;

        if (!open_path && !unvisited) continue;

        ddx = nx - mod.hunter.cell_x;
        ddy = ny - mod.hunter.cell_y;

        score = (ddx * ddx + ddy * ddy) * 32;

        if (unvisited) score += 160;
        if (nx == mod.runner.from_x && ny == mod.runner.from_y) score -= 28;

        score += maze_rand_int(23);

        if (!found || score > best_score) {
            found = 1;
            best_score = score;
            best_x = nx;
            best_y = ny;
            best_dir = dir;
        }
    }

    if (!found) return;

    if (!maze_can_move(mod.runner.cell_x, mod.runner.cell_y, best_dir)) {
        maze_remove_wall(mod.runner.cell_x, mod.runner.cell_y, best_dir);
    }

    maze_actor_set_target(&mod.runner, best_x, best_y);
}

static void maze_choose_hunter_target(void) {
    int next_x;
    int next_y;

    if (maze_find_first_step_towards(mod.hunter.cell_x, mod.hunter.cell_y, mod.runner.cell_x, mod.runner.cell_y, &next_x, &next_y)) {
        maze_actor_set_target(&mod.hunter, next_x, next_y);
    }
}

static void maze_schedule_runner_panic(uint32_t now) {
    mod.runner_panicking = 0;
    mod.runner_next_panic = now + maze_rand_ms(MAZE_RUNNER_MOVE_MIN_MS, MAZE_RUNNER_MOVE_MAX_MS - MAZE_RUNNER_MOVE_MIN_MS + 1u);
    mod.runner_panic_until = 0;
    mod.runner_panic_start = 0;
    mod.runner_panic_duration = 0;
}

static void maze_begin_runner_panic(uint32_t now) {
    mod.runner_panicking = 1;
    mod.runner_panic_start = now;
    mod.runner_panic_duration = maze_rand_ms(MAZE_RUNNER_PANIC_MIN_MS, MAZE_RUNNER_PANIC_MAX_MS - MAZE_RUNNER_PANIC_MIN_MS + 1u);
    mod.runner_panic_until = now + mod.runner_panic_duration;
}

static void maze_get_runner_panic_offset(int *out_x, int *out_y) {
    uint32_t now;
    uint32_t elapsed;
    int amp;
    int phase;

    *out_x = 0;
    *out_y = 0;

    if (!mod.runner_panicking || mod.runner_panic_duration == 0) return;

    now = SDL_GetTicks();
    elapsed = now - mod.runner_panic_start;

    if (elapsed >= mod.runner_panic_duration) return;

    amp = mod.cell_size / 5;
    if (amp < 2) amp = 2;
    if (amp > 8) amp = 8;

    phase = (int) ((elapsed / 38u) & 7u);

    switch (phase) {
        case 0:
            *out_x = -amp;
            *out_y = 0;
            break;
        case 1:
            *out_x = amp;
            *out_y = -amp / 2;
            break;
        case 2:
            *out_x = -amp / 2;
            *out_y = amp;
            break;
        case 3:
            *out_x = amp / 2;
            *out_y = -amp;
            break;
        case 4:
            *out_x = -amp;
            *out_y = amp / 2;
            break;
        case 5:
            *out_x = amp;
            *out_y = amp;
            break;
        case 6:
            *out_x = -amp / 2;
            *out_y = -amp;
            break;
        default:
            *out_x = amp / 2;
            *out_y = amp / 2;
            break;
    }
}

static void maze_schedule_hunter_think(uint32_t now) {
    mod.hunter_thinking = 0;
    mod.hunter_next_think = now + maze_rand_ms(MAZE_HUNTER_MOVE_MIN_MS, MAZE_HUNTER_MOVE_MAX_MS - MAZE_HUNTER_MOVE_MIN_MS + 1u);
    mod.hunter_think_until = 0;
    mod.hunter_ping_start = 0;
    mod.hunter_ping_duration = 0;
}

static void maze_begin_hunter_think(uint32_t now) {
    mod.hunter_thinking = 1;
    mod.hunter_ping_start = now;
    mod.hunter_ping_duration = maze_rand_ms(MAZE_HUNTER_THINK_MIN_MS, MAZE_HUNTER_THINK_MAX_MS - MAZE_HUNTER_THINK_MIN_MS + 1u);
    mod.hunter_think_until = now + mod.hunter_ping_duration;
}

static void maze_begin_chase(void) {
    uint32_t now;

    mod.active_generation = 0;
    mod.chase_active = 1;
    mod.hunter_connect_pending = 0;

    maze_clear_trace_flags();

    maze_mark_runner_cell(mod.runner.cell_x, mod.runner.cell_y);
    maze_mark_trace(mod.hunter.cell_x, mod.hunter.cell_y);

    now = SDL_GetTicks();

    maze_schedule_runner_panic(now);
    maze_schedule_hunter_think(now);

    maze_choose_runner_target();
    maze_choose_hunter_target();
}

static void maze_reset(void) {
    int i;

    int runner_x;
    int runner_y;

    int hunter_x;
    int hunter_y;

    int runner_idx;
    int hunter_idx;

    for (i = 0; i < mod.cell_count; i++) {
        mod.cell[i].flags = 0;
        mod.cell[i].walls = 0x0F;
    }

    runner_x = maze_rand_int(mod.cols);
    runner_y = maze_rand_int(mod.rows);

    maze_find_far_spawn(runner_x, runner_y, &hunter_x, &hunter_y);

    maze_actor_snap(&mod.runner, runner_x, runner_y);
    maze_actor_snap(&mod.hunter, hunter_x, hunter_y);

    runner_idx = maze_index(runner_x, runner_y);
    hunter_idx = maze_index(hunter_x, hunter_y);

    mod.runner_stack_len = 0;
    mod.hunter_stack_len = 0;

    mod.runner_stack[mod.runner_stack_len++] = (uint32_t) runner_idx;
    mod.hunter_stack[mod.hunter_stack_len++] = (uint32_t) hunter_idx;

    mod.cell[runner_idx].flags |= (uint8_t) (MAZE_VISITED | MAZE_RUNNER_REGION | MAZE_RUNNER_TRACE);
    mod.cell[hunter_idx].flags |= (uint8_t) (MAZE_VISITED | MAZE_HUNTER_REGION | MAZE_HUNTER_TRACE);

    mod.active_generation = 1;
    mod.chase_active = 0;
    mod.hunter_connect_pending = 0;

    mod.runner_panicking = 0;
    mod.runner_panic_until = 0;
    mod.runner_next_panic = 0;
    mod.runner_panic_start = 0;
    mod.runner_panic_duration = 0;

    mod.hunter_thinking = 0;
    mod.hunter_think_until = 0;
    mod.hunter_next_think = 0;
    mod.hunter_ping_start = 0;
    mod.hunter_ping_duration = 0;

    maze_runner_generation_choose_target();
    maze_hunter_generation_choose_target();
}

static void maze_update_generation_actor_runner(uint32_t elapsed) {
    int arrived;
    int idx;

    arrived = maze_actor_step(&mod.runner, mod.runner_speed_fp_per_ms, elapsed);
    if (!arrived) return;

    idx = maze_index(mod.runner.cell_x, mod.runner.cell_y);

    if ((mod.cell[idx].flags & MAZE_VISITED) == 0) {
        mod.cell[idx].flags |= (uint8_t) (MAZE_VISITED | MAZE_RUNNER_REGION);
        if (mod.runner_stack_len < mod.cell_count) mod.runner_stack[mod.runner_stack_len++] = (uint32_t) idx;
    }

    mod.cell[idx].flags |= (uint8_t) (MAZE_RUNNER_REGION | MAZE_RUNNER_TRACE);

    maze_runner_generation_choose_target();
}

static void maze_update_generation_actor_hunter(uint32_t elapsed) {
    int arrived;
    int idx;

    arrived = maze_actor_step(&mod.hunter, mod.hunter_speed_fp_per_ms, elapsed);
    if (!arrived) return;

    idx = maze_index(mod.hunter.cell_x, mod.hunter.cell_y);

    mod.cell[idx].flags |= MAZE_HUNTER_TRACE;

    if (mod.hunter_connect_pending) {
        maze_begin_chase();
        return;
    }

    if ((mod.cell[idx].flags & MAZE_VISITED) == 0) {
        mod.cell[idx].flags |= (uint8_t) (MAZE_VISITED | MAZE_HUNTER_REGION);
        if (mod.hunter_stack_len < mod.cell_count) mod.hunter_stack[mod.hunter_stack_len++] = (uint32_t) idx;
    }

    mod.cell[idx].flags |= (uint8_t) (MAZE_HUNTER_REGION | MAZE_HUNTER_TRACE);

    maze_hunter_generation_choose_target();
}

static void maze_update_generation(uint32_t elapsed) {
    maze_update_generation_actor_runner(elapsed);

    if (!mod.active_generation) return;

    maze_update_generation_actor_hunter(elapsed);
}

static void maze_update_chase(uint32_t elapsed) {
    int runner_arrived;
    int hunter_arrived;
    uint32_t now;

    now = SDL_GetTicks();

    if (mod.runner.cell_x == mod.hunter.cell_x && mod.runner.cell_y == mod.hunter.cell_y) {
        maze_reset();
        return;
    }

    if (mod.runner_panicking) {
        if ((int32_t) (now - mod.runner_panic_until) >= 0) {
            maze_schedule_runner_panic(now);
            maze_choose_runner_target();
        }
    } else if ((int32_t) (now - mod.runner_next_panic) >= 0) {
        maze_begin_runner_panic(now);
    } else {
        runner_arrived = maze_actor_step(&mod.runner, mod.runner_speed_fp_per_ms, elapsed);

        if (runner_arrived) {
            maze_mark_runner_cell(mod.runner.cell_x, mod.runner.cell_y);
            maze_choose_runner_target();
        }
    }

    if (mod.runner.cell_x == mod.hunter.cell_x && mod.runner.cell_y == mod.hunter.cell_y) {
        maze_reset();
        return;
    }

    if (mod.hunter_thinking) {
        if ((int32_t) (now - mod.hunter_think_until) >= 0) {
            maze_schedule_hunter_think(now);
            maze_choose_hunter_target();
        }

        return;
    }

    if ((int32_t) (now - mod.hunter_next_think) >= 0) {
        maze_begin_hunter_think(now);
        return;
    }

    hunter_arrived = maze_actor_step(&mod.hunter, mod.hunter_speed_fp_per_ms, elapsed);

    if (hunter_arrived) {
        maze_mark_trace(mod.hunter.cell_x, mod.hunter.cell_y);
        maze_choose_hunter_target();
    }

    if (mod.runner.cell_x == mod.hunter.cell_x && mod.runner.cell_y == mod.hunter.cell_y) {
        maze_reset();
    }
}

static void maze_render_circle(SDL_Renderer *renderer, int cx, int cy, int radius) {
    int x;
    int y;
    int err;

    if (radius <= 0) return;

    x = radius;
    y = 0;
    err = 0;

    while (x >= y) {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(renderer, cx + y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(renderer, cx - x, cy - y);
        SDL_RenderDrawPoint(renderer, cx - y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + x, cy - y);

        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }

        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static void maze_render_trail(SDL_Renderer *renderer, const maze_actor_t *actor, uint8_t r, uint8_t g, uint8_t b) {
    int seg;
    int cur_idx;

    if (actor->trail_count <= 1) return;

    cur_idx = actor->trail_head;

    for (seg = 0; seg < actor->trail_count - 1; seg++) {
        int prev_idx;
        int alpha;

        prev_idx = (cur_idx + MAZE_TRAIL_LEN - 1) % MAZE_TRAIL_LEN;
        alpha = 220 - (seg * 220 / MAZE_TRAIL_LEN);
        if (alpha < 20) alpha = 20;

        SDL_SetRenderDrawColor(renderer, r, g, b, (uint8_t) alpha);
        SDL_RenderDrawLine(renderer, actor->trail_x[cur_idx], actor->trail_y[cur_idx], actor->trail_x[prev_idx], actor->trail_y[prev_idx]);

        cur_idx = prev_idx;
    }
}

static void maze_render_hunter_radar(SDL_Renderer *renderer) {
    uint32_t now;
    uint32_t elapsed;

    int cx;
    int cy;

    int max_radius;
    int ring;

    if (!mod.chase_active || !mod.hunter_thinking || mod.hunter_ping_duration == 0) return;

    now = SDL_GetTicks();
    elapsed = now - mod.hunter_ping_start;

    if (elapsed >= mod.hunter_ping_duration) return;

    cx = mod.hunter.fx >> SAVER_FRAME_SHF;
    cy = mod.hunter.fy >> SAVER_FRAME_SHF;

    max_radius = mod.cell_size * MAZE_RADAR_MAX_RADIUS_CELLS;
    if (max_radius < mod.cell_size * 2) {
        max_radius = mod.cell_size * 2;
    }

    for (ring = 0; ring < MAZE_RADAR_RING_COUNT; ring++) {
        uint32_t shifted;
        int radius;
        int alpha;

        shifted = elapsed + (mod.hunter_ping_duration * (uint32_t) ring) / MAZE_RADAR_RING_COUNT;
        shifted %= mod.hunter_ping_duration;

        radius = (int) ((shifted * (uint32_t) max_radius) / mod.hunter_ping_duration);
        alpha = 190 - (int) ((shifted * 170u) / mod.hunter_ping_duration);

        if (alpha < 16) alpha = 16;

        SDL_SetRenderDrawColor(renderer, MAZE_HUNTER_R, MAZE_HUNTER_G, MAZE_HUNTER_B, (uint8_t) alpha);
        maze_render_circle(renderer, cx, cy, radius);

        if (radius > 2) {
            SDL_SetRenderDrawColor(renderer, MAZE_HUNTER_R, MAZE_HUNTER_G, MAZE_HUNTER_B, (uint8_t) (alpha / 2));
            maze_render_circle(renderer, cx, cy, radius - 1);
        }
    }
}

static void maze_render_actor_offset(SDL_Renderer *renderer, const maze_actor_t *actor, uint8_t r, uint8_t g, uint8_t b, int off_x, int off_y) {
    SDL_Rect rect;
    int px;
    int py;
    int size;

    px = (actor->fx >> SAVER_FRAME_SHF) + off_x;
    py = (actor->fy >> SAVER_FRAME_SHF) + off_y;

    size = mod.cell_size / 2;
    if (size < 4) size = 4;

    rect.x = px - size / 2;
    rect.y = py - size / 2;
    rect.w = size;
    rect.h = size;

    SDL_SetRenderDrawColor(renderer, r, g, b, 235);
    SDL_RenderFillRect(renderer, &rect);
}

static void maze_render_actor(SDL_Renderer *renderer, const maze_actor_t *actor, uint8_t r, uint8_t g, uint8_t b) {
    maze_render_actor_offset(renderer, actor, r, g, b, 0, 0);
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

    saver_init_base(&mod.base, screen_w, screen_h, "Maze Runner", 130, 180, 220, on_speed_changed, on_idle_enter, &mod);

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
    uint32_t now;
    uint32_t elapsed;

    now = SDL_GetTicks();

    if (!saver_poll_idle(&mod.base, now)) return;

    elapsed = now - mod.base.last_tick;
    if (!elapsed) return;

    mod.base.last_tick = now;

    if (mod.active_generation) {
        maze_update_generation(elapsed);
        return;
    }

    if (mod.chase_active) {
        maze_update_chase(elapsed);
        return;
    }

    maze_reset();
}

void maze_render(SDL_Renderer *renderer) {
    int y;
    int x;

    uint8_t wall_r;
    uint8_t wall_g;
    uint8_t wall_b;

    if (!mod.base.enabled || !mod.base.idle_active || !mod.cell) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    wall_r = saver_clamp_u8(mod.base.colour_r + 28);
    wall_g = saver_clamp_u8(mod.base.colour_g + 28);
    wall_b = saver_clamp_u8(mod.base.colour_b + 28);

    for (y = 0; y < mod.rows; y++) {
        for (x = 0; x < mod.cols; x++) {
            const maze_cell_t *cell;

            uint8_t flags;
            uint8_t walls;

            int px;
            int py;

            cell = &mod.cell[maze_index(x, y)];
            flags = cell->flags;
            walls = cell->walls;

            px = x * mod.cell_size;
            py = y * mod.cell_size;

            if (flags & MAZE_VISITED) {
                SDL_Rect fill = {px + 2, py + 2, mod.cell_size - 3, mod.cell_size - 3};

                SDL_SetRenderDrawColor(renderer, mod.base.colour_r, mod.base.colour_g, mod.base.colour_b, 12);
                SDL_RenderFillRect(renderer, &fill);
            }

            if (flags & MAZE_RUNNER_TRACE) {
                SDL_Rect fill = {px + mod.cell_size / 3, py + mod.cell_size / 3, mod.cell_size / 3, mod.cell_size / 3};

                SDL_SetRenderDrawColor(renderer, MAZE_RUNNER_R, MAZE_RUNNER_G, MAZE_RUNNER_B, 86);
                SDL_RenderFillRect(renderer, &fill);
            }

            if (flags & MAZE_HUNTER_TRACE) {
                SDL_Rect fill = {px + mod.cell_size / 3, py + mod.cell_size / 3, mod.cell_size / 3, mod.cell_size / 3};

                SDL_SetRenderDrawColor(renderer, MAZE_HUNTER_R, MAZE_HUNTER_G, MAZE_HUNTER_B, 86);
                SDL_RenderFillRect(renderer, &fill);
            }

            SDL_SetRenderDrawColor(renderer, wall_r, wall_g, wall_b, 118);

            if (walls & (1u << MAZE_DIR_RIGHT)) SDL_RenderDrawLine(renderer, px + mod.cell_size, py, px + mod.cell_size, py + mod.cell_size);
            if (walls & (1u << MAZE_DIR_DOWN)) SDL_RenderDrawLine(renderer, px, py + mod.cell_size, px + mod.cell_size, py + mod.cell_size);

            if (x == 0 && (walls & (1u << MAZE_DIR_LEFT))) SDL_RenderDrawLine(renderer, px, py, px, py + mod.cell_size);
            if (y == 0 && (walls & (1u << MAZE_DIR_UP))) SDL_RenderDrawLine(renderer, px, py, px + mod.cell_size, py);
        }
    }

    maze_render_trail(renderer, &mod.runner, MAZE_RUNNER_R, MAZE_RUNNER_G, MAZE_RUNNER_B);
    maze_render_trail(renderer, &mod.hunter, MAZE_HUNTER_R, MAZE_HUNTER_G, MAZE_HUNTER_B);

    if (mod.chase_active) maze_render_hunter_radar(renderer);

    if (mod.runner_panicking) {
        int panic_x;
        int panic_y;
        int px;
        int py;
        int pulse_size;
        SDL_Rect panic_rect;

        maze_get_runner_panic_offset(&panic_x, &panic_y);

        px = (mod.runner.fx >> SAVER_FRAME_SHF) + panic_x;
        py = (mod.runner.fy >> SAVER_FRAME_SHF) + panic_y;

        pulse_size = (mod.cell_size * 3) / 4;
        if (pulse_size < 7) pulse_size = 7;

        panic_rect.x = px - pulse_size / 2;
        panic_rect.y = py - pulse_size / 2;
        panic_rect.w = pulse_size;
        panic_rect.h = pulse_size;

        SDL_SetRenderDrawColor(renderer, MAZE_RUNNER_R, MAZE_RUNNER_G, MAZE_RUNNER_B, 58);
        SDL_RenderDrawRect(renderer, &panic_rect);

        maze_render_actor_offset(renderer, &mod.runner, MAZE_RUNNER_R, MAZE_RUNNER_G, MAZE_RUNNER_B, panic_x, panic_y);
    } else {
        maze_render_actor(renderer, &mod.runner, MAZE_RUNNER_R, MAZE_RUNNER_G, MAZE_RUNNER_B);
    }

    maze_render_actor(renderer, &mod.hunter, MAZE_HUNTER_R, MAZE_HUNTER_G, MAZE_HUNTER_B);

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
    free(mod.runner_stack);
    free(mod.hunter_stack);
    free(mod.queue);
    free(mod.parent);
    free(mod.seen);

    mod.cell = NULL;
    mod.runner_stack = NULL;
    mod.hunter_stack = NULL;
    mod.queue = NULL;
    mod.parent = NULL;
    mod.seen = NULL;

    saver_shutdown_base(&mod.base);
}
