#include "ui_muxshare.h"
#include "ui_muxpower.h"

#define POWER(NAME, UDATA)                                                                                             \
    lv_obj_t *ui_pnl_##NAME##_power;                                                                                   \
    lv_obj_t *ui_lbl_##NAME##_power;                                                                                   \
    lv_obj_t *ui_ico_##NAME##_power;                                                                                   \
    lv_obj_t *ui_dro_##NAME##_power;

POWER_ELEMENTS
#undef POWER

void init_muxpower(lv_obj_t *ui_pnl_content) {
#define POWER(NAME, UDATA) CREATE_OPTION_ITEM(power, NAME);
    POWER_ELEMENTS
#undef POWER
}
