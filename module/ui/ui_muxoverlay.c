#include "ui_muxshare.h"
#include "ui_muxoverlay.h"

#define OVERLAY(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_overlay;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_overlay;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_overlay;                                                                                 \
    lv_obj_t *ui_dro_##NAME##_overlay;

OVERLAY_ELEMENTS
#undef OVERLAY

void init_muxoverlay(lv_obj_t *ui_pnl_content) {
#define OVERLAY(NAME, UDATA) CREATE_OPTION_ITEM(overlay, NAME);
    OVERLAY_ELEMENTS
#undef OVERLAY
}
