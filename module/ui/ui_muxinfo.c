#include "ui_muxshare.h"
#include "ui_muxinfo.h"

#define INFO(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_info; \
    lv_obj_t *ui_lbl##NAME##_info; \
    lv_obj_t *ui_ico##NAME##_info;

INFO_ELEMENTS
#undef INFO

void init_muxinfo(lv_obj_t *ui_pnlContent) {
#define INFO(NAME, UDATA) CREATE_STATIC_ITEM(info, NAME);
    INFO_ELEMENTS
#undef INFO
}
