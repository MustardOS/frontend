#define _DEFAULT_SOURCE

#include "smart_pro.h"
#include "smart_pro_defs.h"

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../drivers/sunxi-gpio/sunxi-gpio.h"
#include "../../drivers/gpio/gpio.h"

static struct smart_pro_device smart_pro_ctx;
static const struct device_axis_cfg SP_AXIS_LEFT_CFG = {
    .abs_code_x = ABS_X, .abs_code_y = ABS_Y, .invert_x = 1, .invert_y = 1
};
static const struct device_axis_cfg SP_AXIS_RIGHT_CFG = {
    .abs_code_x = ABS_Z, .abs_code_y = ABS_RZ, .invert_x = 1, .invert_y = 1
};

static int smart_pro_gpio_initialise(void) {
    const int output_pins[] = {SP_GPIO_OUT1, SP_GPIO_OUT2};
    for (size_t i = 0; i < sizeof(output_pins) / sizeof(output_pins[0]); ++i) {
        if (gpio_export(output_pins[i]) < 0 || gpio_set_direction(output_pins[i], 1) < 0
            || gpio_write(output_pins[i], 1) < 0) {
            return -1;
        }
    }
    if (sunxi_gpio_initialise() < 0) {
        return -1;
    }
    if (sunxi_gpio_set_cfgpin((uint32_t) SP_GPIO_INPUT, SUNXI_GPIO_INPUT) < 0) {
        sunxi_gpio_close();
        return -1;
    }

    return 0;
}

static int poll_switch(struct gamepad *gp, int pin, int *last_val) {
    int val = sunxi_gpio_input((uint32_t) pin);
    if (val < 0) {
        return 0;
    }
    if (*last_val == val) {
        return 0;
    }
    *last_val = val;
    gamepad_emit_sw(gp, SW_TABLET_MODE, val);
    return 1;
}

static void emit_buttons_from_map(
    struct gamepad *gp, uint8_t btn_raw, uint8_t diff, const struct sp_button_map_entry *map, size_t map_len,
    struct device_dirty_state *dirty, struct turbo_binding *turbo, size_t turbo_count
) {
    for (size_t i = 0; i < map_len; ++i) {
        uint8_t mask = map[i].mask;
        if (diff & mask) {
            int pressed = (btn_raw & mask) ? 1 : 0;
            int handled = turbo_note_physical(turbo, turbo_count, map[i].code, pressed != 0);
            if (!handled) {
                gamepad_emit_key(gp, map[i].code, pressed);
                device_dirty_mark(dirty);
            }
        }
    }
}

static void process_left_frame_bytes(
    struct smart_pro_left_ctx *ctx, const uint8_t frame_bytes[8], struct device_dirty_state *dirty,
    struct turbo_binding *turbo, size_t turbo_count
) {
    uint8_t btn_raw = frame_bytes[2];
    uint16_t raw_x = ((uint16_t) frame_bytes[3] << 8) | frame_bytes[4];
    uint16_t raw_y = ((uint16_t) frame_bytes[5] << 8) | frame_bytes[6];

    uint8_t diff = btn_raw ^ ctx->c.prev_buttons;
    int axes_changed = !ctx->c.have_prev_raw || raw_x != ctx->c.prev_raw_x || raw_y != ctx->c.prev_raw_y;

    if (!axes_changed && diff == 0) {
        return;
    }

    if (axes_changed) {
        device_axes_process_pair(
            ctx->c.gp, ctx->c.ax, ctx->c.ay, raw_x, raw_y, &SP_AXIS_LEFT_CFG, &ctx->c.last_x, &ctx->c.last_y, dirty
        );
    }

    if (diff) {
        device_hat_apply_masks(
            &ctx->hat, btn_raw, diff, SP_BTN_DPAD_UP_MASK, SP_BTN_DPAD_DOWN_MASK, SP_BTN_DPAD_LEFT_MASK,
            SP_BTN_DPAD_RIGHT_MASK
        );
        emit_buttons_from_map(
            ctx->c.gp, btn_raw, diff, SP_LEFT_BUTTON_MAP, SP_LEFT_BUTTON_MAP_COUNT, dirty, turbo, turbo_count
        );

        if (diff & SP_DPAD_MASK) {
            device_dirty_merge(dirty, device_hat_emit(ctx->c.gp, &ctx->hat));
        }
    }

    ctx->c.prev_buttons = btn_raw;
    ctx->c.prev_raw_x = raw_x;
    ctx->c.prev_raw_y = raw_y;
    ctx->c.have_prev_raw = 1;
}

static void process_right_frame_bytes(
    struct smart_pro_right_ctx *ctx, const uint8_t frame_bytes[8], struct device_dirty_state *dirty,
    struct turbo_binding *turbo, size_t turbo_count
) {
    uint8_t btn_raw = frame_bytes[2];
    uint16_t raw_x = ((uint16_t) frame_bytes[3] << 8) | frame_bytes[4];
    uint16_t raw_y = ((uint16_t) frame_bytes[5] << 8) | frame_bytes[6];

    uint8_t diff = btn_raw ^ ctx->c.prev_buttons;
    int axes_changed = !ctx->c.have_prev_raw || raw_x != ctx->c.prev_raw_x || raw_y != ctx->c.prev_raw_y;

    if (!axes_changed && diff == 0) {
        return;
    }

    if (axes_changed) {
        device_axes_process_pair(
            ctx->c.gp, ctx->c.ax, ctx->c.ay, raw_x, raw_y, &SP_AXIS_RIGHT_CFG, &ctx->c.last_x, &ctx->c.last_y, dirty
        );
    }

    if (diff) {
        emit_buttons_from_map(
            ctx->c.gp, btn_raw, diff, SP_RIGHT_BUTTON_MAP, SP_RIGHT_BUTTON_MAP_COUNT, dirty, turbo, turbo_count
        );
    }

    ctx->c.prev_buttons = btn_raw;
    ctx->c.prev_raw_x = raw_x;
    ctx->c.prev_raw_y = raw_y;
    ctx->c.have_prev_raw = 1;
}

