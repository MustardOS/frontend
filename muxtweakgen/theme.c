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
            {ui_scrTweakGeneral,       theme.SYSTEM.BACKGROUND},
            {ui_pnlFooter,             theme.FOOTER.BACKGROUND},
            {ui_pnlHeader,             theme.HEADER.BACKGROUND},
            {ui_pnlHelpMessage,        theme.HELP.BACKGROUND},
            {ui_lblHidden,             theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblBGM,                theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblStartup,            theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblColour,             theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblBrightness,         theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblHDMI,               theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblShutdown,           theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblInterface,          theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblStorage,            theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblAdvanced,           theme.LIST_DEFAULT.BACKGROUND},
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
            {ui_scrTweakGeneral,       theme.SYSTEM.BACKGROUND_ALPHA},
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
            {ui_barProgressVolume,     theme.BAR.PROGRESS_ACTIVE_BACKGROUND},
    };
    for (size_t i = 0; i < sizeof(progress_elements) / sizeof(progress_elements[0]); ++i) {
        lv_obj_set_style_bg_color(progress_elements[i].e, lv_color_hex(progress_elements[i].c),
                                  LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }

    struct small progress_alpha_elements[] = {
            {ui_barProgressBrightness, theme.BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA},
            {ui_barProgressVolume,     theme.BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA},
    };
    for (size_t i = 0; i < sizeof(progress_alpha_elements) / sizeof(progress_alpha_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(progress_alpha_elements[i].e, progress_alpha_elements[i].c,
                                LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }

    struct big background_focus_elements[] = {
            {ui_lblHidden,     theme.LIST_FOCUS.BACKGROUND},
            {ui_lblBGM,        theme.LIST_FOCUS.BACKGROUND},
            {ui_lblStartup,    theme.LIST_FOCUS.BACKGROUND},
            {ui_lblColour,     theme.LIST_FOCUS.BACKGROUND},
            {ui_lblBrightness, theme.LIST_FOCUS.BACKGROUND},
            {ui_lblHDMI,       theme.LIST_FOCUS.BACKGROUND},
            {ui_lblShutdown,   theme.LIST_FOCUS.BACKGROUND},
            {ui_lblInterface,  theme.LIST_FOCUS.BACKGROUND},
            {ui_lblStorage,    theme.LIST_FOCUS.BACKGROUND},
            {ui_lblAdvanced,   theme.LIST_FOCUS.BACKGROUND},

    };
    for (size_t i = 0; i < sizeof(background_focus_elements) / sizeof(background_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_color(background_focus_elements[i].e, lv_color_hex(background_focus_elements[i].c),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big gradient_elements[] = {
            {ui_lblHidden,     theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblBGM,        theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblStartup,    theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblColour,     theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblBrightness, theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblHDMI,       theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblShutdown,   theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblInterface,  theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblStorage,    theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblAdvanced,   theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
    };
    for (size_t i = 0; i < sizeof(gradient_elements) / sizeof(gradient_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_color(gradient_elements[i].e, lv_color_hex(gradient_elements[i].c),
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big gradient_focused_elements[] = {
            {ui_lblHidden,     theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblBGM,        theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblStartup,    theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblColour,     theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblBrightness, theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblHDMI,       theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblShutdown,   theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblInterface,  theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblStorage,    theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblAdvanced,   theme.LIST_FOCUS.BACKGROUND_GRADIENT},
    };
    for (size_t i = 0; i < sizeof(gradient_focused_elements) / sizeof(gradient_focused_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_color(gradient_focused_elements[i].e, lv_color_hex(gradient_focused_elements[i].c),
                                       LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big indicator_elements[] = {
            {ui_lblHidden,     theme.LIST_DEFAULT.INDICATOR},
            {ui_lblBGM,        theme.LIST_DEFAULT.INDICATOR},
            {ui_lblStartup,    theme.LIST_DEFAULT.INDICATOR},
            {ui_lblColour,     theme.LIST_DEFAULT.INDICATOR},
            {ui_lblBrightness, theme.LIST_DEFAULT.INDICATOR},
            {ui_lblHDMI,       theme.LIST_DEFAULT.INDICATOR},
            {ui_lblShutdown,   theme.LIST_DEFAULT.INDICATOR},
            {ui_lblInterface,  theme.LIST_DEFAULT.INDICATOR},
            {ui_lblStorage,    theme.LIST_DEFAULT.INDICATOR},
            {ui_lblAdvanced,   theme.LIST_DEFAULT.INDICATOR},
    };
    for (size_t i = 0; i < sizeof(indicator_elements) / sizeof(indicator_elements[0]); ++i) {
        lv_obj_set_style_border_color(indicator_elements[i].e, lv_color_hex(indicator_elements[i].c),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big indicator_focus_elements[] = {
            {ui_lblHidden,     theme.LIST_FOCUS.INDICATOR},
            {ui_lblBGM,        theme.LIST_FOCUS.INDICATOR},
            {ui_lblStartup,    theme.LIST_FOCUS.INDICATOR},
            {ui_lblColour,     theme.LIST_FOCUS.INDICATOR},
            {ui_lblBrightness, theme.LIST_FOCUS.INDICATOR},
            {ui_lblHDMI,       theme.LIST_FOCUS.INDICATOR},
            {ui_lblShutdown,   theme.LIST_FOCUS.INDICATOR},
            {ui_lblInterface,  theme.LIST_FOCUS.INDICATOR},
            {ui_lblStorage,    theme.LIST_FOCUS.INDICATOR},
            {ui_lblAdvanced,   theme.LIST_FOCUS.INDICATOR},
    };
    for (size_t i = 0; i < sizeof(indicator_focus_elements) / sizeof(indicator_focus_elements[0]); ++i) {
        lv_obj_set_style_border_color(indicator_focus_elements[i].e, lv_color_hex(indicator_focus_elements[i].c),
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big default_elements[] = {
            {ui_lblHidden,             theme.LIST_DEFAULT.TEXT},
            {ui_lblBGM,                theme.LIST_DEFAULT.TEXT},
            {ui_lblStartup,            theme.LIST_DEFAULT.TEXT},
            {ui_lblColour,             theme.LIST_DEFAULT.TEXT},
            {ui_lblBrightness,         theme.LIST_DEFAULT.TEXT},
            {ui_lblHDMI,               theme.LIST_DEFAULT.TEXT},
            {ui_lblShutdown,           theme.LIST_DEFAULT.TEXT},
            {ui_lblInterface,          theme.LIST_DEFAULT.TEXT},
            {ui_lblStorage,            theme.LIST_DEFAULT.TEXT},
            {ui_lblAdvanced,           theme.LIST_DEFAULT.TEXT},
            {ui_icoHidden,             theme.LIST_DEFAULT.TEXT},
            {ui_icoBGM,                theme.LIST_DEFAULT.TEXT},
            {ui_icoStartup,            theme.LIST_DEFAULT.TEXT},
            {ui_icoColour,             theme.LIST_DEFAULT.TEXT},
            {ui_icoBrightness,         theme.LIST_DEFAULT.TEXT},
            {ui_icoHDMI,               theme.LIST_DEFAULT.TEXT},
            {ui_icoShutdown,           theme.LIST_DEFAULT.TEXT},
            {ui_icoInterface,          theme.LIST_DEFAULT.TEXT},
            {ui_icoStorage,            theme.LIST_DEFAULT.TEXT},
            {ui_icoAdvanced,           theme.LIST_DEFAULT.TEXT},
            {ui_droHidden,             theme.LIST_DEFAULT.TEXT},
            {ui_droBGM,                theme.LIST_DEFAULT.TEXT},
            {ui_droStartup,            theme.LIST_DEFAULT.TEXT},
            {ui_droColour,             theme.LIST_DEFAULT.TEXT},
            {ui_droBrightness,         theme.LIST_DEFAULT.TEXT},
            {ui_droHDMI,               theme.LIST_DEFAULT.TEXT},
            {ui_droShutdown,           theme.LIST_DEFAULT.TEXT},
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
            {ui_lblHidden,     theme.LIST_FOCUS.TEXT},
            {ui_lblBGM,        theme.LIST_FOCUS.TEXT},
            {ui_lblStartup,    theme.LIST_FOCUS.TEXT},
            {ui_lblColour,     theme.LIST_FOCUS.TEXT},
            {ui_lblBrightness, theme.LIST_FOCUS.TEXT},
            {ui_lblHDMI,       theme.LIST_FOCUS.TEXT},
            {ui_lblShutdown,   theme.LIST_FOCUS.TEXT},
            {ui_lblInterface,  theme.LIST_FOCUS.TEXT},
            {ui_lblStorage,    theme.LIST_FOCUS.TEXT},
            {ui_lblAdvanced,   theme.LIST_FOCUS.TEXT},
            {ui_icoHidden,     theme.LIST_FOCUS.TEXT},
            {ui_icoBGM,        theme.LIST_FOCUS.TEXT},
            {ui_icoStartup,    theme.LIST_FOCUS.TEXT},
            {ui_icoColour,     theme.LIST_FOCUS.TEXT},
            {ui_icoBrightness, theme.LIST_FOCUS.TEXT},
            {ui_icoHDMI,       theme.LIST_FOCUS.TEXT},
            {ui_icoShutdown,   theme.LIST_FOCUS.TEXT},
            {ui_icoInterface,  theme.LIST_FOCUS.TEXT},
            {ui_icoStorage,    theme.LIST_FOCUS.TEXT},
            {ui_icoAdvanced,   theme.LIST_FOCUS.TEXT},
            {ui_droHidden,     theme.LIST_FOCUS.TEXT},
            {ui_droBGM,        theme.LIST_FOCUS.TEXT},
            {ui_droStartup,    theme.LIST_FOCUS.TEXT},
            {ui_droColour,     theme.LIST_FOCUS.TEXT},
            {ui_droBrightness, theme.LIST_FOCUS.TEXT},
            {ui_droHDMI,       theme.LIST_FOCUS.TEXT},
            {ui_droShutdown,   theme.LIST_FOCUS.TEXT},
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
            {ui_lblHidden,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblBGM,                theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblStartup,            theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblColour,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblBrightness,         theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblHDMI,               theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblShutdown,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblInterface,          theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblStorage,            theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblAdvanced,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoHidden,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoBGM,                theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoStartup,            theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoColour,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoBrightness,         theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoHDMI,               theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoShutdown,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoInterface,          theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoStorage,            theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoAdvanced,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_droHidden,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_droBGM,                theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_droStartup,            theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_droColour,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_droBrightness,         theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_droHDMI,               theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_droShutdown,           theme.LIST_DEFAULT.TEXT_ALPHA},
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
            {ui_lblHidden,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblBGM,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblStartup,    theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblColour,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblBrightness, theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblHDMI,       theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblShutdown,   theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblInterface,  theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblStorage,    theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblAdvanced,   theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoHidden,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoBGM,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoStartup,    theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoColour,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoBrightness, theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoHDMI,       theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoShutdown,   theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoInterface,  theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoStorage,    theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoAdvanced,   theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_droHidden,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_droBGM,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_droStartup,    theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_droColour,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_droBrightness, theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_droHDMI,       theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_droShutdown,   theme.LIST_FOCUS.TEXT_ALPHA},
    };
    for (size_t i = 0; i < sizeof(text_focus_alpha_elements) / sizeof(text_focus_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(text_focus_alpha_elements[i].e, text_focus_alpha_elements[i].c,
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small indicator_default_alpha_elements[] = {
            {ui_lblHidden,     theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblBGM,        theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblStartup,    theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblColour,     theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblBrightness, theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblHDMI,       theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblShutdown,   theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblInterface,  theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblStorage,    theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblAdvanced,   theme.LIST_DEFAULT.INDICATOR_ALPHA},
    };
    for (size_t i = 0;
         i < sizeof(indicator_default_alpha_elements) / sizeof(indicator_default_alpha_elements[0]); ++i) {
        lv_obj_set_style_border_opa(indicator_default_alpha_elements[i].e, indicator_default_alpha_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small indicator_focus_alpha_elements[] = {
            {ui_lblHidden,     theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblBGM,        theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblStartup,    theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblColour,     theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblBrightness, theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblHDMI,       theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblShutdown,   theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblInterface,  theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblStorage,    theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblAdvanced,   theme.LIST_FOCUS.INDICATOR_ALPHA},
    };
    for (size_t i = 0; i < sizeof(indicator_focus_alpha_elements) / sizeof(indicator_focus_alpha_elements[0]); ++i) {
        lv_obj_set_style_border_opa(indicator_focus_alpha_elements[i].e, indicator_focus_alpha_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small item_height_elements[] = {
            {ui_lblHidden,     theme.MUX.ITEM.HEIGHT},
            {ui_lblBGM,        theme.MUX.ITEM.HEIGHT},
            {ui_lblStartup,    theme.MUX.ITEM.HEIGHT},
            {ui_lblColour,     theme.MUX.ITEM.HEIGHT},
            {ui_lblBrightness, theme.MUX.ITEM.HEIGHT},
            {ui_lblHDMI,       theme.MUX.ITEM.HEIGHT},
            {ui_lblShutdown,   theme.MUX.ITEM.HEIGHT},
            {ui_lblInterface,  theme.MUX.ITEM.HEIGHT},
            {ui_lblStorage,    theme.MUX.ITEM.HEIGHT},
            {ui_lblAdvanced,   theme.MUX.ITEM.HEIGHT},
            {ui_icoHidden,     theme.MUX.ITEM.HEIGHT},
            {ui_icoBGM,        theme.MUX.ITEM.HEIGHT},
            {ui_icoStartup,    theme.MUX.ITEM.HEIGHT},
            {ui_icoColour,     theme.MUX.ITEM.HEIGHT},
            {ui_icoBrightness, theme.MUX.ITEM.HEIGHT},
            {ui_icoHDMI,       theme.MUX.ITEM.HEIGHT},
            {ui_icoShutdown,   theme.MUX.ITEM.HEIGHT},
            {ui_icoInterface,  theme.MUX.ITEM.HEIGHT},
            {ui_icoStorage,    theme.MUX.ITEM.HEIGHT},
            {ui_icoAdvanced,   theme.MUX.ITEM.HEIGHT},
            {ui_droHidden,     theme.MUX.ITEM.HEIGHT},
            {ui_droBGM,        theme.MUX.ITEM.HEIGHT},
            {ui_droStartup,    theme.MUX.ITEM.HEIGHT},
            {ui_droColour,     theme.MUX.ITEM.HEIGHT},
            {ui_droBrightness, theme.MUX.ITEM.HEIGHT},
            {ui_droHDMI,       theme.MUX.ITEM.HEIGHT},
            {ui_droShutdown,   theme.MUX.ITEM.HEIGHT},
            {ui_droInterface,  theme.MUX.ITEM.HEIGHT},
            {ui_droStorage,    theme.MUX.ITEM.HEIGHT},
            {ui_droAdvanced,   theme.MUX.ITEM.HEIGHT},
            {ui_pnlContent,    theme.MISC.CONTENT.HEIGHT},
            {ui_pnlGlyph,      theme.MISC.CONTENT.HEIGHT},
            {ui_pnlHighlight,  theme.MISC.CONTENT.HEIGHT},
    };
    for (size_t i = 0; i < sizeof(item_height_elements) / sizeof(item_height_elements[0]); ++i) {
        lv_obj_set_height(item_height_elements[i].e, item_height_elements[i].c);
    }

    struct small item_width_elements[] = {
            {ui_lblHidden,     theme.MISC.CONTENT.WIDTH},
            {ui_lblBGM,        theme.MISC.CONTENT.WIDTH},
            {ui_lblStartup,    theme.MISC.CONTENT.WIDTH},
            {ui_lblColour,     theme.MISC.CONTENT.WIDTH},
            {ui_lblBrightness, theme.MISC.CONTENT.WIDTH},
            {ui_lblHDMI,       theme.MISC.CONTENT.WIDTH},
            {ui_lblShutdown,   theme.MISC.CONTENT.WIDTH},
            {ui_lblInterface,  theme.MISC.CONTENT.WIDTH},
            {ui_lblStorage,    theme.MISC.CONTENT.WIDTH},
            {ui_lblAdvanced,   theme.MISC.CONTENT.WIDTH},
            {ui_icoHidden,     theme.MISC.CONTENT.WIDTH},
            {ui_icoBGM,        theme.MISC.CONTENT.WIDTH},
            {ui_icoStartup,    theme.MISC.CONTENT.WIDTH},
            {ui_icoColour,     theme.MISC.CONTENT.WIDTH},
            {ui_icoBrightness, theme.MISC.CONTENT.WIDTH},
            {ui_icoHDMI,       theme.MISC.CONTENT.WIDTH},
            {ui_icoShutdown,   theme.MISC.CONTENT.WIDTH},
            {ui_icoInterface,  theme.MISC.CONTENT.WIDTH},
            {ui_icoStorage,    theme.MISC.CONTENT.WIDTH},
            {ui_icoAdvanced,   theme.MISC.CONTENT.WIDTH},
            {ui_droHidden,     theme.MISC.CONTENT.WIDTH},
            {ui_droBGM,        theme.MISC.CONTENT.WIDTH},
            {ui_droStartup,    theme.MISC.CONTENT.WIDTH},
            {ui_droColour,     theme.MISC.CONTENT.WIDTH},
            {ui_droBrightness, theme.MISC.CONTENT.WIDTH},
            {ui_droHDMI,       theme.MISC.CONTENT.WIDTH},
            {ui_droShutdown,   theme.MISC.CONTENT.WIDTH},
    };
    for (size_t i = 0; i < sizeof(item_width_elements) / sizeof(item_width_elements[0]); ++i) {
        lv_obj_set_width(item_width_elements[i].e, item_width_elements[i].c);
    }

    struct small gradient_start_default_elements[] = {
            {ui_lblHidden,     theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblBGM,        theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblStartup,    theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblColour,     theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblBrightness, theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblHDMI,       theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblShutdown,   theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblInterface,  theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblStorage,    theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblAdvanced,   theme.LIST_DEFAULT.GRADIENT_START},
    };
    for (size_t i = 0; i < sizeof(gradient_start_default_elements) / sizeof(gradient_start_default_elements[0]); ++i) {
        lv_obj_set_style_bg_main_stop(gradient_start_default_elements[i].e, gradient_start_default_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small gradient_start_focus_elements[] = {
            {ui_lblHidden,     theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblBGM,        theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblStartup,    theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblColour,     theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblBrightness, theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblHDMI,       theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblShutdown,   theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblInterface,  theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblStorage,    theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblAdvanced,   theme.LIST_FOCUS.GRADIENT_START},
    };
    for (size_t i = 0; i < sizeof(gradient_start_focus_elements) / sizeof(gradient_start_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_main_stop(gradient_start_focus_elements[i].e, gradient_start_focus_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small gradient_stop_default_elements[] = {
            {ui_lblHidden,     theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblBGM,        theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblStartup,    theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblColour,     theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblBrightness, theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblHDMI,       theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblShutdown,   theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblInterface,  theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblStorage,    theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblAdvanced,   theme.LIST_DEFAULT.GRADIENT_STOP},
    };
    for (size_t i = 0; i < sizeof(gradient_stop_default_elements) / sizeof(gradient_stop_default_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_stop(gradient_stop_default_elements[i].e, gradient_stop_default_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small gradient_stop_focus_elements[] = {
            {ui_lblHidden,     theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblBGM,        theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblStartup,    theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblColour,     theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblBrightness, theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblHDMI,       theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblShutdown,   theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblInterface,  theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblStorage,    theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblAdvanced,   theme.LIST_FOCUS.GRADIENT_STOP},
    };
    for (size_t i = 0; i < sizeof(gradient_stop_focus_elements) / sizeof(gradient_stop_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_stop(gradient_stop_focus_elements[i].e, gradient_stop_focus_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small background_alpha_default_elements[] = {
            {ui_lblHidden,     theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblBGM,        theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblStartup,    theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblColour,     theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblBrightness, theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblHDMI,       theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblShutdown,   theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblInterface,  theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblStorage,    theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblAdvanced,   theme.LIST_DEFAULT.BACKGROUND_ALPHA},
    };
    for (size_t i = 0;
         i < sizeof(background_alpha_default_elements) / sizeof(background_alpha_default_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(background_alpha_default_elements[i].e, background_alpha_default_elements[i].c,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small background_alpha_focus_elements[] = {
            {ui_lblHidden,     theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblBGM,        theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblStartup,    theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblColour,     theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblBrightness, theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblHDMI,       theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblShutdown,   theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblInterface,  theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblStorage,    theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblAdvanced,   theme.LIST_FOCUS.BACKGROUND_ALPHA},
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
            {ui_lblHidden,     theme.LIST_DEFAULT.RADIUS},
            {ui_lblBGM,        theme.LIST_DEFAULT.RADIUS},
            {ui_lblStartup,    theme.LIST_DEFAULT.RADIUS},
            {ui_lblColour,     theme.LIST_DEFAULT.RADIUS},
            {ui_lblBrightness, theme.LIST_DEFAULT.RADIUS},
            {ui_lblHDMI,       theme.LIST_DEFAULT.RADIUS},
            {ui_lblShutdown,   theme.LIST_DEFAULT.RADIUS},
            {ui_lblInterface,  theme.LIST_DEFAULT.RADIUS},
            {ui_lblStorage,    theme.LIST_DEFAULT.RADIUS},
            {ui_lblAdvanced,   theme.LIST_DEFAULT.RADIUS},
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
            {ui_lblHidden,     theme.FONT.LIST_PAD_TOP},
            {ui_lblBGM,        theme.FONT.LIST_PAD_TOP},
            {ui_lblStartup,    theme.FONT.LIST_PAD_TOP},
            {ui_lblColour,     theme.FONT.LIST_PAD_TOP},
            {ui_lblBrightness, theme.FONT.LIST_PAD_TOP},
            {ui_lblHDMI,       theme.FONT.LIST_PAD_TOP},
            {ui_lblShutdown,   theme.FONT.LIST_PAD_TOP},
            {ui_lblInterface,  theme.FONT.LIST_PAD_TOP},
            {ui_lblStorage,    theme.FONT.LIST_PAD_TOP},
            {ui_lblAdvanced,   theme.FONT.LIST_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_list_top_elements) / sizeof(font_pad_list_top_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_list_top_elements[i].e, font_pad_list_top_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_list_bottom_elements[] = {
            {ui_lblHidden,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblBGM,        theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblStartup,    theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblColour,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblBrightness, theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblHDMI,       theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblShutdown,   theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblInterface,  theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblStorage,    theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblAdvanced,   theme.FONT.LIST_PAD_BOTTOM},
    };
    for (size_t i = 0; i < sizeof(font_pad_list_bottom_elements) / sizeof(font_pad_list_bottom_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_list_bottom_elements[i].e, font_pad_list_bottom_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_dropdown_elements[] = {
            {ui_droHidden,     theme.FONT.LIST_PAD_TOP},
            {ui_droBGM,        theme.FONT.LIST_PAD_TOP},
            {ui_droStartup,    theme.FONT.LIST_PAD_TOP},
            {ui_droColour,     theme.FONT.LIST_PAD_TOP},
            {ui_droBrightness, theme.FONT.LIST_PAD_TOP},
            {ui_droHDMI,       theme.FONT.LIST_PAD_TOP},
            {ui_droShutdown,   theme.FONT.LIST_PAD_TOP},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_top_dropdown_elements) / sizeof(font_pad_top_dropdown_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_dropdown_elements[i].e, font_pad_top_dropdown_elements[i].c + 5,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_dropdown_elements[] = {
            {ui_droHidden,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_droBGM,        theme.FONT.LIST_PAD_BOTTOM},
            {ui_droStartup,    theme.FONT.LIST_PAD_BOTTOM},
            {ui_droColour,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_droBrightness, theme.FONT.LIST_PAD_BOTTOM},
            {ui_droHDMI,       theme.FONT.LIST_PAD_BOTTOM},
            {ui_droShutdown,   theme.FONT.LIST_PAD_BOTTOM},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_bottom_dropdown_elements) / sizeof(font_pad_bottom_dropdown_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_dropdown_elements[i].e,
                                    font_pad_bottom_dropdown_elements[i].c + 5,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_list_icon_elements[] = {
            {ui_icoHidden,     theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoBGM,        theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoStartup,    theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoColour,     theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoBrightness, theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoHDMI,       theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoShutdown,   theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoInterface,  theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoStorage,    theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoAdvanced,   theme.FONT.LIST_ICON_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_top_list_icon_elements) / sizeof(font_pad_top_list_icon_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_list_icon_elements[i].e, font_pad_top_list_icon_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_list_icon_elements[] = {
            {ui_icoHidden,     theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoBGM,        theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoStartup,    theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoColour,     theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoBrightness, theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoHDMI,       theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoShutdown,   theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoInterface,  theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoStorage,    theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoAdvanced,   theme.FONT.LIST_ICON_PAD_BOTTOM},
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
