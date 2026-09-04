#pragma once

#include "../../common/uinput.h"
#include "brick_pro_structs.h"
#include "../device.h"

extern const struct device_backend tui_brick_pro_profile;

struct axis_state;

int brick_pro_initialise(
    void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx,
    struct axis_state *ry, const struct device_options *options
);

int brick_pro_poll(void *ctx);

void brick_pro_close(void *ctx);
