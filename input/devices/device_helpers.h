#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../common/calibration.h"
#include "../common/uinput.h"

struct device_hat_state {
    int x;
    int y;
    int left;
    int right;
    int up;
    int down;
};

struct device_dirty_state {
    int dirty;
};

static inline int device_hat_emit(struct gamepad *gp, struct device_hat_state *hat) {
    const int horiz[2] = {hat->left, hat->right};
    const int vert[2] = {hat->up, hat->down};
    int new_x = (horiz[0] == horiz[1]) ? 0 : (horiz[0] ? -1 : 1);
    int new_y = (vert[0] == vert[1]) ? 0 : (vert[0] ? -1 : 1);
    int changed = 0;

    if (new_x != hat->x) {
        hat->x = new_x;
        gamepad_emit_abs(gp, ABS_HAT0X, hat->x);
        changed = 1;
    }
    if (new_y != hat->y) {
        hat->y = new_y;
        gamepad_emit_abs(gp, ABS_HAT0Y, hat->y);
        changed = 1;
    }
    return changed;
}

static inline void device_hat_apply_masks(
    struct device_hat_state *hat, uint32_t btn_raw, uint32_t diff, uint32_t up_mask, uint32_t down_mask,
    uint32_t left_mask, uint32_t right_mask
) {
    const struct {
        uint32_t mask;
        int *target;
    } map[] = {
        {up_mask, &hat->up},
        {down_mask, &hat->down},
        {left_mask, &hat->left},
        {right_mask, &hat->right},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i) {
        if (diff & map[i].mask) {
            *map[i].target = (btn_raw & map[i].mask) ? 1 : 0;
        }
    }
}

struct device_axis_cfg {
    int abs_code_x;
    int abs_code_y;
    int invert_x;
    int invert_y;
};

static inline void device_dirty_mark(struct device_dirty_state *state) {
    state->dirty = 1;
}

static inline void device_dirty_merge(struct device_dirty_state *state, int changed) {
    if (changed) {
        state->dirty = 1;
    }
}

static inline void device_dirty_reset(struct device_dirty_state *state, int initial_dirty) {
    state->dirty = initial_dirty;
}

static inline void device_dirty_flush(struct device_dirty_state *state, struct gamepad *gp) {
    if (state->dirty) {
        gamepad_sync(gp);
        state->dirty = 0;
    }
}

static inline void device_axes_process_pair(
    struct gamepad *gp, struct axis_state *ax, struct axis_state *ay, uint16_t raw_x, uint16_t raw_y,
    const struct device_axis_cfg *cfg, int *last_x, int *last_y, struct device_dirty_state *dirty
) {
    cal_update2(ax, ay, raw_x, raw_y);
    int x = cal_apply(ax, raw_x);
    int y = cal_apply(ay, raw_y);
    if (cfg->invert_x) x = -x;
    if (cfg->invert_y) y = -y;

    if (x != *last_x) {
        *last_x = x;
        gamepad_emit_abs(gp, cfg->abs_code_x, x);
        device_dirty_mark(dirty);
    }
    if (y != *last_y) {
        *last_y = y;
        gamepad_emit_abs(gp, cfg->abs_code_y, y);
        device_dirty_mark(dirty);
    }
}
