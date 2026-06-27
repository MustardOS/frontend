#include "ui_muxshare.h"
#include "ui_muxvisual.h"

#define VISUAL(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_visual;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_visual;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_visual;                                                                                  \
    lv_obj_t *ui_dro_##NAME##_visual;

VISUAL_ELEMENTS
#undef VISUAL

void init_muxvisual(lv_obj_t *ui_pnl_content) {
#define VISUAL(NAME, UDATA) CREATE_OPTION_ITEM(visual, NAME);
    VISUAL_ELEMENTS
#undef VISUAL
}
