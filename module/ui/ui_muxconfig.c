#include "ui_muxshare.h"
#include "ui_muxconfig.h"

#define CONFIG(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_config; \
    lv_obj_t *ui_lbl##NAME##_config; \
    lv_obj_t *ui_ico##NAME##_config;

CONFIG_ELEMENTS
#undef CONFIG

void init_muxconfig(lv_obj_t *ui_pnlContent) {
#define CONFIG(NAME, ENUM, UDATA) CREATE_STATIC_ITEM(config, NAME);
    CONFIG_ELEMENTS
#undef CONFIG
}
