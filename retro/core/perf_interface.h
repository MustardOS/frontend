#pragma once

#include <stdbool.h>
#include "libretro.h"

bool perf_interface_get(struct retro_perf_callback *callback);

void perf_interface_reset(void);
