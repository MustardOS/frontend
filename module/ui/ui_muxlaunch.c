#include "ui_muxshare.h"
#include "ui_muxlaunch.h"

#define LAUNCH(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_launch; \
    lv_obj_t *ui_lbl##NAME##_launch; \
    lv_obj_t *ui_ico##NAME##_launch;

LAUNCH_ELEMENTS
#undef LAUNCH

void init_muxlaunch(lv_obj_t *ui_pnlContent) {
#define LAUNCH(NAME, ENUM, UDATA) CREATE_STATIC_ITEM(launch, NAME);
    LAUNCH_ELEMENTS
#undef LAUNCH
}
