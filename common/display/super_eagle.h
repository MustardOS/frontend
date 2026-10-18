#pragma once

#include <stdint.h>

void super_eagle_16_565(const uint16_t *src, uint16_t *dst, int width, int height, int src_pitch_px, int dst_pitch_px);

void super_eagle_16_1555(const uint16_t *src, uint16_t *dst, int width, int height, int src_pitch_px, int dst_pitch_px);

void super_eagle_32(const uint32_t *src, uint32_t *dst, int width, int height, int src_pitch_px, int dst_pitch_px);
