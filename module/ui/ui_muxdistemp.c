#include "ui_muxshare.h"
#include "ui_muxdistemp.h"

#define DISTEMP(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_distemp; \
    lv_obj_t *ui_lbl##NAME##_distemp; \
    lv_obj_t *ui_ico##NAME##_distemp; \
    lv_obj_t *ui_dro##NAME##_distemp;

DISTEMP_ELEMENTS
#undef DISTEMP

void init_muxdistemp(lv_obj_t *ui_pnlContent) {
#define DISTEMP(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(distemp, NAME);
    DISTEMP_ELEMENTS
#undef DISTEMP
}
