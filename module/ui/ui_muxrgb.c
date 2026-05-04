#include "ui_muxshare.h"
#include "ui_muxrgb.h"

#define RGB(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_rgb; \
    lv_obj_t *ui_lbl##NAME##_rgb; \
    lv_obj_t *ui_ico##NAME##_rgb; \
    lv_obj_t *ui_dro##NAME##_rgb;
RGB_ELEMENTS
#undef RGB

void init_muxrgb(lv_obj_t *ui_pnlContent) {
#define RGB(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(rgb, NAME);
    RGB_ELEMENTS
#undef RGB
}
