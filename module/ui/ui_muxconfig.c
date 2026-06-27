#include "ui_muxshare.h"
#include "ui_muxconfig.h"

#define CONFIG(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_config;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_config;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_config;

CONFIG_ELEMENTS
#undef CONFIG

void init_muxconfig(lv_obj_t *ui_pnl_content) {
#define CONFIG(NAME, UDATA) CREATE_STATIC_ITEM(config, NAME);
    CONFIG_ELEMENTS
#undef CONFIG
}
