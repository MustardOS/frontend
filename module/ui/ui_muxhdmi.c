#include "ui_muxshare.h"
#include "ui_muxhdmi.h"

#define HDMI(NAME, UDATA)                                                                                              \
    lv_obj_t *ui_pnl_##NAME##_hdmi;                                                                                    \
    lv_obj_t *ui_lbl_##NAME##_hdmi;                                                                                    \
    lv_obj_t *ui_ico_##NAME##_hdmi;                                                                                    \
    lv_obj_t *ui_dro_##NAME##_hdmi;

HDMI_ELEMENTS
#undef HDMI

void init_muxhdmi(lv_obj_t *ui_pnl_content) {
#define HDMI(NAME, UDATA) CREATE_OPTION_ITEM(hdmi, NAME);
    HDMI_ELEMENTS
#undef HDMI
}
