#include "ui_muxshare.h"
#include "ui_muxcoladjust.h"

#define COLADJUST(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_coladjust; \
    lv_obj_t *ui_lbl##NAME##_coladjust; \
    lv_obj_t *ui_ico##NAME##_coladjust; \
    lv_obj_t *ui_dro##NAME##_coladjust;

COLADJUST_ELEMENTS
#undef COLADJUST

void init_muxcoladjust(lv_obj_t *ui_pnlContent) {
#define COLADJUST(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(coladjust, NAME);
    COLADJUST_ELEMENTS
#undef COLADJUST
}
