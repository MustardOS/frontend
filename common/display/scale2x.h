#pragma once

#include <stdint.h>

void scale2_x_16(const uint16_t *src, uint16_t *dst, int width, int height, int src_pitch_px, int dst_pitch_px);

void scale2_x_32(const uint32_t *src, uint32_t *dst, int width, int height, int src_pitch_px, int dst_pitch_px);

void scale3_x_16(const uint16_t *src, uint16_t *dst, int width, int height, int src_pitch_px, int dst_pitch_px);

void scale3_x_32(const uint32_t *src, uint32_t *dst, int width, int height, int src_pitch_px, int dst_pitch_px);
