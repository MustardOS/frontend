#include "ui_muxshare.h"
#include "ui_muxvisual.h"

#define VISUAL(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_visual; \
    lv_obj_t *ui_lbl##NAME##_visual; \
    lv_obj_t *ui_ico##NAME##_visual; \
    lv_obj_t *ui_dro##NAME##_visual;

VISUAL_ELEMENTS
#undef VISUAL

void init_muxvisual(lv_obj_t *ui_pnlContent) {
#define VISUAL(NAME, UDATA) CREATE_OPTION_ITEM(visual, NAME);
    VISUAL_ELEMENTS
#undef VISUAL
}
