#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../core/libretro.h"

void interframe_blend_detected(
    void *current, const void *previous, const void *two_back, const void *three_back, const void *four_back,
    unsigned width, unsigned height, size_t pitch, enum retro_pixel_format format, uint8_t *persistence,
    size_t persistence_pitch
);

void interframe_blend_shutdown(void);
