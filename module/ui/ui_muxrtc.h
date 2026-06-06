#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxrtc(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_pnlEntry_rtc;
extern lv_obj_t *ui_txtEntry_rtc;

#define RTC(NAME, ENUM, UDATA)                \
    extern lv_obj_t *ui_pnl##NAME##_rtc;      \
    extern lv_obj_t *ui_lbl##NAME##_rtc;      \
    extern lv_obj_t *ui_ico##NAME##_rtc;      \
    extern lv_obj_t *ui_lbl##NAME##Value_rtc;

RTC_ELEMENTS
#undef RTC
