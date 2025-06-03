#include "ui_muxshare.h"
#include "ui_muxsysinfo.h"

#define SYSINFO(NAME, UDATA)               \
    lv_obj_t *ui_pnl##NAME##_sysinfo;      \
    lv_obj_t *ui_lbl##NAME##_sysinfo;      \
    lv_obj_t *ui_ico##NAME##_sysinfo;      \
    lv_obj_t *ui_lbl##NAME##Value_sysinfo;

SYSINFO_ELEMENTS
#undef SYSINFO

void init_muxsysinfo(lv_obj_t *ui_pnlContent) {
#define SYSINFO(NAME, UDATA) CREATE_VALUE_ITEM(sysinfo, NAME);
    SYSINFO_ELEMENTS
#undef SYSINFO
}
