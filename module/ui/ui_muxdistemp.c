#include "ui_muxshare.h"
#include "ui_muxdistemp.h"

#define DISTEMP(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_distemp;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_distemp;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_distemp;                                                                                 \
    lv_obj_t *ui_dro_##NAME##_distemp;

DISTEMP_ELEMENTS
#undef DISTEMP

void init_muxdistemp(lv_obj_t *ui_pnl_content) {
#define DISTEMP(NAME, UDATA) CREATE_OPTION_ITEM(distemp, NAME);
    DISTEMP_ELEMENTS
#undef DISTEMP
}
