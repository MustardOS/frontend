#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxrtc(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content);

extern lv_obj_t *ui_pnl_entry_rtc;
extern lv_obj_t *ui_txt_entry_rtc;

#define RTC(NAME, UDATA)                                                                                               \
    extern lv_obj_t *ui_pnl_##NAME##_rtc;                                                                              \
    extern lv_obj_t *ui_lbl_##NAME##_rtc;                                                                              \
    extern lv_obj_t *ui_ico_##NAME##_rtc;                                                                              \
    extern lv_obj_t *ui_val_##NAME##_rtc;

RTC_ELEMENTS
#undef RTC
