#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxrtc(lv_obj_t *ui_pnlContent);

#define RTC(NAME, UDATA)                      \
    extern lv_obj_t *ui_pnl##NAME##_rtc;      \
    extern lv_obj_t *ui_lbl##NAME##_rtc;      \
    extern lv_obj_t *ui_ico##NAME##_rtc;      \
    extern lv_obj_t *ui_lbl##NAME##Value_rtc;

RTC_ELEMENTS
#undef RTC
