#include "../lvgl/lvgl.h"
#include "ui/ui.h"
#include "../common/theme.h"

struct theme_config theme;

struct big {
    lv_obj_t *e;
    uint32_t c;
};

struct small {
    lv_obj_t *e;
    int16_t c;
};

void apply_theme() {
    struct small text_default_alpha_elements[] = {
            {ui_lblButton,    theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblFirst,     theme.LIST_DEFAULT.TEXT_ALPHA},
    };
    for (size_t i = 0; i < sizeof(text_default_alpha_elements) / sizeof(text_default_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(text_default_alpha_elements[i].e, text_default_alpha_elements[i].c,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_list_elements[] = {
            {ui_lblFirst, theme.FONT.LIST_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_top_list_elements) / sizeof(font_pad_top_list_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_list_elements[i].e, font_pad_top_list_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_list_elements[] = {
            {ui_lblFirst, theme.FONT.LIST_PAD_BOTTOM},
    };
    for (size_t i = 0; i < sizeof(font_pad_bottom_list_elements) / sizeof(font_pad_bottom_list_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_list_elements[i].e, font_pad_bottom_list_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
