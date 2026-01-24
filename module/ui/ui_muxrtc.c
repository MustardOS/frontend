#include "ui_muxshare.h"
#include "ui_muxrtc.h"

#define RTC(NAME, ENUM, UDATA)         \
    lv_obj_t *ui_pnl##NAME##_rtc;      \
    lv_obj_t *ui_lbl##NAME##_rtc;      \
    lv_obj_t *ui_ico##NAME##_rtc;      \
    lv_obj_t *ui_lbl##NAME##Value_rtc;

RTC_ELEMENTS
#undef RTC

void init_muxrtc(lv_obj_t *ui_pnlContent) {
#define RTC(NAME, ENUM, UDATA) CREATE_VALUE_ITEM(rtc, NAME);
    RTC_ELEMENTS
#undef RTC
}
