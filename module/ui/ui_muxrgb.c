#include "ui_muxshare.h"
#include "ui_muxrgb.h"

#define RGB(NAME, UDATA)                                                                                               \
    lv_obj_t *ui_pnl_##NAME##_rgb;                                                                                     \
    lv_obj_t *ui_lbl_##NAME##_rgb;                                                                                     \
    lv_obj_t *ui_ico_##NAME##_rgb;                                                                                     \
    lv_obj_t *ui_dro_##NAME##_rgb;
RGB_ELEMENTS
#undef RGB

void init_muxrgb(lv_obj_t *ui_pnl_content) {
#define RGB(NAME, UDATA) CREATE_OPTION_ITEM(rgb, NAME);
    RGB_ELEMENTS
#undef RGB
}
