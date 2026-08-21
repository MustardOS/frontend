#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxlink(lv_obj_t *ui_pnl_content);

#define LINK(NAME, UDATA)                                                                                              \
    extern lv_obj_t *ui_pnl_##NAME##_link;                                                                             \
    extern lv_obj_t *ui_lbl_##NAME##_link;                                                                             \
    extern lv_obj_t *ui_ico_##NAME##_link;                                                                             \
    extern lv_obj_t *ui_val_##NAME##_link;

LINK_ELEMENTS
#undef LINK
