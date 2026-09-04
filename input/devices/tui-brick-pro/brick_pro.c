#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "brick_pro.h"
#include "brick_pro_defs.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../drivers/sunxi-gpio/sunxi-gpio.h"
#include "../../drivers/i2c/i2c.h"

static struct brick_pro_state brick_pro_ctx;

static const struct device_axis_cfg brick_pro_axis_left_cfg = {
    .abs_code_x = ABS_X, .abs_code_y = ABS_Y, .invert_x = 1, .invert_y = 1
};
static const struct device_axis_cfg brick_pro_axis_right_cfg = {
    .abs_code_x = ABS_RX, .abs_code_y = ABS_RY, .invert_x = 1, .invert_y = 1
};

static void initialise_active_low(struct brick_pro_state *st) {
    const char *env = getenv("BRICK_PRO_ACTIVE_LOW");
    if (env && strcmp(env, "0") == 0) {
        st->active_low = 0;
    } else {
        st->active_low = 1;
    }
}

static int initialise_pin(int pin) {
    return sunxi_gpio_set_cfgpin((uint32_t) pin, SUNXI_GPIO_INPUT);
}

static int poll_switch(struct brick_pro_state *st) {
    int val = sunxi_gpio_input((uint32_t) brick_pro_gpio_switch);
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

static int read_stick(int fd, uint8_t addr7, int *out_x, int *out_y) {
    uint8_t data[4];
    if (i2c_read_reg(fd, addr7, BRICK_PRO_I2C_REG_POLL, data, sizeof(data)) < 0) {
        return 0;
    }
    int x = (data[0] << 8) | data[1];
    int y = (data[2] << 8) | data[3];
    if (x > BRICK_PRO_AXIS_RAW_MAX) x = BRICK_PRO_AXIS_RAW_MAX;
    if (y > BRICK_PRO_AXIS_RAW_MAX) y = BRICK_PRO_AXIS_RAW_MAX;
    *out_x = x;
    *out_y = y;
    return 1;
}

int brick_pro_initialise(
    void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx,
    struct axis_state *ry, const struct device_options *options
) {

    if (sunxi_gpio_initialise() < 0) {
        return -1;
    }

    struct brick_pro_state *st = &brick_pro_ctx;
    memset(st, 0, sizeof(*st));
    st->gp = gp;
    initialise_active_low(st);
    memcpy(st->buttons, brick_pro_button_defs, sizeof(brick_pro_button_defs));
    memcpy(st->hat_pins, brick_pro_hat_pins, sizeof(brick_pro_hat_pins));
    if (device_rumble_initialise(&st->rumble, rumble_a133_driver(), NULL, options->rumble_strength) < 0) {
        sunxi_gpio_close();
        return -1;
    }

    for (size_t i = 0; i < brick_pro_button_count; ++i) {
        if (initialise_pin(st->buttons[i].gpio) < 0) {
            device_rumble_close(&st->rumble);
            sunxi_gpio_close();
            return -1;
        }
    }
    for (size_t i = 0; i < brick_pro_hat_pin_count; ++i) {
        if (initialise_pin(st->hat_pins[i]) < 0) {
            device_rumble_close(&st->rumble);
            sunxi_gpio_close();
            return -1;
        }
    }
    if (initialise_pin(brick_pro_gpio_switch) < 0) {
        device_rumble_close(&st->rumble);
        sunxi_gpio_close();
        return -1;
    }

    st->i2c_fd = i2c_open(BRICK_PRO_I2C_DEV);
    if (st->i2c_fd < 0) {
        device_rumble_close(&st->rumble);
        sunxi_gpio_close();
        return -1;
    }

    st->last_switch = -1;
    st->hat = (struct device_hat_state) {.x = 0, .y = 0, .left = 0, .right = 0, .up = 0, .down = 0};
    st->turbo_count = brick_pro_turbo_cfg_count;
    turbo_initialise_bindings(st->turbo, st->turbo_count, brick_pro_turbo_cfg);

    st->ax_lx = lx;
    st->ax_ly = ly;
    st->ax_rx = rx;
    st->ax_ry = ry;
    st->last_lx = 0;
    st->last_ly = 0;
    st->last_rx = 0;
    st->last_ry = 0;

    *ctx = st;
    return 0;
}

int brick_pro_poll(void *ctx) {
    struct brick_pro_state *st = ctx;
    if (!device_rumble_poll(&st->rumble, st->gp)) {
        return 0;
    }
    device_dirty_reset(&st->dirty, poll_switch(st));

    for (size_t i = 0; i < brick_pro_button_count; ++i) {
        int v = sunxi_gpio_input((uint32_t) st->buttons[i].gpio);
        if (v < 0) {
            continue;
        }
        int pressed = st->active_low ? (v == 0) : (v != 0);
        if (pressed != st->buttons[i].prev) {
            st->buttons[i].prev = pressed;
            int handled = turbo_note_physical(st->turbo, st->turbo_count, st->buttons[i].code, pressed != 0);
            if (!handled) {
                gamepad_emit_key(st->gp, st->buttons[i].code, pressed);
                device_dirty_mark(&st->dirty);
            }
        }
    }

    int *hat_targets[brick_pro_hat_pin_count] = {&st->hat.up, &st->hat.down, &st->hat.left, &st->hat.right};
    for (size_t i = 0; i < brick_pro_hat_pin_count; ++i) {
        int value = sunxi_gpio_input((uint32_t) st->hat_pins[i]);
        if (value < 0) {
            continue;
        }
        int pressed = st->active_low ? (value == 0) : (value != 0);
        *hat_targets[i] = pressed;
    }
    device_dirty_merge(&st->dirty, device_hat_emit(st->gp, &st->hat));

    int lx, ly, rx, ry;
    if (read_stick(st->i2c_fd, BRICK_PRO_I2C_ADDR_L, &lx, &ly)) {
        device_axes_process_pair(
            st->gp, st->ax_lx, st->ax_ly, (uint16_t) lx, (uint16_t) ly, &brick_pro_axis_left_cfg, &st->last_lx,
            &st->last_ly, &st->dirty
        );
    }
    if (read_stick(st->i2c_fd, BRICK_PRO_I2C_ADDR_R, &rx, &ry)) {
        device_axes_process_pair(
            st->gp, st->ax_rx, st->ax_ry, (uint16_t) rx, (uint16_t) ry, &brick_pro_axis_right_cfg, &st->last_rx,
            &st->last_ry, &st->dirty
        );
    }

    turbo_process_frame(st->turbo, st->turbo_count, st->gp, &st->dirty);
    device_dirty_flush(&st->dirty, st->gp);
    return 1;
}

void brick_pro_close(void *ctx) {
    struct brick_pro_state *st = ctx;
    if (st) {
        device_rumble_close(&st->rumble);
        i2c_close(st->i2c_fd);
    }
    sunxi_gpio_close();
}

static int brick_pro_probe(void) {
    return access("/dev/i2c-3", F_OK) == 0;
}

static void brick_pro_refresh(void *ctx) {
    struct brick_pro_state *st = ctx;
    if (st) {
        turbo_refresh_flags(st->turbo, st->turbo_count);
    }
}

static void brick_pro_set_rumble_strength(void *ctx, unsigned int strength_percent) {
    struct brick_pro_state *st = ctx;
    if (st) device_rumble_set_strength(&st->rumble, strength_percent);
}

const struct device_backend tui_brick_pro_profile = {
    .id = "tui-brick-pro",
    .name = "TRIMUI BRICK PRO",
    .probe_priority = 100,
    .probe = brick_pro_probe,
    .gamepad = &brick_pro_gamepad_desc,
    .has_analogue_calibration = 1,
    .ops =
        {
            .name = "TRIMUI BRICK PRO",
            .initialise = brick_pro_initialise,
            .poll = brick_pro_poll,
            .refresh = brick_pro_refresh,
            .set_rumble_strength = brick_pro_set_rumble_strength,
            .close = brick_pro_close,
        },
    .poll_interval_us = 4000,
};
