#include "ui_muxshare.h"
#include "ui_muxtweakgen.h"

#define TWEAKGEN(NAME)                 \
    lv_obj_t *ui_pnl##NAME##_tweakgen; \
    lv_obj_t *ui_lbl##NAME##_tweakgen; \
    lv_obj_t *ui_ico##NAME##_tweakgen; \
    lv_obj_t *ui_dro##NAME##_tweakgen;

TWEAKGEN_ELEMENTS
#undef TWEAKGEN

void init_muxtweakgen(lv_obj_t *ui_pnlContent) {
#define TWEAKGEN(NAME) CREATE_OPTION_ITEM(tweakgen, NAME);
    TWEAKGEN_ELEMENTS
#undef TWEAKGEN
}
