#include "ui_muxshare.h"
#include "ui_muxsysinfo.h"

#define SYSINFO(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_sysinfo;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_sysinfo;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_sysinfo;                                                                                 \
    lv_obj_t *ui_val_##NAME##_sysinfo;

SYSINFO_ELEMENTS
#undef SYSINFO

void init_muxsysinfo(lv_obj_t *ui_pnl_content) {
#define SYSINFO(NAME, UDATA) CREATE_VALUE_ITEM(sysinfo, NAME);
    SYSINFO_ELEMENTS
#undef SYSINFO
}
