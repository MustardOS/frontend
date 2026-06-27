#include "ui_muxshare.h"
#include "ui_muxbatinfo.h"

#define BATINFO(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_batinfo;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_batinfo;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_batinfo;                                                                                 \
    lv_obj_t *ui_val_##NAME##_batinfo;

BATINFO_ELEMENTS
#undef BATINFO

void init_muxbatinfo(const lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme) {
    (void) ui_screen;
    (void) theme;

#define BATINFO(NAME, UDATA) CREATE_VALUE_ITEM(batinfo, NAME);
    BATINFO_ELEMENTS
#undef BATINFO
}
