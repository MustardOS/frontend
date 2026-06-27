#include "ui_muxshare.h"
#include "ui_muxlaunch.h"

#define LAUNCH(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_launch;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_launch;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_launch;

LAUNCH_ELEMENTS
#undef LAUNCH

void init_muxlaunch(lv_obj_t *ui_pnl_content) {
#define LAUNCH(NAME, UDATA) CREATE_STATIC_ITEM(launch, NAME);
    LAUNCH_ELEMENTS
#undef LAUNCH
}
