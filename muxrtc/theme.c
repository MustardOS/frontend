#include "../lvgl/lvgl.h"
#include "ui/ui.h"
#include "../common/theme.h"
#include "../common/glyph.h"

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
    struct big background_elements[] = {
            {ui_scrRTC,                theme.SYSTEM.BACKGROUND},
            {ui_pnlFooter,             theme.FOOTER.BACKGROUND},
            {ui_pnlHeader,             theme.HEADER.BACKGROUND},
            {ui_pnlHelpMessage,        theme.HELP.BACKGROUND},
            {ui_lblYear,               theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblMonth,              theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblDay,                theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblHour,               theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblMinute,             theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblNotation,           theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblTimezone,           theme.LIST_DEFAULT.BACKGROUND},
            {ui_pnlMessage,            theme.MESSAGE.BACKGROUND},
            {ui_pnlProgressBrightness, theme.BAR.PANEL_BACKGROUND},
            {ui_barProgressBrightness, theme.BAR.PROGRESS_MAIN_BACKGROUND},
            {ui_pnlProgressVolume,     theme.BAR.PANEL_BACKGROUND},
            {ui_barProgressVolume,     theme.BAR.PROGRESS_MAIN_BACKGROUND},
    };
    for (size_t i = 0; i < sizeof(background_elements) / sizeof(background_elements[0]); ++i) {
        lv_obj_set_style_bg_color(background_elements[i].e, lv_color_hex(background_elements[i].c),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small background_alpha_elements[] = {
            {ui_scrRTC,                theme.SYSTEM.BACKGROUND_ALPHA},
            {ui_pnlFooter,             theme.FOOTER.BACKGROUND_ALPHA},
            {ui_pnlHeader,             theme.HEADER.BACKGROUND_ALPHA},
            {ui_pnlHelpMessage,        theme.HELP.BACKGROUND_ALPHA},
            {ui_pnlMessage,            theme.MESSAGE.BACKGROUND_ALPHA},
            {ui_pnlProgressBrightness, theme.BAR.PANEL_BACKGROUND_ALPHA},
            {ui_barProgressBrightness, theme.BAR.PROGRESS_MAIN_BACKGROUND_ALPHA},
            {ui_pnlProgressVolume,     theme.BAR.PANEL_BACKGROUND_ALPHA},
            {ui_barProgressVolume,     theme.BAR.PROGRESS_MAIN_BACKGROUND_ALPHA},
    };
    for (size_t i = 0; i < sizeof(background_alpha_elements) / sizeof(background_alpha_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(background_alpha_elements[i].e, background_alpha_elements[i].c,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big progress_elements[] = {
            {ui_barProgressBrightness, theme.BAR.PROGRESS_ACTIVE_BACKGROUND},
            {ui_barProgressVolume, theme.BAR.PROGRESS_ACTIVE_BACKGROUND},
    };
    for (size_t i = 0; i < sizeof(progress_elements) / sizeof(progress_elements[0]); ++i) {
        lv_obj_set_style_bg_color(progress_elements[i].e, lv_color_hex(progress_elements[i].c),
                                  LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }

    struct small progress_alpha_elements[] = {
            {ui_barProgressBrightness, theme.BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA},
            {ui_barProgressVolume, theme.BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA},
    };
    for (size_t i = 0; i < sizeof(progress_alpha_elements) / sizeof(progress_alpha_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(progress_alpha_elements[i].e, progress_alpha_elements[i].c,
                                LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }

    struct big background_focus_elements[] = {
            {ui_lblYear,     theme.LIST_FOCUS.BACKGROUND},
            {ui_lblMonth,    theme.LIST_FOCUS.BACKGROUND},
            {ui_lblDay,      theme.LIST_FOCUS.BACKGROUND},
            {ui_lblHour,     theme.LIST_FOCUS.BACKGROUND},
            {ui_lblMinute,   theme.LIST_FOCUS.BACKGROUND},
            {ui_lblNotation, theme.LIST_FOCUS.BACKGROUND},
            {ui_lblTimezone, theme.LIST_FOCUS.BACKGROUND},
    };
    for (size_t i = 0; i < sizeof(background_focus_elements) / sizeof(background_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_color(background_focus_elements[i].e, lv_color_hex(background_focus_elements[i].c),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big gradient_elements[] = {
            {ui_lblYear,     theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblMonth,    theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblDay,      theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblHour,     theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblMinute,   theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblNotation, theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblTimezone, theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
    };
    for (size_t i = 0; i < sizeof(gradient_elements) / sizeof(gradient_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_color(gradient_elements[i].e, lv_color_hex(gradient_elements[i].c),
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big gradient_focused_elements[] = {
            {ui_lblYear,     theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblMonth,    theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblDay,      theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblHour,     theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblMinute,   theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblNotation, theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblTimezone, theme.LIST_FOCUS.BACKGROUND_GRADIENT},
    };
    for (size_t i = 0; i < sizeof(gradient_focused_elements) / sizeof(gradient_focused_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_color(gradient_focused_elements[i].e, lv_color_hex(gradient_focused_elements[i].c),
                                       LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big indicator_elements[] = {
            {ui_lblYear,     theme.LIST_DEFAULT.INDICATOR},
            {ui_lblMonth,    theme.LIST_DEFAULT.INDICATOR},
            {ui_lblDay,      theme.LIST_DEFAULT.INDICATOR},
            {ui_lblHour,     theme.LIST_DEFAULT.INDICATOR},
            {ui_lblMinute,   theme.LIST_DEFAULT.INDICATOR},
            {ui_lblNotation, theme.LIST_DEFAULT.INDICATOR},
            {ui_lblTimezone, theme.LIST_DEFAULT.INDICATOR},
    };
    for (size_t i = 0; i < sizeof(indicator_elements) / sizeof(indicator_elements[0]); ++i) {
        lv_obj_set_style_border_color(indicator_elements[i].e, lv_color_hex(indicator_elements[i].c),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big indicator_focus_elements[] = {
            {ui_lblYear,     theme.LIST_FOCUS.INDICATOR},
            {ui_lblMonth,    theme.LIST_FOCUS.INDICATOR},
            {ui_lblDay,      theme.LIST_FOCUS.INDICATOR},
            {ui_lblHour,     theme.LIST_FOCUS.INDICATOR},
            {ui_lblMinute,   theme.LIST_FOCUS.INDICATOR},
            {ui_lblNotation, theme.LIST_FOCUS.INDICATOR},
            {ui_lblTimezone, theme.LIST_FOCUS.INDICATOR},
    };
    for (size_t i = 0; i < sizeof(indicator_focus_elements) / sizeof(indicator_focus_elements[0]); ++i) {
        lv_obj_set_style_border_color(indicator_focus_elements[i].e, lv_color_hex(indicator_focus_elements[i].c),
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big default_elements[] = {
            {ui_lblYear,               theme.LIST_DEFAULT.TEXT},
            {ui_lblMonth,              theme.LIST_DEFAULT.TEXT},
            {ui_lblDay,                theme.LIST_DEFAULT.TEXT},
            {ui_lblHour,               theme.LIST_DEFAULT.TEXT},
            {ui_lblMinute,             theme.LIST_DEFAULT.TEXT},
            {ui_lblNotation,           theme.LIST_DEFAULT.TEXT},
            {ui_lblTimezone,           theme.LIST_DEFAULT.TEXT},
            {ui_icoYear,               theme.LIST_DEFAULT.TEXT},
            {ui_icoMonth,              theme.LIST_DEFAULT.TEXT},
            {ui_icoDay,                theme.LIST_DEFAULT.TEXT},
            {ui_icoHour,               theme.LIST_DEFAULT.TEXT},
            {ui_icoMinute,             theme.LIST_DEFAULT.TEXT},
            {ui_icoNotation,           theme.LIST_DEFAULT.TEXT},
            {ui_icoTimezone,           theme.LIST_DEFAULT.TEXT},
            {ui_lblYearValue,          theme.LIST_DEFAULT.TEXT},
            {ui_lblMonthValue,         theme.LIST_DEFAULT.TEXT},
            {ui_lblDayValue,           theme.LIST_DEFAULT.TEXT},
            {ui_lblHourValue,          theme.LIST_DEFAULT.TEXT},
            {ui_lblMinuteValue,        theme.LIST_DEFAULT.TEXT},
            {ui_lblNotationValue,      theme.LIST_DEFAULT.TEXT},
            {ui_lblTimezoneValue,      theme.LIST_DEFAULT.TEXT},
            {ui_lblDatetime,           theme.DATETIME.TEXT},
            {ui_lblMessage,            theme.MESSAGE.TEXT},
            {ui_lblTitle,              theme.HEADER.TEXT},
            {ui_lblHelpContent,        theme.HELP.CONTENT},
            {ui_lblHelpHeader,         theme.HELP.TITLE},
            {ui_staBluetooth,          theme.STATUS.BLUETOOTH.NORMAL},
            {ui_staNetwork,            theme.STATUS.NETWORK.NORMAL},
            {ui_staCapacity,           theme.STATUS.BATTERY.NORMAL},
            {ui_lblNavA,               theme.NAV.A.TEXT},
            {ui_lblNavB,               theme.NAV.B.TEXT},
            {ui_lblNavC,               theme.NAV.C.TEXT},
            {ui_lblNavX,               theme.NAV.X.TEXT},
            {ui_lblNavY,               theme.NAV.Y.TEXT},
            {ui_lblNavZ,               theme.NAV.Z.TEXT},
            {ui_lblNavMenu,            theme.NAV.MENU.TEXT},
            {ui_lblNavAGlyph,          theme.NAV.A.GLYPH},
            {ui_lblNavBGlyph,          theme.NAV.B.GLYPH},
            {ui_lblNavCGlyph,          theme.NAV.C.GLYPH},
            {ui_lblNavXGlyph,          theme.NAV.X.GLYPH},
            {ui_lblNavYGlyph,          theme.NAV.Y.GLYPH},
            {ui_lblNavZGlyph,          theme.NAV.Z.GLYPH},
            {ui_lblNavMenuGlyph,       theme.NAV.MENU.GLYPH},
            {ui_icoProgressBrightness, theme.BAR.ICON},
            {ui_icoProgressVolume,     theme.BAR.ICON},
    };
    for (size_t i = 0; i < sizeof(default_elements) / sizeof(default_elements[0]); ++i) {
        lv_obj_set_style_text_color(default_elements[i].e, lv_color_hex(default_elements[i].c),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big focus_elements[] = {
            {ui_lblYear,          theme.LIST_FOCUS.TEXT},
            {ui_lblMonth,         theme.LIST_FOCUS.TEXT},
            {ui_lblDay,           theme.LIST_FOCUS.TEXT},
            {ui_lblHour,          theme.LIST_FOCUS.TEXT},
            {ui_lblMinute,        theme.LIST_FOCUS.TEXT},
            {ui_lblNotation,      theme.LIST_FOCUS.TEXT},
            {ui_lblTimezone,      theme.LIST_FOCUS.TEXT},
            {ui_icoYear,          theme.LIST_FOCUS.TEXT},
            {ui_icoMonth,         theme.LIST_FOCUS.TEXT},
            {ui_icoDay,           theme.LIST_FOCUS.TEXT},
            {ui_icoHour,          theme.LIST_FOCUS.TEXT},
            {ui_icoMinute,        theme.LIST_FOCUS.TEXT},
            {ui_icoNotation,      theme.LIST_FOCUS.TEXT},
            {ui_icoTimezone,      theme.LIST_FOCUS.TEXT},
            {ui_lblYearValue,     theme.LIST_FOCUS.TEXT},
            {ui_lblMonthValue,    theme.LIST_FOCUS.TEXT},
            {ui_lblDayValue,      theme.LIST_FOCUS.TEXT},
            {ui_lblHourValue,     theme.LIST_FOCUS.TEXT},
            {ui_lblMinuteValue,   theme.LIST_FOCUS.TEXT},
            {ui_lblNotationValue, theme.LIST_FOCUS.TEXT},
            {ui_lblTimezoneValue, theme.LIST_FOCUS.TEXT},
    };
    for (size_t i = 0; i < sizeof(focus_elements) / sizeof(focus_elements[0]); ++i) {
        lv_obj_set_style_text_color(focus_elements[i].e, lv_color_hex(focus_elements[i].c),
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big border_elements[] = {
            {ui_pnlHelpMessage,        theme.HELP.BORDER},
            {ui_pnlMessage,            theme.MESSAGE.BORDER},
            {ui_pnlProgressBrightness, theme.BAR.PANEL_BORDER},
            {ui_pnlProgressVolume,     theme.BAR.PANEL_BORDER},
    };
    for (size_t i = 0; i < sizeof(border_elements) / sizeof(border_elements[0]); ++i) {
        lv_obj_set_style_border_color(border_elements[i].e, lv_color_hex(border_elements[i].c),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small border_alpha_elements[] = {
            {ui_pnlHelpMessage,        theme.HELP.BORDER_ALPHA},
            {ui_pnlMessage,            theme.MESSAGE.BORDER_ALPHA},
            {ui_pnlProgressBrightness, theme.BAR.PANEL_BORDER_ALPHA},
            {ui_pnlProgressVolume,     theme.BAR.PANEL_BORDER_ALPHA},
    };
    for (size_t i = 0; i < sizeof(border_alpha_elements) / sizeof(border_alpha_elements[0]); ++i) {
        lv_obj_set_style_border_opa(border_alpha_elements[i].e, border_alpha_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small text_default_alpha_elements[] = {
            {ui_lblYear,               theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblMonth,              theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblDay,                theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblHour,               theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblMinute,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblNotation,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblTimezone,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoYear,               theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoMonth,              theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoDay,                theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoHour,               theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoMinute,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoNotation,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoTimezone,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblYearValue,          theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblMonthValue,         theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblDayValue,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblHourValue,          theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblMinuteValue,        theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblNotationValue,      theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblTimezoneValue,      theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblTitle,              theme.HEADER.TEXT_ALPHA},
            {ui_lblMessage,            theme.MESSAGE.TEXT_ALPHA},
            {ui_lblDatetime,           theme.DATETIME.ALPHA},
            {ui_staBluetooth,          theme.STATUS.BLUETOOTH.NORMAL_ALPHA},
            {ui_staNetwork,            theme.STATUS.NETWORK.NORMAL_ALPHA},
            {ui_staCapacity,           theme.STATUS.BATTERY.NORMAL_ALPHA},
            {ui_lblNavA,               theme.NAV.A.TEXT_ALPHA},
            {ui_lblNavB,               theme.NAV.B.TEXT_ALPHA},
            {ui_lblNavC,               theme.NAV.C.TEXT_ALPHA},
            {ui_lblNavX,               theme.NAV.X.TEXT_ALPHA},
            {ui_lblNavY,               theme.NAV.Y.TEXT_ALPHA},
            {ui_lblNavZ,               theme.NAV.Z.TEXT_ALPHA},
            {ui_lblNavMenu,            theme.NAV.MENU.TEXT_ALPHA},
            {ui_lblNavAGlyph,          theme.NAV.A.GLYPH_ALPHA},
            {ui_lblNavBGlyph,          theme.NAV.B.GLYPH_ALPHA},
            {ui_lblNavCGlyph,          theme.NAV.C.GLYPH_ALPHA},
            {ui_lblNavXGlyph,          theme.NAV.X.GLYPH_ALPHA},
            {ui_lblNavYGlyph,          theme.NAV.Y.GLYPH_ALPHA},
            {ui_lblNavZGlyph,          theme.NAV.Z.GLYPH_ALPHA},
            {ui_lblNavMenuGlyph,       theme.NAV.MENU.GLYPH_ALPHA},
            {ui_icoProgressBrightness, theme.BAR.ICON_ALPHA},
            {ui_icoProgressVolume,     theme.BAR.ICON_ALPHA},
    };
    for (size_t i = 0; i < sizeof(text_default_alpha_elements) / sizeof(text_default_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(text_default_alpha_elements[i].e, text_default_alpha_elements[i].c,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small text_focus_alpha_elements[] = {
            {ui_lblYear,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblMonth,         theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblDay,           theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblHour,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblMinute,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblNotation,      theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblTimezone,      theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoYear,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoMonth,         theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoDay,           theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoHour,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoMinute,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoNotation,      theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoTimezone,      theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblYearValue,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblMonthValue,    theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblDayValue,      theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblHourValue,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblMinuteValue,   theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblNotationValue, theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblTimezoneValue, theme.LIST_FOCUS.TEXT_ALPHA},
    };
    for (size_t i = 0; i < sizeof(text_focus_alpha_elements) / sizeof(text_focus_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(text_focus_alpha_elements[i].e, text_focus_alpha_elements[i].c,
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small indicator_default_alpha_elements[] = {
            {ui_lblYear,     theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblMonth,    theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblDay,      theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblHour,     theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblMinute,   theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblNotation, theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblTimezone, theme.LIST_DEFAULT.INDICATOR_ALPHA},
    };
    for (size_t i = 0;
         i < sizeof(indicator_default_alpha_elements) / sizeof(indicator_default_alpha_elements[0]); ++i) {
        lv_obj_set_style_border_opa(indicator_default_alpha_elements[i].e, indicator_default_alpha_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small indicator_focus_alpha_elements[] = {
            {ui_lblYear,     theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblMonth,    theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblDay,      theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblHour,     theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblMinute,   theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblNotation, theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblTimezone, theme.LIST_FOCUS.INDICATOR_ALPHA},
    };
    for (size_t i = 0; i < sizeof(indicator_focus_alpha_elements) / sizeof(indicator_focus_alpha_elements[0]); ++i) {
        lv_obj_set_style_border_opa(indicator_focus_alpha_elements[i].e, indicator_focus_alpha_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small item_height_elements[] = {
            {ui_lblYear,     theme.MUX.ITEM.HEIGHT},
            {ui_lblMonth,    theme.MUX.ITEM.HEIGHT},
            {ui_lblDay,      theme.MUX.ITEM.HEIGHT},
            {ui_lblHour,     theme.MUX.ITEM.HEIGHT},
            {ui_lblMinute,   theme.MUX.ITEM.HEIGHT},
            {ui_lblNotation, theme.MUX.ITEM.HEIGHT},
            {ui_lblTimezone, theme.MUX.ITEM.HEIGHT},
            {ui_icoYear,     theme.MUX.ITEM.HEIGHT},
            {ui_icoMonth,    theme.MUX.ITEM.HEIGHT},
            {ui_icoDay,      theme.MUX.ITEM.HEIGHT},
            {ui_icoHour,     theme.MUX.ITEM.HEIGHT},
            {ui_icoMinute,   theme.MUX.ITEM.HEIGHT},
            {ui_icoNotation, theme.MUX.ITEM.HEIGHT},
            {ui_icoTimezone, theme.MUX.ITEM.HEIGHT},
            {ui_lblYearValue,     theme.MUX.ITEM.HEIGHT},
            {ui_lblMonthValue,    theme.MUX.ITEM.HEIGHT},
            {ui_lblDayValue,      theme.MUX.ITEM.HEIGHT},
            {ui_lblHourValue,     theme.MUX.ITEM.HEIGHT},
            {ui_lblMinuteValue,   theme.MUX.ITEM.HEIGHT},
            {ui_lblNotationValue, theme.MUX.ITEM.HEIGHT},
            {ui_lblTimezoneValue, theme.MUX.ITEM.HEIGHT},
            {ui_pnlContent,       theme.MISC.CONTENT.HEIGHT},
            {ui_pnlGlyph,         theme.MISC.CONTENT.HEIGHT},
            {ui_pnlHighlight,     theme.MISC.CONTENT.HEIGHT},
    };
    for (size_t i = 0; i < sizeof(item_height_elements) / sizeof(item_height_elements[0]); ++i) {
        lv_obj_set_height(item_height_elements[i].e, item_height_elements[i].c);
    }

    struct small item_width_elements[] = {
            {ui_lblYear,     theme.MISC.CONTENT.WIDTH},
            {ui_lblMonth,    theme.MISC.CONTENT.WIDTH},
            {ui_lblDay,      theme.MISC.CONTENT.WIDTH},
            {ui_lblHour,     theme.MISC.CONTENT.WIDTH},
            {ui_lblMinute,   theme.MISC.CONTENT.WIDTH},
            {ui_lblNotation, theme.MISC.CONTENT.WIDTH},
            {ui_lblTimezone, theme.MISC.CONTENT.WIDTH},
            {ui_icoYear,     theme.MISC.CONTENT.WIDTH},
            {ui_icoMonth,    theme.MISC.CONTENT.WIDTH},
            {ui_icoDay,      theme.MISC.CONTENT.WIDTH},
            {ui_icoHour,     theme.MISC.CONTENT.WIDTH},
            {ui_icoMinute,   theme.MISC.CONTENT.WIDTH},
            {ui_icoNotation, theme.MISC.CONTENT.WIDTH},
            {ui_icoTimezone, theme.MISC.CONTENT.WIDTH},
            {ui_lblYearValue,     theme.MISC.CONTENT.WIDTH},
            {ui_lblMonthValue,    theme.MISC.CONTENT.WIDTH},
            {ui_lblDayValue,      theme.MISC.CONTENT.WIDTH},
            {ui_lblHourValue,     theme.MISC.CONTENT.WIDTH},
            {ui_lblMinuteValue,   theme.MISC.CONTENT.WIDTH},
            {ui_lblNotationValue, theme.MISC.CONTENT.WIDTH},
            {ui_lblTimezoneValue, theme.MISC.CONTENT.WIDTH},
    };
    for (size_t i = 0; i < sizeof(item_width_elements) / sizeof(item_width_elements[0]); ++i) {
        lv_obj_set_width(item_width_elements[i].e, item_width_elements[i].c);
    }

    struct small gradient_start_default_elements[] = {
            {ui_lblYear,     theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblMonth,    theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblDay,      theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblHour,     theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblMinute,   theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblNotation, theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblTimezone, theme.LIST_DEFAULT.GRADIENT_START},
    };
    for (size_t i = 0; i < sizeof(gradient_start_default_elements) / sizeof(gradient_start_default_elements[0]); ++i) {
        lv_obj_set_style_bg_main_stop(gradient_start_default_elements[i].e, gradient_start_default_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small gradient_start_focus_elements[] = {
            {ui_lblYear,     theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblMonth,    theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblDay,      theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblHour,     theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblMinute,   theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblNotation, theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblTimezone, theme.LIST_FOCUS.GRADIENT_START},
    };
    for (size_t i = 0; i < sizeof(gradient_start_focus_elements) / sizeof(gradient_start_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_main_stop(gradient_start_focus_elements[i].e, gradient_start_focus_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small gradient_stop_default_elements[] = {
            {ui_lblYear,     theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblMonth,    theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblDay,      theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblHour,     theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblMinute,   theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblNotation, theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblTimezone, theme.LIST_DEFAULT.GRADIENT_STOP},
    };
    for (size_t i = 0; i < sizeof(gradient_stop_default_elements) / sizeof(gradient_stop_default_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_stop(gradient_stop_default_elements[i].e, gradient_stop_default_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small gradient_stop_focus_elements[] = {
            {ui_lblYear,     theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblMonth,    theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblDay,      theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblHour,     theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblMinute,   theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblNotation, theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblTimezone, theme.LIST_FOCUS.GRADIENT_STOP},
    };
    for (size_t i = 0; i < sizeof(gradient_stop_focus_elements) / sizeof(gradient_stop_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_stop(gradient_stop_focus_elements[i].e, gradient_stop_focus_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small background_alpha_default_elements[] = {
            {ui_lblYear,     theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblMonth,    theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblDay,      theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblHour,     theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblMinute,   theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblNotation, theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblTimezone, theme.LIST_DEFAULT.BACKGROUND_ALPHA},
    };
    for (size_t i = 0;
         i < sizeof(background_alpha_default_elements) / sizeof(background_alpha_default_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(background_alpha_default_elements[i].e, background_alpha_default_elements[i].c,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small background_alpha_focus_elements[] = {
            {ui_lblYear,     theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblMonth,    theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblDay,      theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblHour,     theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblMinute,   theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblNotation, theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblTimezone, theme.LIST_FOCUS.BACKGROUND_ALPHA},
    };
    for (size_t i = 0; i < sizeof(background_alpha_focus_elements) / sizeof(background_alpha_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(background_alpha_focus_elements[i].e, background_alpha_focus_elements[i].c,
                                LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small radius_elements[] = {
            {ui_pnlHelpMessage,        theme.HELP.RADIUS},
            {ui_pnlMessage,            theme.MESSAGE.RADIUS},
            {ui_pnlProgressBrightness, theme.BAR.PANEL_BORDER_RADIUS},
            {ui_barProgressBrightness, theme.BAR.PROGRESS_RADIUS},
            {ui_pnlProgressVolume,     theme.BAR.PANEL_BORDER_RADIUS},
            {ui_barProgressVolume,     theme.BAR.PROGRESS_RADIUS},
            {ui_lblYear,     theme.LIST_DEFAULT.RADIUS},
            {ui_lblMonth,    theme.LIST_DEFAULT.RADIUS},
            {ui_lblDay,      theme.LIST_DEFAULT.RADIUS},
            {ui_lblHour,     theme.LIST_DEFAULT.RADIUS},
            {ui_lblMinute,   theme.LIST_DEFAULT.RADIUS},
            {ui_lblNotation, theme.LIST_DEFAULT.RADIUS},
            {ui_lblTimezone, theme.LIST_DEFAULT.RADIUS},
    };
    for (size_t i = 0; i < sizeof(radius_elements) / sizeof(radius_elements[0]); ++i) {
        lv_obj_set_style_radius(radius_elements[i].e, radius_elements[i].c,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_header_elements[] = {
            {ui_lblTitle,    theme.FONT.HEADER_PAD_TOP},
            {ui_lblDatetime, theme.FONT.HEADER_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_top_header_elements) / sizeof(font_pad_top_header_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_header_elements[i].e, font_pad_top_header_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_header_elements[] = {
            {ui_lblTitle,    theme.FONT.HEADER_PAD_BOTTOM},
            {ui_lblDatetime, theme.FONT.HEADER_PAD_BOTTOM},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_bottom_header_elements) / sizeof(font_pad_bottom_header_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_header_elements[i].e, font_pad_bottom_header_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_footer_elements[] = {
            {ui_lblNavA,    theme.FONT.FOOTER_PAD_TOP},
            {ui_lblNavB,    theme.FONT.FOOTER_PAD_TOP},
            {ui_lblNavC,    theme.FONT.FOOTER_PAD_TOP},
            {ui_lblNavX,    theme.FONT.FOOTER_PAD_TOP},
            {ui_lblNavY,    theme.FONT.FOOTER_PAD_TOP},
            {ui_lblNavZ,    theme.FONT.FOOTER_PAD_TOP},
            {ui_lblNavMenu, theme.FONT.FOOTER_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_top_footer_elements) / sizeof(font_pad_top_footer_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_footer_elements[i].e, font_pad_top_footer_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_footer_elements[] = {
            {ui_lblNavA,    theme.FONT.FOOTER_PAD_BOTTOM},
            {ui_lblNavB,    theme.FONT.FOOTER_PAD_BOTTOM},
            {ui_lblNavC,    theme.FONT.FOOTER_PAD_BOTTOM},
            {ui_lblNavX,    theme.FONT.FOOTER_PAD_BOTTOM},
            {ui_lblNavY,    theme.FONT.FOOTER_PAD_BOTTOM},
            {ui_lblNavZ,    theme.FONT.FOOTER_PAD_BOTTOM},
            {ui_lblNavMenu, theme.FONT.FOOTER_PAD_BOTTOM},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_bottom_footer_elements) / sizeof(font_pad_bottom_footer_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_footer_elements[i].e, font_pad_bottom_footer_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_footer_icon_elements[] = {
            {ui_lblNavAGlyph,    theme.FONT.FOOTER_ICON_PAD_TOP},
            {ui_lblNavBGlyph,    theme.FONT.FOOTER_ICON_PAD_TOP},
            {ui_lblNavCGlyph,    theme.FONT.FOOTER_ICON_PAD_TOP},
            {ui_lblNavXGlyph,    theme.FONT.FOOTER_ICON_PAD_TOP},
            {ui_lblNavYGlyph,    theme.FONT.FOOTER_ICON_PAD_TOP},
            {ui_lblNavZGlyph,    theme.FONT.FOOTER_ICON_PAD_TOP},
            {ui_lblNavMenuGlyph, theme.FONT.FOOTER_ICON_PAD_TOP},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_top_footer_icon_elements) / sizeof(font_pad_top_footer_icon_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_footer_icon_elements[i].e, font_pad_top_footer_icon_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_footer_icon_elements[] = {
            {ui_lblNavAGlyph,    theme.FONT.FOOTER_ICON_PAD_BOTTOM},
            {ui_lblNavBGlyph,    theme.FONT.FOOTER_ICON_PAD_BOTTOM},
            {ui_lblNavCGlyph,    theme.FONT.FOOTER_ICON_PAD_BOTTOM},
            {ui_lblNavXGlyph,    theme.FONT.FOOTER_ICON_PAD_BOTTOM},
            {ui_lblNavYGlyph,    theme.FONT.FOOTER_ICON_PAD_BOTTOM},
            {ui_lblNavZGlyph,    theme.FONT.FOOTER_ICON_PAD_BOTTOM},
            {ui_lblNavMenuGlyph, theme.FONT.FOOTER_ICON_PAD_BOTTOM},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_bottom_footer_icon_elements) / sizeof(font_pad_bottom_footer_icon_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_footer_icon_elements[i].e,
                                    font_pad_bottom_footer_icon_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_list_top_elements[] = {
            {ui_lblYear,     theme.FONT.LIST_PAD_TOP},
            {ui_lblMonth,    theme.FONT.LIST_PAD_TOP},
            {ui_lblDay,      theme.FONT.LIST_PAD_TOP},
            {ui_lblHour,     theme.FONT.LIST_PAD_TOP},
            {ui_lblMinute,   theme.FONT.LIST_PAD_TOP},
            {ui_lblNotation, theme.FONT.LIST_PAD_TOP},
            {ui_lblTimezone, theme.FONT.LIST_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_list_top_elements) / sizeof(font_pad_list_top_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_list_top_elements[i].e, font_pad_list_top_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_list_bottom_elements[] = {
            {ui_lblYear,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblMonth,    theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblDay,      theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblHour,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblMinute,   theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblNotation, theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblTimezone, theme.FONT.LIST_PAD_BOTTOM},
    };
    for (size_t i = 0; i < sizeof(font_pad_list_bottom_elements) / sizeof(font_pad_list_bottom_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_list_bottom_elements[i].e, font_pad_list_bottom_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_dropdown_elements[] = {
            {ui_lblYearValue,     theme.FONT.LIST_PAD_TOP},
            {ui_lblMonthValue,    theme.FONT.LIST_PAD_TOP},
            {ui_lblDayValue,      theme.FONT.LIST_PAD_TOP},
            {ui_lblHourValue,     theme.FONT.LIST_PAD_TOP},
            {ui_lblMinuteValue,   theme.FONT.LIST_PAD_TOP},
            {ui_lblNotationValue, theme.FONT.LIST_PAD_TOP},
            {ui_lblTimezoneValue, theme.FONT.LIST_PAD_TOP},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_top_dropdown_elements) / sizeof(font_pad_top_dropdown_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_dropdown_elements[i].e, font_pad_top_dropdown_elements[i].c + 5,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_dropdown_elements[] = {
            {ui_lblYearValue,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblMonthValue,    theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblDayValue,      theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblHourValue,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblMinuteValue,   theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblNotationValue, theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblTimezoneValue, theme.FONT.LIST_PAD_BOTTOM},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_bottom_dropdown_elements) / sizeof(font_pad_bottom_dropdown_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_dropdown_elements[i].e,
                                    font_pad_bottom_dropdown_elements[i].c + 5,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_list_icon_elements[] = {
            {ui_icoYear,     theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoMonth,    theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoDay,      theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoHour,     theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoMinute,   theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoNotation, theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoTimezone, theme.FONT.LIST_ICON_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_top_list_icon_elements) / sizeof(font_pad_top_list_icon_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_list_icon_elements[i].e, font_pad_top_list_icon_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_list_icon_elements[] = {
            {ui_icoYear,     theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoMonth,    theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoDay,      theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoHour,     theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoMinute,   theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoNotation, theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoTimezone, theme.FONT.LIST_ICON_PAD_BOTTOM},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_bottom_list_icon_elements) / sizeof(font_pad_bottom_list_icon_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_list_icon_elements[i].e, font_pad_bottom_list_icon_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_header_icon_elements[] = {
            {ui_staBluetooth, theme.FONT.HEADER_ICON_PAD_TOP},
            {ui_staNetwork,   theme.FONT.HEADER_ICON_PAD_TOP},
            {ui_staCapacity,  theme.FONT.HEADER_ICON_PAD_TOP},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_top_header_icon_elements) / sizeof(font_pad_top_header_icon_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_header_icon_elements[i].e, font_pad_top_header_icon_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_header_icon_elements[] = {
            {ui_staBluetooth, theme.FONT.HEADER_ICON_PAD_BOTTOM},
            {ui_staNetwork,   theme.FONT.HEADER_ICON_PAD_BOTTOM},
            {ui_staCapacity,  theme.FONT.HEADER_ICON_PAD_BOTTOM},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_bottom_header_icon_elements) / sizeof(font_pad_bottom_header_icon_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_header_icon_elements[i].e,
                                    font_pad_bottom_header_icon_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_message_elements[] = {
            {ui_lblMessage, theme.FONT.MESSAGE_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_top_message_elements) / sizeof(font_pad_top_message_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_message_elements[i].e, font_pad_top_message_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_message_elements[] = {
            {ui_lblMessage, theme.FONT.MESSAGE_PAD_BOTTOM},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_bottom_message_elements) / sizeof(font_pad_bottom_message_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_message_elements[i].e, font_pad_bottom_message_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small content_pad_left_element[] = {
            {ui_pnlContent, theme.MISC.CONTENT.PADDING_LEFT},
            {ui_pnlGlyph,   theme.MISC.CONTENT.PADDING_LEFT},
            {ui_pnlHighlight,   theme.MISC.CONTENT.PADDING_LEFT},
    };
    for (size_t i = 0; i < sizeof(content_pad_left_element) / sizeof(content_pad_left_element[0]); ++i) {
        lv_obj_set_style_pad_left(content_pad_left_element[i].e, content_pad_left_element[i].c,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small content_padding_top_element[] = {
            {ui_pnlContent, 44 + theme.MISC.CONTENT.PADDING_TOP},
            {ui_pnlGlyph,   44 + theme.MISC.CONTENT.PADDING_TOP},
            {ui_pnlHighlight,   44 + theme.MISC.CONTENT.PADDING_TOP},
    };
    for (size_t i = 0; i < sizeof(content_padding_top_element) / sizeof(content_padding_top_element[0]); ++i) {
        lv_obj_set_y(content_padding_top_element[i].e, content_padding_top_element[i].c);
    }

    struct small highlight_pad_right_element[] = {
            {ui_pnlHighlight, theme.MISC.CONTENT.PADDING_LEFT},
    };
    for (size_t i = 0; i < sizeof(highlight_pad_right_element) / sizeof(highlight_pad_right_element[0]); ++i) {
        lv_obj_set_style_pad_right(highlight_pad_right_element[i].e, (highlight_pad_right_element[i].c / 2) + 4,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small datetime_pad_left_element[] = {
            {ui_lblDatetime, theme.DATETIME.PADDING_LEFT},
    };
    for (size_t i = 0; i < sizeof(datetime_pad_left_element) / sizeof(datetime_pad_left_element[0]); ++i) {
        lv_obj_set_style_pad_left(datetime_pad_left_element[i].e, datetime_pad_left_element[i].c,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small status_pad_right_element[] = {
            {ui_conGlyphs, theme.STATUS.PADDING_RIGHT},
    };
    for (size_t i = 0; i < sizeof(status_pad_right_element) / sizeof(status_pad_right_element[0]); ++i) {
        lv_obj_set_style_pad_right(status_pad_right_element[i].e, status_pad_right_element[i].c,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small navigation_alignment_element[] = {
            {ui_pnlFooter, theme.NAV.ALIGNMENT},
    };
    for (size_t i = 0; i < sizeof(navigation_alignment_element) / sizeof(navigation_alignment_element[0]); ++i) {
        lv_flex_align_t e_align;
        switch (navigation_alignment_element[i].c) {
            case 1:
                e_align = LV_FLEX_ALIGN_CENTER;
                break;
            case 2:
                e_align = LV_FLEX_ALIGN_END;
                break;
            case 3:
                e_align = LV_FLEX_ALIGN_SPACE_AROUND;
                break;
            case 4:
                e_align = LV_FLEX_ALIGN_SPACE_BETWEEN;
                break;
            case 5:
                e_align = LV_FLEX_ALIGN_SPACE_EVENLY;
                break;
            default:
                e_align = LV_FLEX_ALIGN_START;
                break;
        }
        lv_obj_set_style_flex_main_place(navigation_alignment_element[i].e, e_align, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
