#pragma once

#include <stdint.h>

#include "smart_pro_structs.h"
#include "../device.h"

extern const struct device_backend tui_spoon_profile;

struct gamepad;
struct axis_state;

int smart_pro_initialise(
    void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx,
    struct axis_state *ry, const struct device_options *options
);

int smart_pro_poll(void *ctx);

void smart_pro_close(void *ctx);
