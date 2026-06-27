#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxkiosk(lv_obj_t *ui_pnl_content);

#define KIOSK(NAME, UDATA)                                                                                             \
    extern lv_obj_t *ui_pnl_##NAME##_kiosk;                                                                            \
    extern lv_obj_t *ui_lbl_##NAME##_kiosk;                                                                            \
    extern lv_obj_t *ui_ico_##NAME##_kiosk;                                                                            \
    extern lv_obj_t *ui_dro_##NAME##_kiosk;

KIOSK_ELEMENTS
#undef KIOSK
