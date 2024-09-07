#include "ui.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

const LV_ATTRIBUTE_MEM_ALIGN uint8_t ui_img_nothing_png_data[] = {
        0x00, 0x00, 0x00, 0x00,
};
const lv_img_dsc_t ui_img_nothing_png = {
        .header.always_zero = 0,
        .header.w = 1,
        .header.h = 1,
        .data_size = sizeof(ui_img_nothing_png_data),
        .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
        .data = ui_img_nothing_png_data
};
