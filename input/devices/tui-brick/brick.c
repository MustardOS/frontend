#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "brick.h"
#include "brick_defs.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../drivers/sunxi-gpio/sunxi-gpio.h"

static struct brick_state brick_ctx;
static const int brick_axis_max = 32767;

static int file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

static inline void update_button_state(struct brick_state *st, size_t idx, int pressed) {
    if (idx >= 32) {
        return;
    }
    uint32_t mask = BRICK_BUTTON_BIT(idx);
    if (pressed) {
        st->state_buttons |= mask;
    } else {
        st->state_buttons &= ~mask;
    }
}

static enum brick_dpad2axis_mode is_dpad2axis_enable(const struct brick_state *st) {
    if (st->toggle_dpad2axis) {
        return brick_d2a_mode_global;
    }
    if ((st->state_buttons & st->alt_dpad2axis_bits) != 0) {
        return brick_d2a_mode_hold;
    }
    return brick_d2a_mode_disabled;
}

static void check_dpad_to_axis_settings(struct brick_state *st) {
    uint32_t alt_bits = 0;
    for (size_t i = 0; i < brick_hold_map_count; ++i) {
        const struct brick_hold_map *entry = &brick_hold_map[i];
        if (file_exists(entry->path)) {
            alt_bits |= entry->bit;
        }
    }

    int toggle_dpad2axis = file_exists(BRICK_DPAD_FLAG("input_dpad_to_joystick"));
    int toggle_dpad = !file_exists(BRICK_DPAD_FLAG("input_no_dpad"));

    st->alt_dpad2axis_bits = alt_bits;
    st->toggle_dpad2axis = toggle_dpad2axis;
    st->toggle_dpad = toggle_dpad;
}

void brick_refresh_dpad_flags(struct brick_state *st) {
    check_dpad_to_axis_settings(st);
}

static inline int axis_from_dpad(int negative, int positive) {
    if (negative == positive) {
        return 0;
    }
    return negative ? -brick_axis_max : brick_axis_max;
}

static void emit_synth_axis(struct brick_state *st, int x, int y) {
    if (st->last_axis_x == x && st->last_axis_y == y) {
        return;
    }
    st->last_axis_x = x;
    st->last_axis_y = y;
    gamepad_emit_abs(st->gp, ABS_X, x);
    gamepad_emit_abs(st->gp, ABS_Y, y);
    device_dirty_mark(&st->dirty);
}

static void clear_hat_output(struct brick_state *st) {
    st->hat.left = st->hat.right = st->hat.up = st->hat.down = 0;
    device_dirty_merge(&st->dirty, device_hat_emit(st->gp, &st->hat));
}

static void initialise_active_low(struct brick_state *st) {
    const char *env = getenv("BRICK_ACTIVE_LOW");
    if (env && strcmp(env, "0") == 0) {
        st->active_low = 0;
    } else {
        st->active_low = 1;
    }
}

static int initialise_pin(int pin) {
    return sunxi_gpio_set_cfgpin((uint32_t) pin, SUNXI_GPIO_INPUT);
}

static int poll_switch(struct brick_state *st) {
    int val = sunxi_gpio_input((uint32_t) brick_gpio_switch);
    if (val < 0) {
        return 0;
    }
    if (st->last_switch == val) {
        return 0;
    }
    st->last_switch = val;
    gamepad_emit_sw(st->gp, SW_TABLET_MODE, val);
    return 1;
}

int brick_initialise(
    void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx,
    struct axis_state *ry, const struct device_options *options
) {
    (void) lx;
    (void) ly;
    (void) rx;
    (void) ry;

    if (sunxi_gpio_initialise() < 0) {
        return -1;
    }

    struct brick_state *st = &brick_ctx;
    memset(st, 0, sizeof(*st));
    st->gp = gp;
    initialise_active_low(st);
    memcpy(st->buttons, brick_button_defs, sizeof(brick_button_defs));
    memcpy(st->hat_pins, brick_hat_pins, sizeof(brick_hat_pins));
    if (device_rumble_initialise(&st->rumble, rumble_a133_driver(), NULL, options->rumble_strength) < 0) {
        sunxi_gpio_close();
        return -1;
    }

    for (size_t i = 0; i < brick_button_count; ++i) {
        if (st->buttons[i].gpio < 0) {
            continue;
        }
        if (initialise_pin(st->buttons[i].gpio) < 0) {
            device_rumble_close(&st->rumble);
            sunxi_gpio_close();
            return -1;
        }
    }
    for (size_t i = 0; i < brick_hat_pin_count; ++i) {
        if (initialise_pin(st->hat_pins[i]) < 0) {
            device_rumble_close(&st->rumble);
            sunxi_gpio_close();
            return -1;
        }
    }
    if (initialise_pin(brick_gpio_switch) < 0) {
        device_rumble_close(&st->rumble);
        sunxi_gpio_close();
        return -1;
    }

    st->last_switch = -1;
    st->turbo_count = brick_turbo_cfg_count;
    turbo_initialise_bindings(st->turbo, st->turbo_count, brick_turbo_cfg);
    st->toggle_dpad = 1;
    *ctx = st;
    return 0;
}

