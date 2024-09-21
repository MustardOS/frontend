#include "nothing.h"

const lv_img_dsc_t ui_image_Nothing = {
        .header.always_zero = 0,
        .header.w = 1,
        .header.h = 1,
        .data_size = 0,
        .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
        .data = 0x00
};
