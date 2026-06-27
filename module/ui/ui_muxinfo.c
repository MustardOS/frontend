#include "ui_muxshare.h"
#include "ui_muxinfo.h"

#define INFO(NAME, UDATA)                                                                                              \
    lv_obj_t *ui_pnl_##NAME##_info;                                                                                    \
    lv_obj_t *ui_lbl_##NAME##_info;                                                                                    \
    lv_obj_t *ui_ico_##NAME##_info;

INFO_ELEMENTS
#undef INFO

void init_muxinfo(lv_obj_t *ui_pnl_content) {
#define INFO(NAME, UDATA) CREATE_STATIC_ITEM(info, NAME);
    INFO_ELEMENTS
#undef INFO
}