int brick_poll(void *ctx) {
    struct brick_state *st = ctx;
    if (!device_rumble_poll(&st->rumble, st->gp)) {
        return 0;
    }
    device_dirty_reset(&st->dirty, poll_switch(st));

    for (size_t i = 0; i < brick_button_count; ++i) {
        if (st->buttons[i].gpio < 0) {
            continue;
        }
        int v = sunxi_gpio_input((uint32_t) st->buttons[i].gpio);
        if (v < 0) {
            continue;
        }
        int pressed = st->active_low ? (v == 0) : (v != 0);
        update_button_state(st, i, pressed);
        if (pressed != st->buttons[i].prev) {
            st->buttons[i].prev = pressed;
            int handled = turbo_note_physical(st->turbo, st->turbo_count, st->buttons[i].code, pressed != 0);
            if (!handled) {
                gamepad_emit_key(st->gp, st->buttons[i].code, pressed);
                device_dirty_mark(&st->dirty);
            }
        }
    }

    int dpad_vals[brick_hat_pin_count] = {0};
    int *hat_targets[brick_hat_pin_count] = {&st->hat.up, &st->hat.down, &st->hat.left, &st->hat.right};
    for (size_t i = 0; i < brick_hat_pin_count; ++i) {
        int value = sunxi_gpio_input((uint32_t) st->hat_pins[i]);
        if (value < 0) {
            continue;
        }
        int pressed = st->active_low ? (value == 0) : (value != 0);
        *hat_targets[i] = pressed;
        dpad_vals[i] = pressed;
    }

    enum brick_dpad2axis_mode mode = is_dpad2axis_enable(st);

    int axis_x = axis_from_dpad(dpad_vals[2], dpad_vals[3]);
    int axis_y = axis_from_dpad(dpad_vals[0], dpad_vals[1]);

    int allow_hat = st->toggle_dpad && mode == brick_d2a_mode_disabled;
    if (allow_hat) {
        device_dirty_merge(&st->dirty, device_hat_emit(st->gp, &st->hat));
    } else if (st->hat.x != 0 || st->hat.y != 0) {
        clear_hat_output(st);
    }

    if (mode != brick_d2a_mode_disabled) {
        emit_synth_axis(st, axis_x, axis_y);
    } else if (st->d2a_last_mode != brick_d2a_mode_disabled) {
        emit_synth_axis(st, 0, 0);
    }

    st->d2a_last_mode = mode;
    turbo_process_frame(st->turbo, st->turbo_count, st->gp, &st->dirty);
    device_dirty_flush(&st->dirty, st->gp);
    return 1;
}

void brick_close(void *ctx) {
    struct brick_state *st = ctx;
    if (st) {
        device_rumble_close(&st->rumble);
    }
    sunxi_gpio_close();
}

static int brick_probe(void) {

    FILE *compatible = fopen("/proc/device-tree/compatible", "rb");
    if (!compatible) {
        return 0;
    }
    char buffer[256];
    size_t length = fread(buffer, 1, sizeof(buffer), compatible);
    fclose(compatible);
    return memmem(buffer, length, "allwinner,a133", sizeof("allwinner,a133") - 1) != NULL;
}

static void brick_refresh(void *ctx) {
    struct brick_state *st = ctx;
    if (!st) {
        return;
    }
    brick_refresh_dpad_flags(st);
    turbo_refresh_flags(st->turbo, st->turbo_count);
}

static void brick_set_rumble_strength(void *ctx, unsigned int strength_percent) {
    struct brick_state *st = ctx;
    if (st) device_rumble_set_strength(&st->rumble, strength_percent);
}

const struct device_backend tui_brick_profile = {
    .id = "tui-brick",
    .name = "TRIMUI BRICK",
    .probe_priority = 0,
    .probe = brick_probe,
    .gamepad = &brick_gamepad_desc,
    .ops =
        {
            .name = "TRIMUI BRICK",
            .initialise = brick_initialise,
            .poll = brick_poll,
            .refresh = brick_refresh,
            .set_rumble_strength = brick_set_rumble_strength,
            .close = brick_close,
        },
    .poll_interval_us = 2000,
};
