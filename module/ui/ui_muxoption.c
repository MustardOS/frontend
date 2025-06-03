#include "ui_muxshare.h"
#include "ui_muxoption.h"

#define OPTION(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_option; \
    lv_obj_t *ui_lbl##NAME##_option; \
    lv_obj_t *ui_ico##NAME##_option; \
    lv_obj_t *ui_lbl##NAME##Value_option;

OPTION_ELEMENTS
#undef OPTION

void init_muxoption(lv_obj_t *ui_pnlContent) {
#define OPTION(NAME, UDATA) CREATE_VALUE_ITEM(option, NAME);
    OPTION_ELEMENTS
#undef OPTION

}
