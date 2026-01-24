#include "ui_muxshare.h"
#include "ui_muxspace.h"

#include "../../common/device.h"
#include "../../common/theme.h"

#define SPACE(NAME, ENUM, UDATA)         \
    lv_obj_t *ui_pnl##NAME##_space;      \
    lv_obj_t *ui_lbl##NAME##_space;      \
    lv_obj_t *ui_ico##NAME##_space;      \
    lv_obj_t *ui_lbl##NAME##Value_space; \
    lv_obj_t *ui_pnl##NAME##Bar_space;   \
    lv_obj_t *ui_bar##NAME##_space;

SPACE_ELEMENTS
#undef SPACE

void init_muxspace(lv_obj_t *ui_pnlContent) {
#define SPACE(NAME, ENUM, UDATA) CREATE_BAR_ITEM(space, NAME);
    SPACE_ELEMENTS
#undef SPACE
}