int smart_pro_initialise(
    void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx,
    struct axis_state *ry, const struct device_options *options
) {
    if (smart_pro_gpio_initialise() < 0) {
        return -1;
    }

    int fd_left = serial_open_nonblocking("/dev/ttyS4");
    if (fd_left < 0) {
        sunxi_gpio_close();
        return -1;
    }
    int fd_right = serial_open_nonblocking("/dev/ttyS3");
    if (fd_right < 0) {
        close(fd_left);
        sunxi_gpio_close();
        return -1;
    }
    if (serial_config(fd_left) < 0 || serial_config(fd_right) < 0) {
        close(fd_left);
        close(fd_right);
        sunxi_gpio_close();
        return -1;
    }

    struct smart_pro_device *dev = &smart_pro_ctx;
    memset(dev, 0, sizeof(*dev));

    dev->left.c.fd = fd_left;
    dev->left.c.gp = gp;
    dev->left.c.ax = lx;
    dev->left.c.ay = ly;
    dev->left.last_switch = -1;

    dev->right.c.fd = fd_right;
    dev->right.c.gp = gp;
    dev->right.c.ax = rx;
    dev->right.c.ay = ry;

    rb_initialise(&dev->left.c.rb);
    rb_initialise(&dev->right.c.rb);
    dev->turbo_count = SP_TURBO_CFG_COUNT;
    turbo_initialise_bindings(dev->turbo, dev->turbo_count, SP_TURBO_CFG);
    dev->pfds[0] = (struct pollfd) {.fd = fd_left, .events = POLLIN};
    dev->pfds[1] = (struct pollfd) {.fd = fd_right, .events = POLLIN};

    if (device_rumble_initialise(&dev->rumble, rumble_a133_driver(), NULL, options->rumble_strength) < 0) {
        close(fd_left);
        close(fd_right);
        sunxi_gpio_close();
        return -1;
    }

    *ctx = dev;
    return 0;
}

int smart_pro_poll(void *ctx) {
    struct smart_pro_device *dev = ctx;
    uint8_t buf[128];
    uint8_t frame_bytes[8];
    device_dirty_reset(&dev->dirty, poll_switch(dev->left.c.gp, SP_GPIO_INPUT, &dev->left.last_switch));

    if (!device_rumble_poll(&dev->rumble, dev->left.c.gp)) {
        return 0;
    }

    dev->pfds[0].revents = 0;
    dev->pfds[1].revents = 0;
    int pr = poll(dev->pfds, 2, 0);
    if (pr < 0) {
        if (errno == EINTR) {
            return 1;
        }
        perror("poll smart_pro");
        return 0;
    }

    if (dev->pfds[0].revents & POLLIN) {
        ssize_t r = serial_poll_read(dev->left.c.fd, buf, sizeof(buf));
        if (r < 0) {
            perror("read left");
            return 0;
        }
        if (r > 0) {
            rb_push(&dev->left.c.rb, buf, (size_t) r);
            while (rb_try_extract_frame_variantA(&dev->left.c.rb, frame_bytes)) {
                process_left_frame_bytes(&dev->left, frame_bytes, &dev->dirty, dev->turbo, dev->turbo_count);
            }
        }
    }

    if (dev->pfds[1].revents & POLLIN) {
        ssize_t r = serial_poll_read(dev->right.c.fd, buf, sizeof(buf));
        if (r < 0) {
            perror("read right");
            return 0;
        }
        if (r > 0) {
            rb_push(&dev->right.c.rb, buf, (size_t) r);
            while (rb_try_extract_frame_variantA(&dev->right.c.rb, frame_bytes)) {
                process_right_frame_bytes(&dev->right, frame_bytes, &dev->dirty, dev->turbo, dev->turbo_count);
            }
        }
    }

    turbo_process_frame(dev->turbo, dev->turbo_count, dev->left.c.gp, &dev->dirty);
    device_dirty_flush(&dev->dirty, dev->left.c.gp);
    return 1;
}

void smart_pro_close(void *ctx) {
    struct smart_pro_device *dev = ctx;
    if (!dev) return;
    close(dev->left.c.fd);
    close(dev->right.c.fd);
    device_rumble_close(&dev->rumble);
    sunxi_gpio_close();
    memset(dev, 0, sizeof(*dev));
}

static int smart_pro_probe(void) {
    return access("/dev/ttyS4", F_OK) == 0;
}

static void smart_pro_refresh(void *ctx) {
    struct smart_pro_device *dev = ctx;
    if (dev) {
        turbo_refresh_flags(dev->turbo, dev->turbo_count);
    }
}

static void smart_pro_set_rumble_strength(void *ctx, unsigned int strength_percent) {
    struct smart_pro_device *dev = ctx;
    if (dev) device_rumble_set_strength(&dev->rumble, strength_percent);
}

const struct device_backend TUI_SPOON_PROFILE = {
    .id = "tui-spoon",
    .name = "TRIMUI SMART PRO",
    .probe_priority = 200,
    .probe = smart_pro_probe,
    .gamepad = &SMART_PRO_GAMEPAD_DESC,
    .has_analogue_calibration = 1,
    .ops =
        {
            .name = "TRIMUI SMART PRO",
            .initialise = smart_pro_initialise,
            .poll = smart_pro_poll,
            .refresh = smart_pro_refresh,
            .set_rumble_strength = smart_pro_set_rumble_strength,
            .close = smart_pro_close,
        },
    .poll_interval_us = 1000,
};
