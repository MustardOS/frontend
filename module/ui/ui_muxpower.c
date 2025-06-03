#include "ui_muxshare.h"
#include "ui_muxpower.h"

#define POWER(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_power; \
    lv_obj_t *ui_lbl##NAME##_power; \
    lv_obj_t *ui_ico##NAME##_power; \
    lv_obj_t *ui_dro##NAME##_power;

POWER_ELEMENTS
#undef POWER

void init_muxpower(lv_obj_t *ui_pnlContent) {
#define POWER(NAME, UDATA) CREATE_OPTION_ITEM(power, NAME);
    POWER_ELEMENTS
#undef POWER
}
