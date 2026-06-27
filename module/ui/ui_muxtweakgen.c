#include "ui_muxshare.h"
#include "ui_muxtweakgen.h"

#define TWEAKGEN(NAME, UDATA)                                                                                          \
    lv_obj_t *ui_pnl_##NAME##_tweakgen;                                                                                \
    lv_obj_t *ui_lbl_##NAME##_tweakgen;                                                                                \
    lv_obj_t *ui_ico_##NAME##_tweakgen;                                                                                \
    lv_obj_t *ui_dro_##NAME##_tweakgen;

TWEAKGEN_ELEMENTS
#undef TWEAKGEN

void init_muxtweakgen(lv_obj_t *ui_pnl_content) {
#define TWEAKGEN(NAME, UDATA) CREATE_OPTION_ITEM(tweakgen, NAME);
    TWEAKGEN_ELEMENTS
#undef TWEAKGEN
}
