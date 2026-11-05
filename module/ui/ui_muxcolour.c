#include "ui_muxshare.h"
#include "ui_muxcolour.h"

#define COLOUR(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_colour;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_colour;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_colour;                                                                                  \
    lv_obj_t *ui_dro_##NAME##_colour;
COLOUR_ELEMENTS
#undef COLOUR

void init_muxcolour(lv_obj_t *ui_pnl_content) {
#define COLOUR(NAME, UDATA) CREATE_OPTION_ITEM(colour, NAME);
    COLOUR_ELEMENTS
#undef COLOUR
}
