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
            {ui_scrSysInfo,            theme.SYSTEM.BACKGROUND},
            {ui_pnlFooter,             theme.FOOTER.BACKGROUND},
            {ui_pnlHeader,             theme.HEADER.BACKGROUND},
            {ui_pnlHelpMessage,        theme.HELP.BACKGROUND},
            {ui_lblVersion,            theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblKernel,             theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblUptime,             theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblCPU,                theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblSpeed,              theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblGovernor,           theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblMemory,             theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblTemp,               theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblServices,           theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblBatteryCap,         theme.LIST_DEFAULT.BACKGROUND},
            {ui_lblVoltage,            theme.LIST_DEFAULT.BACKGROUND},
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
            {ui_scrSysInfo,            theme.SYSTEM.BACKGROUND_ALPHA},
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
            {ui_lblVersion,    theme.LIST_FOCUS.BACKGROUND},
            {ui_lblKernel,     theme.LIST_FOCUS.BACKGROUND},
            {ui_lblUptime,     theme.LIST_FOCUS.BACKGROUND},
            {ui_lblCPU,        theme.LIST_FOCUS.BACKGROUND},
            {ui_lblSpeed,      theme.LIST_FOCUS.BACKGROUND},
            {ui_lblGovernor,   theme.LIST_FOCUS.BACKGROUND},
            {ui_lblMemory,     theme.LIST_FOCUS.BACKGROUND},
            {ui_lblTemp,       theme.LIST_FOCUS.BACKGROUND},
            {ui_lblServices,   theme.LIST_FOCUS.BACKGROUND},
            {ui_lblBatteryCap, theme.LIST_FOCUS.BACKGROUND},
            {ui_lblVoltage,    theme.LIST_FOCUS.BACKGROUND},
    };
    for (size_t i = 0; i < sizeof(background_focus_elements) / sizeof(background_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_color(background_focus_elements[i].e, lv_color_hex(background_focus_elements[i].c),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big gradient_elements[] = {
            {ui_lblVersion,    theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblKernel,     theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblUptime,     theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblCPU,        theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblSpeed,      theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblGovernor,   theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblMemory,     theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblTemp,       theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblServices,   theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblBatteryCap, theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
            {ui_lblVoltage,    theme.LIST_DEFAULT.BACKGROUND_GRADIENT},
    };
    for (size_t i = 0; i < sizeof(gradient_elements) / sizeof(gradient_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_color(gradient_elements[i].e, lv_color_hex(gradient_elements[i].c),
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big gradient_focused_elements[] = {
            {ui_lblVersion,    theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblKernel,     theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblUptime,     theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblCPU,        theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblSpeed,      theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblGovernor,   theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblMemory,     theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblTemp,       theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblServices,   theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblBatteryCap, theme.LIST_FOCUS.BACKGROUND_GRADIENT},
            {ui_lblVoltage,    theme.LIST_FOCUS.BACKGROUND_GRADIENT},
    };
    for (size_t i = 0; i < sizeof(gradient_focused_elements) / sizeof(gradient_focused_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_color(gradient_focused_elements[i].e, lv_color_hex(gradient_focused_elements[i].c),
                                       LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big indicator_elements[] = {
            {ui_lblVersion,    theme.LIST_DEFAULT.INDICATOR},
            {ui_lblKernel,     theme.LIST_DEFAULT.INDICATOR},
            {ui_lblUptime,     theme.LIST_DEFAULT.INDICATOR},
            {ui_lblCPU,        theme.LIST_DEFAULT.INDICATOR},
            {ui_lblSpeed,      theme.LIST_DEFAULT.INDICATOR},
            {ui_lblGovernor,   theme.LIST_DEFAULT.INDICATOR},
            {ui_lblMemory,     theme.LIST_DEFAULT.INDICATOR},
            {ui_lblTemp,       theme.LIST_DEFAULT.INDICATOR},
            {ui_lblServices,   theme.LIST_DEFAULT.INDICATOR},
            {ui_lblBatteryCap, theme.LIST_DEFAULT.INDICATOR},
            {ui_lblVoltage,    theme.LIST_DEFAULT.INDICATOR},
    };
    for (size_t i = 0; i < sizeof(indicator_elements) / sizeof(indicator_elements[0]); ++i) {
        lv_obj_set_style_border_color(indicator_elements[i].e, lv_color_hex(indicator_elements[i].c),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct big indicator_focus_elements[] = {
            {ui_lblVersion,    theme.LIST_FOCUS.INDICATOR},
            {ui_lblKernel,     theme.LIST_FOCUS.INDICATOR},
            {ui_lblUptime,     theme.LIST_FOCUS.INDICATOR},
            {ui_lblCPU,        theme.LIST_FOCUS.INDICATOR},
            {ui_lblSpeed,      theme.LIST_FOCUS.INDICATOR},
            {ui_lblGovernor,   theme.LIST_FOCUS.INDICATOR},
            {ui_lblMemory,     theme.LIST_FOCUS.INDICATOR},
            {ui_lblTemp,       theme.LIST_FOCUS.INDICATOR},
            {ui_lblServices,   theme.LIST_FOCUS.INDICATOR},
            {ui_lblBatteryCap, theme.LIST_FOCUS.INDICATOR},
            {ui_lblVoltage,    theme.LIST_FOCUS.INDICATOR},
    };
    for (size_t i = 0; i < sizeof(indicator_focus_elements) / sizeof(indicator_focus_elements[0]); ++i) {
        lv_obj_set_style_border_color(indicator_focus_elements[i].e, lv_color_hex(indicator_focus_elements[i].c),
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct big default_elements[] = {
            {ui_lblVersion,            theme.LIST_DEFAULT.TEXT},
            {ui_lblKernel,             theme.LIST_DEFAULT.TEXT},
            {ui_lblUptime,             theme.LIST_DEFAULT.TEXT},
            {ui_lblCPU,                theme.LIST_DEFAULT.TEXT},
            {ui_lblSpeed,              theme.LIST_DEFAULT.TEXT},
            {ui_lblGovernor,           theme.LIST_DEFAULT.TEXT},
            {ui_lblMemory,             theme.LIST_DEFAULT.TEXT},
            {ui_lblTemp,               theme.LIST_DEFAULT.TEXT},
            {ui_lblServices,           theme.LIST_DEFAULT.TEXT},
            {ui_lblBatteryCap,         theme.LIST_DEFAULT.TEXT},
            {ui_lblVoltage,            theme.LIST_DEFAULT.TEXT},
            {ui_icoVersion,            theme.LIST_DEFAULT.TEXT},
            {ui_icoKernel,             theme.LIST_DEFAULT.TEXT},
            {ui_icoUptime,             theme.LIST_DEFAULT.TEXT},
            {ui_icoCPU,                theme.LIST_DEFAULT.TEXT},
            {ui_icoSpeed,              theme.LIST_DEFAULT.TEXT},
            {ui_icoGovernor,           theme.LIST_DEFAULT.TEXT},
            {ui_icoMemory,             theme.LIST_DEFAULT.TEXT},
            {ui_icoTemp,               theme.LIST_DEFAULT.TEXT},
            {ui_icoServices,           theme.LIST_DEFAULT.TEXT},
            {ui_icoBatteryCap,         theme.LIST_DEFAULT.TEXT},
            {ui_icoVoltage,            theme.LIST_DEFAULT.TEXT},
            {ui_lblVersionValue,       theme.LIST_DEFAULT.TEXT},
            {ui_lblKernelValue,        theme.LIST_DEFAULT.TEXT},
            {ui_lblUptimeValue,        theme.LIST_DEFAULT.TEXT},
            {ui_lblCPUValue,           theme.LIST_DEFAULT.TEXT},
            {ui_lblSpeedValue,         theme.LIST_DEFAULT.TEXT},
            {ui_lblGovernorValue,      theme.LIST_DEFAULT.TEXT},
            {ui_lblMemoryValue,        theme.LIST_DEFAULT.TEXT},
            {ui_lblTempValue,          theme.LIST_DEFAULT.TEXT},
            {ui_lblServicesValue,      theme.LIST_DEFAULT.TEXT},
            {ui_lblBatteryCapValue,    theme.LIST_DEFAULT.TEXT},
            {ui_lblVoltageValue,       theme.LIST_DEFAULT.TEXT},
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
            {ui_lblVersion,         theme.LIST_FOCUS.TEXT},
            {ui_lblKernel,          theme.LIST_FOCUS.TEXT},
            {ui_lblUptime,          theme.LIST_FOCUS.TEXT},
            {ui_lblCPU,             theme.LIST_FOCUS.TEXT},
            {ui_lblSpeed,           theme.LIST_FOCUS.TEXT},
            {ui_lblGovernor,        theme.LIST_FOCUS.TEXT},
            {ui_lblMemory,          theme.LIST_FOCUS.TEXT},
            {ui_lblTemp,            theme.LIST_FOCUS.TEXT},
            {ui_lblServices,        theme.LIST_FOCUS.TEXT},
            {ui_lblBatteryCap,      theme.LIST_FOCUS.TEXT},
            {ui_lblVoltage,         theme.LIST_FOCUS.TEXT},
            {ui_icoVersion,         theme.LIST_FOCUS.TEXT},
            {ui_icoKernel,          theme.LIST_FOCUS.TEXT},
            {ui_icoUptime,          theme.LIST_FOCUS.TEXT},
            {ui_icoCPU,             theme.LIST_FOCUS.TEXT},
            {ui_icoSpeed,           theme.LIST_FOCUS.TEXT},
            {ui_icoGovernor,        theme.LIST_FOCUS.TEXT},
            {ui_icoMemory,          theme.LIST_FOCUS.TEXT},
            {ui_icoTemp,            theme.LIST_FOCUS.TEXT},
            {ui_icoServices,        theme.LIST_FOCUS.TEXT},
            {ui_icoBatteryCap,      theme.LIST_FOCUS.TEXT},
            {ui_icoVoltage,         theme.LIST_FOCUS.TEXT},
            {ui_lblVersionValue,    theme.LIST_FOCUS.TEXT},
            {ui_lblKernelValue,     theme.LIST_FOCUS.TEXT},
            {ui_lblUptimeValue,     theme.LIST_FOCUS.TEXT},
            {ui_lblCPUValue,        theme.LIST_FOCUS.TEXT},
            {ui_lblSpeedValue,      theme.LIST_FOCUS.TEXT},
            {ui_lblGovernorValue,   theme.LIST_FOCUS.TEXT},
            {ui_lblMemoryValue,     theme.LIST_FOCUS.TEXT},
            {ui_lblTempValue,       theme.LIST_FOCUS.TEXT},
            {ui_lblServicesValue,   theme.LIST_FOCUS.TEXT},
            {ui_lblBatteryCapValue, theme.LIST_FOCUS.TEXT},
            {ui_lblVoltageValue,    theme.LIST_FOCUS.TEXT},
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
            {ui_lblVersion,            theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblKernel,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblUptime,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblCPU,                theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblSpeed,              theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblGovernor,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblMemory,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblTemp,               theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblServices,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblBatteryCap,         theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblVoltage,            theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoVersion,            theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoKernel,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoUptime,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoCPU,                theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoSpeed,              theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoGovernor,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoMemory,             theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoTemp,               theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoServices,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoBatteryCap,         theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_icoVoltage,            theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblVersionValue,       theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblKernelValue,        theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblUptimeValue,        theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblCPUValue,           theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblSpeedValue,         theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblGovernorValue,      theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblMemoryValue,        theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblTempValue,          theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblServicesValue,      theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblBatteryCapValue,    theme.LIST_DEFAULT.TEXT_ALPHA},
            {ui_lblVoltageValue,       theme.LIST_DEFAULT.TEXT_ALPHA},
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
            {ui_lblVersion,         theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblKernel,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblUptime,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblCPU,             theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblSpeed,           theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblGovernor,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblMemory,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblTemp,            theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblServices,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblBatteryCap,      theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblVoltage,         theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoVersion,         theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoKernel,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoUptime,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoCPU,             theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoSpeed,           theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoGovernor,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoMemory,          theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoTemp,            theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoServices,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoBatteryCap,      theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_icoVoltage,         theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblVersionValue,    theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblKernelValue,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblUptimeValue,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblCPUValue,        theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblSpeedValue,      theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblGovernorValue,   theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblMemoryValue,     theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblTempValue,       theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblServicesValue,   theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblBatteryCapValue, theme.LIST_FOCUS.TEXT_ALPHA},
            {ui_lblVoltageValue,    theme.LIST_FOCUS.TEXT_ALPHA},
    };
    for (size_t i = 0; i < sizeof(text_focus_alpha_elements) / sizeof(text_focus_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(text_focus_alpha_elements[i].e, text_focus_alpha_elements[i].c,
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small indicator_default_alpha_elements[] = {
            {ui_lblVersion,    theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblKernel,     theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblUptime,     theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblCPU,        theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblSpeed,      theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblGovernor,   theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblMemory,     theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblTemp,       theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblServices,   theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblBatteryCap, theme.LIST_DEFAULT.INDICATOR_ALPHA},
            {ui_lblVoltage,    theme.LIST_DEFAULT.INDICATOR_ALPHA},
    };
    for (size_t i = 0;
         i < sizeof(indicator_default_alpha_elements) / sizeof(indicator_default_alpha_elements[0]); ++i) {
        lv_obj_set_style_border_opa(indicator_default_alpha_elements[i].e, indicator_default_alpha_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small indicator_focus_alpha_elements[] = {
            {ui_lblVersion,    theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblKernel,     theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblUptime,     theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblCPU,        theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblSpeed,      theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblGovernor,   theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblMemory,     theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblTemp,       theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblServices,   theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblBatteryCap, theme.LIST_FOCUS.INDICATOR_ALPHA},
            {ui_lblVoltage,    theme.LIST_FOCUS.INDICATOR_ALPHA},
    };
    for (size_t i = 0; i < sizeof(indicator_focus_alpha_elements) / sizeof(indicator_focus_alpha_elements[0]); ++i) {
        lv_obj_set_style_border_opa(indicator_focus_alpha_elements[i].e, indicator_focus_alpha_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small item_height_elements[] = {
            {ui_lblVersion,         theme.MUX.ITEM.HEIGHT},
            {ui_lblKernel,          theme.MUX.ITEM.HEIGHT},
            {ui_lblUptime,          theme.MUX.ITEM.HEIGHT},
            {ui_lblCPU,             theme.MUX.ITEM.HEIGHT},
            {ui_lblSpeed,           theme.MUX.ITEM.HEIGHT},
            {ui_lblGovernor,        theme.MUX.ITEM.HEIGHT},
            {ui_lblMemory,          theme.MUX.ITEM.HEIGHT},
            {ui_lblTemp,            theme.MUX.ITEM.HEIGHT},
            {ui_lblServices,        theme.MUX.ITEM.HEIGHT},
            {ui_lblBatteryCap,      theme.MUX.ITEM.HEIGHT},
            {ui_lblVoltage,         theme.MUX.ITEM.HEIGHT},
            {ui_icoVersion,         theme.MUX.ITEM.HEIGHT},
            {ui_icoKernel,          theme.MUX.ITEM.HEIGHT},
            {ui_icoUptime,          theme.MUX.ITEM.HEIGHT},
            {ui_icoCPU,             theme.MUX.ITEM.HEIGHT},
            {ui_icoSpeed,           theme.MUX.ITEM.HEIGHT},
            {ui_icoGovernor,        theme.MUX.ITEM.HEIGHT},
            {ui_icoMemory,          theme.MUX.ITEM.HEIGHT},
            {ui_icoTemp,            theme.MUX.ITEM.HEIGHT},
            {ui_icoServices,        theme.MUX.ITEM.HEIGHT},
            {ui_icoBatteryCap,      theme.MUX.ITEM.HEIGHT},
            {ui_icoVoltage,         theme.MUX.ITEM.HEIGHT},
            {ui_lblVersionValue,    theme.MUX.ITEM.HEIGHT},
            {ui_lblKernelValue,     theme.MUX.ITEM.HEIGHT},
            {ui_lblUptimeValue,     theme.MUX.ITEM.HEIGHT},
            {ui_lblCPUValue,        theme.MUX.ITEM.HEIGHT},
            {ui_lblSpeedValue,      theme.MUX.ITEM.HEIGHT},
            {ui_lblGovernorValue,   theme.MUX.ITEM.HEIGHT},
            {ui_lblMemoryValue,     theme.MUX.ITEM.HEIGHT},
            {ui_lblTempValue,       theme.MUX.ITEM.HEIGHT},
            {ui_lblServicesValue,   theme.MUX.ITEM.HEIGHT},
            {ui_lblBatteryCapValue, theme.MUX.ITEM.HEIGHT},
            {ui_lblVoltageValue,    theme.MUX.ITEM.HEIGHT},
            {ui_pnlContent,         theme.MISC.CONTENT.HEIGHT},
            {ui_pnlGlyph,           theme.MISC.CONTENT.HEIGHT},
            {ui_pnlHighlight,       theme.MISC.CONTENT.HEIGHT},
    };
    for (size_t i = 0; i < sizeof(item_height_elements) / sizeof(item_height_elements[0]); ++i) {
        lv_obj_set_height(item_height_elements[i].e, item_height_elements[i].c);
    }

    struct small item_width_elements[] = {
            {ui_lblVersion,         theme.MISC.CONTENT.WIDTH},
            {ui_lblKernel,          theme.MISC.CONTENT.WIDTH},
            {ui_lblUptime,          theme.MISC.CONTENT.WIDTH},
            {ui_lblCPU,             theme.MISC.CONTENT.WIDTH},
            {ui_lblSpeed,           theme.MISC.CONTENT.WIDTH},
            {ui_lblGovernor,        theme.MISC.CONTENT.WIDTH},
            {ui_lblMemory,          theme.MISC.CONTENT.WIDTH},
            {ui_lblTemp,            theme.MISC.CONTENT.WIDTH},
            {ui_lblServices,        theme.MISC.CONTENT.WIDTH},
            {ui_lblBatteryCap,      theme.MISC.CONTENT.WIDTH},
            {ui_lblVoltage,         theme.MISC.CONTENT.WIDTH},
            {ui_icoVersion,         theme.MISC.CONTENT.WIDTH},
            {ui_icoKernel,          theme.MISC.CONTENT.WIDTH},
            {ui_icoUptime,          theme.MISC.CONTENT.WIDTH},
            {ui_icoCPU,             theme.MISC.CONTENT.WIDTH},
            {ui_icoSpeed,           theme.MISC.CONTENT.WIDTH},
            {ui_icoGovernor,        theme.MISC.CONTENT.WIDTH},
            {ui_icoMemory,          theme.MISC.CONTENT.WIDTH},
            {ui_icoTemp,            theme.MISC.CONTENT.WIDTH},
            {ui_icoServices,        theme.MISC.CONTENT.WIDTH},
            {ui_icoBatteryCap,      theme.MISC.CONTENT.WIDTH},
            {ui_icoVoltage,         theme.MISC.CONTENT.WIDTH},
            {ui_lblVersionValue,    theme.MISC.CONTENT.WIDTH},
            {ui_lblKernelValue,     theme.MISC.CONTENT.WIDTH},
            {ui_lblUptimeValue,     theme.MISC.CONTENT.WIDTH},
            {ui_lblCPUValue,        theme.MISC.CONTENT.WIDTH},
            {ui_lblSpeedValue,      theme.MISC.CONTENT.WIDTH},
            {ui_lblGovernorValue,   theme.MISC.CONTENT.WIDTH},
            {ui_lblMemoryValue,     theme.MISC.CONTENT.WIDTH},
            {ui_lblTempValue,       theme.MISC.CONTENT.WIDTH},
            {ui_lblServicesValue,   theme.MISC.CONTENT.WIDTH},
            {ui_lblBatteryCapValue, theme.MISC.CONTENT.WIDTH},
            {ui_lblVoltageValue,    theme.MISC.CONTENT.WIDTH},
    };
    for (size_t i = 0; i < sizeof(item_width_elements) / sizeof(item_width_elements[0]); ++i) {
        lv_obj_set_width(item_width_elements[i].e, item_width_elements[i].c);
    }

    struct small gradient_start_default_elements[] = {
            {ui_lblVersion,    theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblKernel,     theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblUptime,     theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblCPU,        theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblSpeed,      theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblGovernor,   theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblMemory,     theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblTemp,       theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblServices,   theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblBatteryCap, theme.LIST_DEFAULT.GRADIENT_START},
            {ui_lblVoltage,    theme.LIST_DEFAULT.GRADIENT_START},
    };
    for (size_t i = 0; i < sizeof(gradient_start_default_elements) / sizeof(gradient_start_default_elements[0]); ++i) {
        lv_obj_set_style_bg_main_stop(gradient_start_default_elements[i].e, gradient_start_default_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small gradient_start_focus_elements[] = {
            {ui_lblVersion,    theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblKernel,     theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblUptime,     theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblCPU,        theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblSpeed,      theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblGovernor,   theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblMemory,     theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblTemp,       theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblServices,   theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblBatteryCap, theme.LIST_FOCUS.GRADIENT_START},
            {ui_lblVoltage,    theme.LIST_FOCUS.GRADIENT_START},
    };
    for (size_t i = 0; i < sizeof(gradient_start_focus_elements) / sizeof(gradient_start_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_main_stop(gradient_start_focus_elements[i].e, gradient_start_focus_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small gradient_stop_default_elements[] = {
            {ui_lblVersion,    theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblKernel,     theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblUptime,     theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblCPU,        theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblSpeed,      theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblGovernor,   theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblMemory,     theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblTemp,       theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblServices,   theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblBatteryCap, theme.LIST_DEFAULT.GRADIENT_STOP},
            {ui_lblVoltage,    theme.LIST_DEFAULT.GRADIENT_STOP},
    };
    for (size_t i = 0; i < sizeof(gradient_stop_default_elements) / sizeof(gradient_stop_default_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_stop(gradient_stop_default_elements[i].e, gradient_stop_default_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small gradient_stop_focus_elements[] = {
            {ui_lblVersion,    theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblKernel,     theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblUptime,     theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblCPU,        theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblSpeed,      theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblGovernor,   theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblMemory,     theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblTemp,       theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblServices,   theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblBatteryCap, theme.LIST_FOCUS.GRADIENT_STOP},
            {ui_lblVoltage,    theme.LIST_FOCUS.GRADIENT_STOP},
    };
    for (size_t i = 0; i < sizeof(gradient_stop_focus_elements) / sizeof(gradient_stop_focus_elements[0]); ++i) {
        lv_obj_set_style_bg_grad_stop(gradient_stop_focus_elements[i].e, gradient_stop_focus_elements[i].c,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct small background_alpha_default_elements[] = {
            {ui_lblVersion,    theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblKernel,     theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblUptime,     theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblCPU,        theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblSpeed,      theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblGovernor,   theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblMemory,     theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblTemp,       theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblServices,   theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblBatteryCap, theme.LIST_DEFAULT.BACKGROUND_ALPHA},
            {ui_lblVoltage,    theme.LIST_DEFAULT.BACKGROUND_ALPHA},
    };
    for (size_t i = 0;
         i < sizeof(background_alpha_default_elements) / sizeof(background_alpha_default_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(background_alpha_default_elements[i].e, background_alpha_default_elements[i].c,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small background_alpha_focus_elements[] = {
            {ui_lblVersion,    theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblKernel,     theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblUptime,     theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblCPU,        theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblSpeed,      theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblGovernor,   theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblMemory,     theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblTemp,       theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblServices,   theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblBatteryCap, theme.LIST_FOCUS.BACKGROUND_ALPHA},
            {ui_lblVoltage,    theme.LIST_FOCUS.BACKGROUND_ALPHA},
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
            {ui_lblVersion,    theme.LIST_DEFAULT.RADIUS},
            {ui_lblKernel,     theme.LIST_DEFAULT.RADIUS},
            {ui_lblUptime,     theme.LIST_DEFAULT.RADIUS},
            {ui_lblCPU,        theme.LIST_DEFAULT.RADIUS},
            {ui_lblSpeed,      theme.LIST_DEFAULT.RADIUS},
            {ui_lblGovernor,   theme.LIST_DEFAULT.RADIUS},
            {ui_lblMemory,     theme.LIST_DEFAULT.RADIUS},
            {ui_lblTemp,       theme.LIST_DEFAULT.RADIUS},
            {ui_lblServices,   theme.LIST_DEFAULT.RADIUS},
            {ui_lblBatteryCap, theme.LIST_DEFAULT.RADIUS},
            {ui_lblVoltage,    theme.LIST_DEFAULT.RADIUS},
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
            {ui_lblVersion,    theme.FONT.LIST_PAD_TOP},
            {ui_lblKernel,     theme.FONT.LIST_PAD_TOP},
            {ui_lblUptime,     theme.FONT.LIST_PAD_TOP},
            {ui_lblCPU,        theme.FONT.LIST_PAD_TOP},
            {ui_lblSpeed,      theme.FONT.LIST_PAD_TOP},
            {ui_lblGovernor,   theme.FONT.LIST_PAD_TOP},
            {ui_lblMemory,     theme.FONT.LIST_PAD_TOP},
            {ui_lblTemp,       theme.FONT.LIST_PAD_TOP},
            {ui_lblServices,   theme.FONT.LIST_PAD_TOP},
            {ui_lblBatteryCap, theme.FONT.LIST_PAD_TOP},
            {ui_lblVoltage,    theme.FONT.LIST_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_list_top_elements) / sizeof(font_pad_list_top_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_list_top_elements[i].e, font_pad_list_top_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_list_bottom_elements[] = {
            {ui_lblVersion,    theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblKernel,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblUptime,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblCPU,        theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblSpeed,      theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblGovernor,   theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblMemory,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblTemp,       theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblServices,   theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblBatteryCap, theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblVoltage,    theme.FONT.LIST_PAD_BOTTOM},
    };
    for (size_t i = 0; i < sizeof(font_pad_list_bottom_elements) / sizeof(font_pad_list_bottom_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_list_bottom_elements[i].e, font_pad_list_bottom_elements[i].c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_dropdown_elements[] = {
            {ui_lblVersionValue,    theme.FONT.LIST_PAD_TOP},
            {ui_lblKernelValue,     theme.FONT.LIST_PAD_TOP},
            {ui_lblUptimeValue,     theme.FONT.LIST_PAD_TOP},
            {ui_lblCPUValue,        theme.FONT.LIST_PAD_TOP},
            {ui_lblSpeedValue,      theme.FONT.LIST_PAD_TOP},
            {ui_lblGovernorValue,   theme.FONT.LIST_PAD_TOP},
            {ui_lblMemoryValue,     theme.FONT.LIST_PAD_TOP},
            {ui_lblTempValue,       theme.FONT.LIST_PAD_TOP},
            {ui_lblServicesValue,   theme.FONT.LIST_PAD_TOP},
            {ui_lblBatteryCapValue, theme.FONT.LIST_PAD_TOP},
            {ui_lblVoltageValue,    theme.FONT.LIST_PAD_TOP},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_top_dropdown_elements) / sizeof(font_pad_top_dropdown_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_dropdown_elements[i].e, font_pad_top_dropdown_elements[i].c + 5,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_dropdown_elements[] = {
            {ui_lblVersionValue,    theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblKernelValue,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblUptimeValue,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblCPUValue,        theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblSpeedValue,      theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblGovernorValue,   theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblMemoryValue,     theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblTempValue,       theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblServicesValue,   theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblBatteryCapValue, theme.FONT.LIST_PAD_BOTTOM},
            {ui_lblVoltageValue,    theme.FONT.LIST_PAD_BOTTOM},
    };
    for (size_t i = 0;
         i < sizeof(font_pad_bottom_dropdown_elements) / sizeof(font_pad_bottom_dropdown_elements[0]); ++i) {
        lv_obj_set_style_pad_bottom(font_pad_bottom_dropdown_elements[i].e,
                                    font_pad_bottom_dropdown_elements[i].c + 5,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_top_list_icon_elements[] = {
            {ui_icoVersion,    theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoKernel,     theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoUptime,     theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoCPU,        theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoSpeed,      theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoGovernor,   theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoMemory,     theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoTemp,       theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoServices,   theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoBatteryCap, theme.FONT.LIST_ICON_PAD_TOP},
            {ui_icoVoltage,    theme.FONT.LIST_ICON_PAD_TOP},
    };
    for (size_t i = 0; i < sizeof(font_pad_top_list_icon_elements) / sizeof(font_pad_top_list_icon_elements[0]); ++i) {
        lv_obj_set_style_pad_top(font_pad_top_list_icon_elements[i].e, font_pad_top_list_icon_elements[i].c,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct small font_pad_bottom_list_icon_elements[] = {
            {ui_icoVersion,    theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoKernel,     theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoUptime,     theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoCPU,        theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoSpeed,      theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoGovernor,   theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoMemory,     theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoTemp,       theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoServices,   theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoBatteryCap, theme.FONT.LIST_ICON_PAD_BOTTOM},
            {ui_icoVoltage,    theme.FONT.LIST_ICON_PAD_BOTTOM},
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
