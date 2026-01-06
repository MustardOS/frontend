#include "ui_muxshare.h"
#include "ui_muxoverlay.h"

#define OVERLAY(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_overlay; \
    lv_obj_t *ui_lbl##NAME##_overlay; \
    lv_obj_t *ui_ico##NAME##_overlay; \
    lv_obj_t *ui_dro##NAME##_overlay;

OVERLAY_ELEMENTS
#undef OVERLAY

void init_muxoverlay(lv_obj_t *ui_pnlContent) {
#define OVERLAY(NAME, UDATA) CREATE_OPTION_ITEM(overlay, NAME);
    OVERLAY_ELEMENTS
#undef OVERLAY
}
