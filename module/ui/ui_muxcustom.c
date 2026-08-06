#include "ui_muxshare.h"
#include "ui_muxcustom.h"

#define FONT(NAME, UDATA)                                                                                              \
    lv_obj_t *ui_pnl_##NAME##_font;                                                                                    \
    lv_obj_t *ui_lbl_##NAME##_font;                                                                                    \
    lv_obj_t *ui_ico_##NAME##_font;                                                                                    \
    lv_obj_t *ui_dro_##NAME##_font;

FONT_ELEMENTS
#undef FONT

#define VISUAL(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_visual;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_visual;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_visual;                                                                                  \
    lv_obj_t *ui_dro_##NAME##_visual;

VISUAL_ELEMENTS
#undef VISUAL

#define CUSTOM(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_custom;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_custom;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_custom;                                                                                  \
    lv_obj_t *ui_dro_##NAME##_custom;

CUSTOM_ELEMENTS
#undef CUSTOM

void init_muxcustom(lv_obj_t *ui_pnl_content) {
#define FONT(NAME, UDATA) CREATE_OPTION_ITEM(font, NAME);
    FONT_ELEMENTS
#undef FONT

#define VISUAL(NAME, UDATA) CREATE_OPTION_ITEM(visual, NAME);
    VISUAL_ELEMENTS
#undef VISUAL

#define CUSTOM(NAME, UDATA) CREATE_OPTION_ITEM(custom, NAME);
    CUSTOM_ELEMENTS
#undef CUSTOM
}

const int font_size_values[] = {0,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34,
                                36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64};

const int font_size_count = sizeof(font_size_values) / sizeof(font_size_values[0]);
