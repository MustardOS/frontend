#include "lvgl/lvgl.h"
#include "ui.h"
#include "common/theme.h"
#include "theme.h"

struct theme_config theme;

void apply_theme() {
    struct big background_elements[] = {
            {ui_scrCharge, theme.SYSTEM.BACKGROUND},
            {ui_pnlCharge, theme.CHARGER.BACKGROUND}
    };
    for (size_t i = 0; i < sizeof(background_elements) / sizeof(background_elements[0]); ++i) {
        lv_obj_set_style_bg_color(background_elements[i].e, lv_color_hex(background_elements[i].c),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small background_alpha_elements[] = {
            {ui_scrCharge, theme.SYSTEM.BACKGROUND_ALPHA}
    };
    for (size_t i = 0; i < sizeof(background_alpha_elements) / sizeof(background_alpha_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(background_alpha_elements[i].e, background_alpha_elements[i].c,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big default_elements[] = {
            {ui_lblCapacity, theme.CHARGER.TEXT},
            {ui_lblVoltage,  theme.CHARGER.TEXT},
            {ui_lblHealth,   theme.CHARGER.TEXT},
            {ui_lblBoot,     theme.CHARGER.TEXT}
    };
    for (size_t i = 0; i < sizeof(default_elements) / sizeof(default_elements[0]); ++i) {
        lv_obj_set_style_text_color(default_elements[i].e, lv_color_hex(default_elements[i].c),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small text_default_alpha_elements[] = {
            {ui_lblCapacity, theme.CHARGER.TEXT_ALPHA},
            {ui_lblVoltage,  theme.CHARGER.TEXT_ALPHA},
            {ui_lblHealth,   theme.CHARGER.TEXT_ALPHA},
            {ui_lblBoot,     theme.CHARGER.TEXT_ALPHA}
    };
    for (size_t i = 0; i < sizeof(text_default_alpha_elements) / sizeof(text_default_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(text_default_alpha_elements[i].e, text_default_alpha_elements[i].c,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small background_alpha_default_elements[] = {
            {ui_pnlCharge, theme.CHARGER.BACKGROUND_ALPHA}
    };
    for (size_t i = 0;
         i < sizeof(background_alpha_default_elements) / sizeof(background_alpha_default_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(background_alpha_default_elements[i].e, background_alpha_default_elements[i].c,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
