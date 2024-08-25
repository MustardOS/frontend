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
    struct big roll_text_elements[] = {
            {ui_rolComboOne, theme.ROLL.TEXT},
            {ui_rolComboTwo, theme.ROLL.TEXT},
            {ui_rolComboThree, theme.ROLL.TEXT},
            {ui_rolComboFour, theme.ROLL.TEXT},
            {ui_rolComboFive, theme.ROLL.TEXT},
            {ui_rolComboSix, theme.ROLL.TEXT},
    };
    for (size_t i = 0; i < sizeof(roll_text_elements) / sizeof(roll_text_elements[0]); ++i) {
        lv_obj_set_style_text_color(roll_text_elements[i].e, lv_color_hex(roll_text_elements[i].c),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big roll_selected_text_elements[] = {
            {ui_rolComboOne, theme.ROLL.SELECT_TEXT},
            {ui_rolComboTwo, theme.ROLL.SELECT_TEXT},
            {ui_rolComboThree, theme.ROLL.SELECT_TEXT},
            {ui_rolComboFour, theme.ROLL.SELECT_TEXT},
            {ui_rolComboFive, theme.ROLL.SELECT_TEXT},
            {ui_rolComboSix, theme.ROLL.SELECT_TEXT},
    };
    for (size_t i = 0; i < sizeof(roll_selected_text_elements) / sizeof(roll_selected_text_elements[0]); ++i) {
        lv_obj_set_style_text_color(roll_selected_text_elements[i].e, lv_color_hex(roll_selected_text_elements[i].c),
                                    LV_PART_SELECTED | LV_STATE_DEFAULT);
    }

    struct big roll_text_alpha_elements[] = {
            {ui_rolComboOne, theme.ROLL.TEXT_ALPHA},
            {ui_rolComboTwo, theme.ROLL.TEXT_ALPHA},
            {ui_rolComboThree, theme.ROLL.TEXT_ALPHA},
            {ui_rolComboFour, theme.ROLL.TEXT_ALPHA},
            {ui_rolComboFive, theme.ROLL.TEXT_ALPHA},
            {ui_rolComboSix, theme.ROLL.TEXT_ALPHA},
    };
    for (size_t i = 0; i < sizeof(roll_text_alpha_elements) / sizeof(roll_text_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(roll_text_alpha_elements[i].e, roll_text_alpha_elements[i].c,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big roll_selected_text_alpha_elements[] = {
            {ui_rolComboOne, theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboTwo, theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboThree, theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboFour, theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboFive, theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboSix, theme.ROLL.SELECT_TEXT_ALPHA},
    };
    for (size_t i = 0;
         i < sizeof(roll_selected_text_alpha_elements) / sizeof(roll_selected_text_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(roll_selected_text_alpha_elements[i].e, roll_selected_text_alpha_elements[i].c,
                                  LV_PART_SELECTED | LV_STATE_DEFAULT);
    }

    struct big roll_background_elements[] = {
            {ui_rolComboOne, theme.ROLL.BACKGROUND},
            {ui_rolComboTwo, theme.ROLL.BACKGROUND},
            {ui_rolComboThree, theme.ROLL.BACKGROUND},
            {ui_rolComboFour, theme.ROLL.BACKGROUND},
            {ui_rolComboFive, theme.ROLL.BACKGROUND},
            {ui_rolComboSix, theme.ROLL.BACKGROUND},
    };
    for (size_t i = 0; i < sizeof(roll_background_elements) / sizeof(roll_background_elements[0]); ++i) {
        lv_obj_set_style_bg_color(roll_background_elements[i].e, lv_color_hex(roll_background_elements[i].c),
                                  LV_PART_SELECTED | LV_STATE_DEFAULT);
    }

    struct small roll_background_alpha_elements[] = {
            {ui_rolComboOne, theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboTwo, theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboThree, theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboFour, theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboFive, theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboSix, theme.ROLL.BACKGROUND_ALPHA},
    };
    for (size_t i = 0; i < sizeof(roll_background_alpha_elements) / sizeof(roll_background_alpha_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(roll_background_alpha_elements[i].e, roll_background_alpha_elements[i].c,
                                LV_PART_SELECTED | LV_STATE_DEFAULT);
    }

    struct big roll_selected_background_elements[] = {
            {ui_rolComboOne, theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboTwo, theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboThree, theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboFour, theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboFive, theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboSix, theme.ROLL.SELECT_BACKGROUND},
    };
    for (size_t i = 0;
         i < sizeof(roll_selected_background_elements) / sizeof(roll_selected_background_elements[0]); ++i) {
        lv_obj_set_style_bg_color(roll_selected_background_elements[i].e,
                                  lv_color_hex(roll_selected_background_elements[i].c),
                                  LV_PART_SELECTED | LV_STATE_FOCUSED);
    }

    struct small roll_selected_background_alpha_elements[] = {
            {ui_rolComboOne, theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboTwo, theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboThree, theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboFour, theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboFive, theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboSix, theme.ROLL.SELECT_BACKGROUND_ALPHA},
    };
    for (size_t i = 0; i < sizeof(roll_selected_background_alpha_elements) /
                           sizeof(roll_selected_background_alpha_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(roll_selected_background_alpha_elements[i].e,
                                roll_selected_background_alpha_elements[i].c,
                                LV_PART_SELECTED | LV_STATE_FOCUSED);
    }

    struct small roll_radius_elements[] = {
            {ui_rolComboOne, theme.ROLL.RADIUS},
            {ui_rolComboTwo, theme.ROLL.RADIUS},
            {ui_rolComboThree, theme.ROLL.RADIUS},
            {ui_rolComboFour, theme.ROLL.RADIUS},
            {ui_rolComboFive, theme.ROLL.RADIUS},
            {ui_rolComboSix, theme.ROLL.RADIUS},
    };
    for (size_t i = 0; i < sizeof(roll_radius_elements) / sizeof(roll_radius_elements[0]); ++i) {
        lv_obj_set_style_radius(roll_radius_elements[i].e, roll_radius_elements[i].c,
                                LV_PART_SELECTED | LV_STATE_DEFAULT);
    }
    struct small roll_selected_radius_elements[] = {
            {ui_rolComboOne, theme.ROLL.SELECT_RADIUS},
            {ui_rolComboTwo, theme.ROLL.SELECT_RADIUS},
            {ui_rolComboThree, theme.ROLL.SELECT_RADIUS},
            {ui_rolComboFour, theme.ROLL.SELECT_RADIUS},
            {ui_rolComboFive, theme.ROLL.SELECT_RADIUS},
            {ui_rolComboSix, theme.ROLL.SELECT_RADIUS},
    };
    for (size_t i = 0; i < sizeof(roll_selected_radius_elements) / sizeof(roll_selected_radius_elements[0]); ++i) {
        lv_obj_set_style_radius(roll_selected_radius_elements[i].e, roll_selected_radius_elements[i].c,
                                LV_PART_SELECTED | LV_STATE_FOCUSED);
    }

    struct small roll_border_radius_elements[] = {
            {ui_rolComboOne, theme.ROLL.BORDER_RADIUS},
            {ui_rolComboTwo, theme.ROLL.BORDER_RADIUS},
            {ui_rolComboThree, theme.ROLL.BORDER_RADIUS},
            {ui_rolComboFour, theme.ROLL.BORDER_RADIUS},
            {ui_rolComboFive, theme.ROLL.BORDER_RADIUS},
            {ui_rolComboSix, theme.ROLL.BORDER_RADIUS},
    };
    for (size_t i = 0; i < sizeof(roll_border_radius_elements) / sizeof(roll_border_radius_elements[0]); ++i) {
        lv_obj_set_style_radius(roll_border_radius_elements[i].e, roll_border_radius_elements[i].c,
                                LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big roll_border_colour_elements[] = {
            {ui_rolComboOne,   theme.ROLL.BORDER_COLOUR},
            {ui_rolComboTwo,   theme.ROLL.BORDER_COLOUR},
            {ui_rolComboThree, theme.ROLL.BORDER_COLOUR},
            {ui_rolComboFour,  theme.ROLL.BORDER_COLOUR},
            {ui_rolComboFive,  theme.ROLL.BORDER_COLOUR},
            {ui_rolComboSix,   theme.ROLL.BORDER_COLOUR},
    };
    for (size_t i = 0; i < sizeof(roll_border_colour_elements) / sizeof(roll_border_colour_elements[0]); ++i) {
        lv_obj_set_style_outline_color(roll_border_colour_elements[i].e, lv_color_hex(roll_border_colour_elements[i].c),
                                       LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small roll_border_alpha_elements[] = {
            {ui_rolComboOne,   theme.ROLL.BORDER_ALPHA},
            {ui_rolComboTwo,   theme.ROLL.BORDER_ALPHA},
            {ui_rolComboThree, theme.ROLL.BORDER_ALPHA},
            {ui_rolComboFour,  theme.ROLL.BORDER_ALPHA},
            {ui_rolComboFive,  theme.ROLL.BORDER_ALPHA},
            {ui_rolComboSix,   theme.ROLL.BORDER_ALPHA},
    };
    for (size_t i = 0; i < sizeof(roll_border_alpha_elements) / sizeof(roll_border_alpha_elements[0]); ++i) {
        lv_obj_set_style_outline_opa(roll_border_alpha_elements[i].e, roll_border_alpha_elements[i].c,
                                     LV_PART_MAIN | LV_STATE_FOCUSED);
    }
}
