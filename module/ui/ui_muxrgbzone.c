#include "ui_muxshare.h"
#include "ui_muxrgbzone.h"

#define RGBZONE(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_rgbzone;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_rgbzone;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_rgbzone;                                                                                 \
    lv_obj_t *ui_dro_##NAME##_rgbzone;
RGBZONE_ELEMENTS
#undef RGBZONE

void init_muxrgbzone(lv_obj_t *ui_pnl_content) {
#define RGBZONE(NAME, UDATA) CREATE_OPTION_ITEM(rgbzone, NAME);
    RGBZONE_ELEMENTS
#undef RGBZONE
}
