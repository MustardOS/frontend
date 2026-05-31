#include "ui_muxshare.h"
#include "ui_muxbatinfo.h"

#define BATINFO(NAME, ENUM, UDATA)         \
    lv_obj_t *ui_pnl##NAME##_batinfo;      \
    lv_obj_t *ui_lbl##NAME##_batinfo;      \
    lv_obj_t *ui_ico##NAME##_batinfo;      \
    lv_obj_t *ui_lbl##NAME##Value_batinfo;

BATINFO_ELEMENTS
#undef BATINFO

void init_muxbatinfo(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
    (void) ui_screen;
    (void) theme;

#define BATINFO(NAME, ENUM, UDATA) CREATE_VALUE_ITEM(batinfo, NAME);
    BATINFO_ELEMENTS
#undef BATINFO
}
