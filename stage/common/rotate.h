#pragma once

#include "../common/common.h"

#define ROTATE_DETECT OVERLAY_RUNNER "rotate"

enum { rotate_0 = 0, rotate_90 = 90, rotate_180 = 180, rotate_270 = 270 };

int rotate_normalise(int deg);

int rotate_read(void);

int rotate_read_cached(void);

void rotate_dims(int w, int h, int rot, int *out_w, int *out_h);

double rotate_angle(int rot);
