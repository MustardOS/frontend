#include "ui_muxshare.h"
#include "ui_muxhdmi.h"

#define HDMI(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_hdmi; \
    lv_obj_t *ui_lbl##NAME##_hdmi; \
    lv_obj_t *ui_ico##NAME##_hdmi; \
    lv_obj_t *ui_dro##NAME##_hdmi;

HDMI_ELEMENTS
#undef HDMI

void init_muxhdmi(lv_obj_t *ui_pnlContent) {
#define HDMI(NAME, UDATA) CREATE_OPTION_ITEM(hdmi, NAME);
    HDMI_ELEMENTS
#undef HDMI
}
