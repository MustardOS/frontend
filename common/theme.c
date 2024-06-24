#include <stdio.h>
#include "common.h"
#include "options.h"
#include "theme.h"
#include "config.h"
#include "device.h"
#include "mini/mini.h"

void load_theme(struct theme_config *theme, struct mux_config *config, struct mux_device *device, char *mux_name) {
    char scheme[MAX_BUFFER_SIZE];

    if (config->BOOT.FACTORY_RESET) {
        snprintf(scheme, sizeof(scheme), "%s/theme/scheme/default.txt", INTERNAL_PATH);
    } else {
        snprintf(scheme, sizeof(scheme), "%s/MUOS/theme/active/scheme/%s.txt", device->STORAGE.ROM.MOUNT, mux_name);
        if (!file_exist(scheme)) {
            snprintf(scheme, sizeof(scheme), "%s/MUOS/theme/active/scheme/default.txt", device->STORAGE.ROM.MOUNT);
        }
    }

    mini_t * muos_theme = mini_try_load(scheme);

    theme->SYSTEM.BACKGROUND = get_ini_hex(muos_theme, "background", "BACKGROUND");
    theme->SYSTEM.BACKGROUND_ALPHA = get_ini_int(muos_theme, "background", "BACKGROUND_ALPHA", IGNORE);

    theme->FONT.HEADER_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_HEADER_PAD_TOP", LABEL);
    theme->FONT.HEADER_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_HEADER_PAD_BOTTOM", LABEL);
    theme->FONT.HEADER_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_HEADER_ICON_PAD_TOP", LABEL);
    theme->FONT.HEADER_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_HEADER_ICON_PAD_BOTTOM", LABEL);
    theme->FONT.FOOTER_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_FOOTER_PAD_TOP", LABEL);
    theme->FONT.FOOTER_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_FOOTER_PAD_BOTTOM", LABEL);
    theme->FONT.FOOTER_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_FOOTER_ICON_PAD_TOP", LABEL);
    theme->FONT.FOOTER_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_FOOTER_ICON_PAD_BOTTOM", LABEL);
    theme->FONT.MESSAGE_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_MESSAGE_PAD_TOP", LABEL);
    theme->FONT.MESSAGE_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_MESSAGE_PAD_BOTTOM", LABEL);
    theme->FONT.MESSAGE_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_MESSAGE_ICON_PAD_TOP", LABEL);
    theme->FONT.MESSAGE_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_MESSAGE_ICON_PAD_BOTTOM", LABEL);
    theme->FONT.LIST_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_TOP", LABEL);
    theme->FONT.LIST_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_BOTTOM", LABEL);
    theme->FONT.LIST_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_TOP", LABEL);
    theme->FONT.LIST_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_BOTTOM", LABEL);

    theme->STATUS.PADDING_RIGHT = get_ini_int(muos_theme, "status", "PADDING_RIGHT", VALUE);

    theme->STATUS.BATTERY.NORMAL = get_ini_hex(muos_theme, "battery", "BATTERY_NORMAL");
    theme->STATUS.BATTERY.ACTIVE = get_ini_hex(muos_theme, "battery", "BATTERY_ACTIVE");
    theme->STATUS.BATTERY.LOW = get_ini_hex(muos_theme, "battery", "BATTERY_LOW");
    theme->STATUS.BATTERY.NORMAL_ALPHA = get_ini_int(muos_theme, "battery", "BATTERY_NORMAL_ALPHA", VALUE);
    theme->STATUS.BATTERY.ACTIVE_ALPHA = get_ini_int(muos_theme, "battery", "BATTERY_ACTIVE_ALPHA", VALUE);
    theme->STATUS.BATTERY.LOW_ALPHA = get_ini_int(muos_theme, "battery", "BATTERY_LOW_ALPHA", VALUE);

    theme->STATUS.NETWORK.NORMAL = get_ini_hex(muos_theme, "network", "NETWORK_NORMAL");
    theme->STATUS.NETWORK.ACTIVE = get_ini_hex(muos_theme, "network", "NETWORK_ACTIVE");
    theme->STATUS.NETWORK.NORMAL_ALPHA = get_ini_int(muos_theme, "network", "NETWORK_NORMAL_ALPHA", VALUE);
    theme->STATUS.NETWORK.ACTIVE_ALPHA = get_ini_int(muos_theme, "network", "NETWORK_ACTIVE_ALPHA", VALUE);

    theme->STATUS.BLUETOOTH.NORMAL = get_ini_hex(muos_theme, "bluetooth", "BLUETOOTH_NORMAL");
    theme->STATUS.BLUETOOTH.ACTIVE = get_ini_hex(muos_theme, "bluetooth", "BLUETOOTH_ACTIVE");
    theme->STATUS.BLUETOOTH.NORMAL_ALPHA = get_ini_int(muos_theme, "bluetooth", "BLUETOOTH_NORMAL_ALPHA", VALUE);
    theme->STATUS.BLUETOOTH.ACTIVE_ALPHA = get_ini_int(muos_theme, "bluetooth", "BLUETOOTH_ACTIVE_ALPHA", VALUE);

    theme->DATETIME.TEXT = get_ini_hex(muos_theme, "date", "DATETIME_TEXT");
    theme->DATETIME.ALPHA = get_ini_int(muos_theme, "date", "DATETIME_ALPHA", VALUE);
    theme->DATETIME.PADDING_LEFT = get_ini_int(muos_theme, "date", "PADDING_LEFT", VALUE);

    theme->FOOTER.BACKGROUND = get_ini_hex(muos_theme, "footer", "FOOTER_BACKGROUND");
    theme->FOOTER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "footer", "FOOTER_BACKGROUND_ALPHA", IGNORE);
    theme->FOOTER.TEXT = get_ini_hex(muos_theme, "footer", "FOOTER_TEXT");
    theme->FOOTER.TEXT_ALPHA = get_ini_int(muos_theme, "footer", "FOOTER_TEXT_ALPHA", VALUE);

    theme->HEADER.BACKGROUND = get_ini_hex(muos_theme, "header", "HEADER_BACKGROUND");
    theme->HEADER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "header", "HEADER_BACKGROUND_ALPHA", IGNORE);
    theme->HEADER.TEXT = get_ini_hex(muos_theme, "header", "HEADER_TEXT");
    theme->HEADER.TEXT_ALPHA = get_ini_int(muos_theme, "header", "HEADER_TEXT_ALPHA", VALUE);

    theme->HELP.BACKGROUND = get_ini_hex(muos_theme, "help", "HELP_BACKGROUND");
    theme->HELP.BACKGROUND_ALPHA = get_ini_int(muos_theme, "help", "HELP_BACKGROUND_ALPHA", IGNORE);
    theme->HELP.BORDER = get_ini_hex(muos_theme, "help", "HELP_BORDER");
    theme->HELP.BORDER_ALPHA = get_ini_int(muos_theme, "help", "HELP_BORDER_ALPHA", IGNORE);
    theme->HELP.CONTENT = get_ini_hex(muos_theme, "help", "HELP_CONTENT");
    theme->HELP.TITLE = get_ini_hex(muos_theme, "help", "HELP_TITLE");
    theme->HELP.RADIUS = get_ini_hex(muos_theme, "help", "HELP_RADIUS");

    theme->NAV.ALIGNMENT = get_ini_int(muos_theme, "navigation", "ALIGNMENT", VALUE);

    theme->NAV.A.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_A_GLYPH");
    theme->NAV.A.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_GLYPH_ALPHA", VALUE);
    theme->NAV.A.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_A_TEXT");
    theme->NAV.A.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_TEXT_ALPHA", VALUE);

    theme->NAV.B.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_B_GLYPH");
    theme->NAV.B.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_GLYPH_ALPHA", VALUE);
    theme->NAV.B.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_B_TEXT");
    theme->NAV.B.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_TEXT_ALPHA", VALUE);

    theme->NAV.C.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_C_GLYPH");
    theme->NAV.C.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_GLYPH_ALPHA", VALUE);
    theme->NAV.C.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_C_TEXT");
    theme->NAV.C.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_TEXT_ALPHA", VALUE);

    theme->NAV.X.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_X_GLYPH");
    theme->NAV.X.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_GLYPH_ALPHA", VALUE);
    theme->NAV.X.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_X_TEXT");
    theme->NAV.X.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_TEXT_ALPHA", VALUE);

    theme->NAV.Y.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_Y_GLYPH");
    theme->NAV.Y.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_GLYPH_ALPHA", VALUE);
    theme->NAV.Y.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_Y_TEXT");
    theme->NAV.Y.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_TEXT_ALPHA", VALUE);

    theme->NAV.Z.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_Z_GLYPH");
    theme->NAV.Z.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_GLYPH_ALPHA", VALUE);
    theme->NAV.Z.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_Z_TEXT");
    theme->NAV.Z.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_TEXT_ALPHA", VALUE);

    theme->NAV.MENU.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_MENU_GLYPH");
    theme->NAV.MENU.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_GLYPH_ALPHA", VALUE);
    theme->NAV.MENU.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_MENU_TEXT");
    theme->NAV.MENU.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_TEXT_ALPHA", VALUE);

    theme->LIST_DEFAULT.BACKGROUND = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_BACKGROUND");
    theme->LIST_DEFAULT.BACKGROUND_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_BACKGROUND_ALPHA", IGNORE);
    theme->LIST_DEFAULT.GRADIENT_START = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_START", IGNORE);
    theme->LIST_DEFAULT.GRADIENT_STOP = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_STOP", IGNORE);
    theme->LIST_DEFAULT.INDICATOR = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_INDICATOR");
    theme->LIST_DEFAULT.INDICATOR_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_INDICATOR_ALPHA", VALUE);
    theme->LIST_DEFAULT.TEXT = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_TEXT");
    theme->LIST_DEFAULT.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_TEXT_ALPHA", VALUE);

    theme->LIST_DISABLED.TEXT = get_ini_hex(muos_theme, "list", "LIST_DISABLED_TEXT");
    theme->LIST_DISABLED.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_DISABLED_TEXT_ALPHA", VALUE);

    theme->LIST_FOCUS.BACKGROUND = get_ini_hex(muos_theme, "list", "LIST_FOCUS_BACKGROUND");
    theme->LIST_FOCUS.BACKGROUND_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_BACKGROUND_ALPHA", IGNORE);
    theme->LIST_FOCUS.GRADIENT_START = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_START", IGNORE);
    theme->LIST_FOCUS.GRADIENT_STOP = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_STOP", IGNORE);
    theme->LIST_FOCUS.INDICATOR = get_ini_hex(muos_theme, "list", "LIST_FOCUS_INDICATOR");
    theme->LIST_FOCUS.INDICATOR_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_INDICATOR_ALPHA", IGNORE);
    theme->LIST_FOCUS.TEXT = get_ini_hex(muos_theme, "list", "LIST_FOCUS_TEXT");
    theme->LIST_FOCUS.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_TEXT_ALPHA", VALUE);

    theme->IMAGE_LIST.RADIUS = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RADIUS", IGNORE);
    theme->IMAGE_LIST.RECOLOUR = get_ini_hex(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR");
    theme->IMAGE_LIST.RECOLOUR_ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR_ALPHA", IGNORE);

    theme->IMAGE_PREVIEW.RADIUS = get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_RADIUS", IGNORE);
    theme->IMAGE_PREVIEW.RECOLOUR = get_ini_hex(muos_theme, "image_list", "IMAGE_PREVIEW_RECOLOUR");
    theme->IMAGE_PREVIEW.RECOLOUR_ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_RECOLOUR_ALPHA", IGNORE);

    theme->CHARGER.BACKGROUND = get_ini_hex(muos_theme, "charging", "CHARGER_BACKGROUND");
    theme->CHARGER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "charging", "CHARGER_BACKGROUND_ALPHA", IGNORE);
    theme->CHARGER.TEXT = get_ini_hex(muos_theme, "charging", "CHARGER_TEXT");
    theme->CHARGER.TEXT_ALPHA = get_ini_int(muos_theme, "charging", "CHARGER_TEXT_ALPHA", VALUE);
    theme->CHARGER.Y_POS = get_ini_int(muos_theme, "charging", "CHARGER_Y_POS", LABEL);

    theme->VERBOSE_BOOT.BACKGROUND = get_ini_hex(muos_theme, "verbose", "VERBOSE_BOOT_BACKGROUND");
    theme->VERBOSE_BOOT.BACKGROUND_ALPHA = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_BACKGROUND_ALPHA", IGNORE);
    theme->VERBOSE_BOOT.TEXT = get_ini_hex(muos_theme, "verbose", "VERBOSE_BOOT_TEXT");
    theme->VERBOSE_BOOT.TEXT_ALPHA = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_TEXT_ALPHA", VALUE);
    theme->VERBOSE_BOOT.Y_POS = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_Y_POS", LABEL);

    theme->OSK.BACKGROUND = get_ini_hex(muos_theme, "keyboard", "OSK_BACKGROUND");
    theme->OSK.BACKGROUND_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_BACKGROUND_ALPHA", IGNORE);
    theme->OSK.BORDER = get_ini_hex(muos_theme, "keyboard", "OSK_BORDER");
    theme->OSK.BORDER_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_BORDER_ALPHA", IGNORE);
    theme->OSK.RADIUS = get_ini_int(muos_theme, "keyboard", "OSK_RADIUS", IGNORE);
    theme->OSK.TEXT = get_ini_hex(muos_theme, "keyboard", "OSK_TEXT");
    theme->OSK.TEXT_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_TEXT_ALPHA", VALUE);
    theme->OSK.TEXT_FOCUS = get_ini_hex(muos_theme, "keyboard", "OSK_TEXT_FOCUS");
    theme->OSK.TEXT_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_TEXT_FOCUS_ALPHA", VALUE);
    theme->OSK.ITEM.BACKGROUND = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND");
    theme->OSK.ITEM.BACKGROUND_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_ALPHA", IGNORE);
    theme->OSK.ITEM.BACKGROUND_FOCUS = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_FOCUS");
    theme->OSK.ITEM.BACKGROUND_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_FOCUS_ALPHA", IGNORE);
    theme->OSK.ITEM.BORDER = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BORDER");
    theme->OSK.ITEM.BORDER_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BORDER_ALPHA", IGNORE);
    theme->OSK.ITEM.BORDER_FOCUS = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BORDER_FOCUS");
    theme->OSK.ITEM.BORDER_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BORDER_FOCUS_ALPHA", IGNORE);
    theme->OSK.ITEM.RADIUS = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_RADIUS", IGNORE);

    theme->MESSAGE.BACKGROUND = get_ini_hex(muos_theme, "notification", "MSG_BACKGROUND");
    theme->MESSAGE.BACKGROUND_ALPHA = get_ini_int(muos_theme, "notification", "MSG_BACKGROUND_ALPHA", IGNORE);
    theme->MESSAGE.BORDER = get_ini_hex(muos_theme, "notification", "MSG_BORDER");
    theme->MESSAGE.BORDER_ALPHA = get_ini_int(muos_theme, "notification", "MSG_BORDER_ALPHA", IGNORE);
    theme->MESSAGE.RADIUS = get_ini_int(muos_theme, "notification", "MSG_RADIUS", IGNORE);
    theme->MESSAGE.TEXT = get_ini_hex(muos_theme, "notification", "MSG_TEXT");
    theme->MESSAGE.TEXT_ALPHA = get_ini_int(muos_theme, "notification", "MSG_TEXT_ALPHA", VALUE);

    theme->BAR.PANEL_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_BACKGROUND");
    theme->BAR.PANEL_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_BACKGROUND_ALPHA", IGNORE);
    theme->BAR.PANEL_BORDER = get_ini_hex(muos_theme, "bar", "BAR_BORDER");
    theme->BAR.PANEL_BORDER_ALPHA = get_ini_int(muos_theme, "bar", "BAR_BORDER_ALPHA", IGNORE);
    theme->BAR.PANEL_BORDER_RADIUS = get_ini_int(muos_theme, "bar", "BAR_RADIUS", IGNORE);
    theme->BAR.PROGRESS_MAIN_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND");
    theme->BAR.PROGRESS_MAIN_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND_ALPHA", IGNORE);
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND");
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND_ALPHA", IGNORE);
    theme->BAR.PROGRESS_RADIUS = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_RADIUS", IGNORE);
    theme->BAR.ICON = get_ini_hex(muos_theme, "bar", "BAR_ICON");
    theme->BAR.ICON_ALPHA = get_ini_int(muos_theme, "bar", "BAR_ICON_ALPHA", VALUE);

    theme->ROLL.TEXT = get_ini_hex(muos_theme, "roll", "ROLL_TEXT");
    theme->ROLL.TEXT_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_TEXT_ALPHA", IGNORE);
    theme->ROLL.BACKGROUND = get_ini_hex(muos_theme, "roll", "ROLL_BACKGROUND");
    theme->ROLL.BACKGROUND_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_BACKGROUND_ALPHA", IGNORE);
    theme->ROLL.RADIUS = get_ini_int(muos_theme, "roll", "ROLL_RADIUS", IGNORE);
    theme->ROLL.SELECT_TEXT = get_ini_hex(muos_theme, "roll", "ROLL_SELECT_TEXT");
    theme->ROLL.SELECT_TEXT_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_SELECT_TEXT_ALPHA", IGNORE);
    theme->ROLL.SELECT_BACKGROUND = get_ini_hex(muos_theme, "roll", "ROLL_SELECT_BACKGROUND");
    theme->ROLL.SELECT_BACKGROUND_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_SELECT_BACKGROUND_ALPHA", IGNORE);
    theme->ROLL.SELECT_RADIUS = get_ini_int(muos_theme, "roll", "ROLL_SELECT_RADIUS", IGNORE);
    theme->ROLL.BORDER_COLOUR = get_ini_hex(muos_theme, "roll", "ROLL_BORDER_COLOUR");
    theme->ROLL.BORDER_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_BORDER_ALPHA", VALUE);
    theme->ROLL.BORDER_RADIUS = get_ini_int(muos_theme, "roll", "ROLL_BORDER_RADIUS", VALUE);

    theme->MISC.STATIC_ALIGNMENT = get_ini_int(muos_theme, "misc", "STATIC_ALIGNMENT", VALUE);
    theme->MISC.CONTENT.PADDING_LEFT = get_ini_int(muos_theme, "misc", "CONTENT_PADDING_LEFT", VALUE);
    theme->MISC.CONTENT.WIDTH = get_ini_int(muos_theme, "misc", "CONTENT_WIDTH", VALUE);
    theme->MISC.ANIMATED_BACKGROUND = get_ini_int(muos_theme, "misc", "ANIMATED_BACKGROUND", VALUE);
    theme->MISC.IMAGE_OVERLAY = get_ini_int(muos_theme, "misc", "IMAGE_OVERLAY", VALUE);
    theme->MISC.NAVIGATION_TYPE = get_ini_int(muos_theme, "misc", "NAVIGATION_TYPE", VALUE);

    mini_free(muos_theme);
}
