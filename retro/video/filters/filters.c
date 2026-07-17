#include "filters.h"
#include "scale2x.h"
#include "super_eagle.h"
#include "../../core/muxretro.h"
#include "../../settings/settings.h"

int texture_filter_scale_factor(const int mode) {
    if (mode == texture_filter_scale2_x || mode == texture_filter_scale2_x_smooth
        || mode == texture_filter_super_eagle) {
        return 2;
    }
    if (mode == texture_filter_scale3_x) return 3;
    return 1;
}

int texture_filter_is_cpu_scaled(const int mode) {
    return texture_filter_scale_factor(mode) > 1;
}

int texture_filter_wants_linear_sample(const int mode) {
    return mode == texture_filter_smooth || mode == texture_filter_scale2_x_smooth;
}

void texture_filter_apply(
    const int mode, const void *src, void *dst, const int width, const int height, const int src_pitch_px,
    const int dst_pitch_px, const unsigned bpp, const int pixel_format
) {
    if (mode == texture_filter_super_eagle) {
        if (bpp == 4) {
            super_eagle_32(src, dst, width, height, src_pitch_px, dst_pitch_px);
        } else if (pixel_format == RETRO_PIXEL_FORMAT_RGB565) {
            super_eagle_16_565(src, dst, width, height, src_pitch_px, dst_pitch_px);
        } else {
            super_eagle_16_1555(src, dst, width, height, src_pitch_px, dst_pitch_px);
        }
        return;
    }

    const int is_scale3_x = mode == texture_filter_scale3_x;

    if (bpp == 4) {
        if (is_scale3_x) {
            scale3_x_32(src, dst, width, height, src_pitch_px, dst_pitch_px);
        } else {
            scale2_x_32(src, dst, width, height, src_pitch_px, dst_pitch_px);
        }
    } else {
        if (is_scale3_x) {
            scale3_x_16(src, dst, width, height, src_pitch_px, dst_pitch_px);
        } else {
            scale2_x_16(src, dst, width, height, src_pitch_px, dst_pitch_px);
        }
    }
}
