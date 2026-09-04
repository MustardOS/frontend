#pragma once

#include "../../common/uinput.h"
#include "brick_structs.h"
#include "../device.h"

extern const struct device_backend tui_brick_profile;

struct axis_state;

int brick_initialise(
    void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx,
    struct axis_state *ry, const struct device_options *options
);

int brick_poll(void *ctx);

void brick_close(void *ctx);

struct brick_state;
void brick_refresh_dpad_flags(struct brick_state *st);
