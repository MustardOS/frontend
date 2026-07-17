#pragma once

int texture_filter_scale_factor(int mode);

int texture_filter_is_cpu_scaled(int mode);

int texture_filter_wants_linear_sample(int mode);

void texture_filter_apply(
    int mode, const void *src, void *dst, int width, int height, int src_pitch_px, int dst_pitch_px, unsigned bpp,
    int pixel_format
);
