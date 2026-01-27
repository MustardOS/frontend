#pragma once

#include "../common/common.h"

#define ROTATE_DETECT OVERLAY_RUNNER "rotate"

enum {
    ROTATE_0 = 0,
    ROTATE_90 = 90,
    ROTATE_180 = 180,
    ROTATE_270 = 270
};

int rotate_normalise(int deg);

int rotate_read(void);

int rotate_read_cached(void);

void rotate_dims(int w, int h, int rot, int *out_w, int *out_h);

double rotate_angle(int rot);
