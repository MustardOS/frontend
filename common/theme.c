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
    theme->SYSTEM.BACKGROUND_ALPHA = get_ini_int(muos_theme, "background", "BACKGROUND_ALPHA", 255);

    theme->MUX.ITEM.COUNT = get_ini_int(muos_theme, "mux", "item_count", device->MUX.ITEM.COUNT);
    theme->MUX.ITEM.HEIGHT = get_ini_int(muos_theme, "mux", "item_height", device->MUX.ITEM.HEIGHT);
    theme->MUX.ITEM.PANEL = get_ini_int(muos_theme, "mux", "item_panel", device->MUX.ITEM.PANEL);
    theme->MUX.ITEM.PREV_LOW = get_ini_int(muos_theme, "mux", "item_prev_low", device->MUX.ITEM.PREV_LOW);
    theme->MUX.ITEM.PREV_HIGH = get_ini_int(muos_theme, "mux", "item_prev_high", device->MUX.ITEM.PREV_HIGH);
    theme->MUX.ITEM.NEXT_LOW = get_ini_int(muos_theme, "mux", "item_next_low", device->MUX.ITEM.NEXT_LOW);
    theme->MUX.ITEM.NEXT_HIGH = get_ini_int(muos_theme, "mux", "item_next_high", device->MUX.ITEM.NEXT_HIGH);

    theme->FONT.HEADER_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_HEADER_PAD_TOP", 0);
    theme->FONT.HEADER_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_HEADER_PAD_BOTTOM", 0);
    theme->FONT.HEADER_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_HEADER_ICON_PAD_TOP", 0);
    theme->FONT.HEADER_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_HEADER_ICON_PAD_BOTTOM", 0);
    theme->FONT.FOOTER_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_FOOTER_PAD_TOP", 0);
    theme->FONT.FOOTER_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_FOOTER_PAD_BOTTOM", 0);
    theme->FONT.FOOTER_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_FOOTER_ICON_PAD_TOP", 0);
    theme->FONT.FOOTER_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_FOOTER_ICON_PAD_BOTTOM", 0);
    theme->FONT.MESSAGE_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_MESSAGE_PAD_TOP", 0);
    theme->FONT.MESSAGE_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_MESSAGE_PAD_BOTTOM", 0);
    theme->FONT.MESSAGE_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_MESSAGE_ICON_PAD_TOP", 0);
    theme->FONT.MESSAGE_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_MESSAGE_ICON_PAD_BOTTOM", 0);
    theme->FONT.LIST_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_TOP", 0);
    theme->FONT.LIST_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_BOTTOM", 0);
    theme->FONT.LIST_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_TOP", 0);
    theme->FONT.LIST_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_BOTTOM", 0);

    theme->STATUS.PADDING_RIGHT = get_ini_int(muos_theme, "status", "PADDING_RIGHT", 0);

    theme->STATUS.BATTERY.NORMAL = get_ini_hex(muos_theme, "battery", "BATTERY_NORMAL");
    theme->STATUS.BATTERY.ACTIVE = get_ini_hex(muos_theme, "battery", "BATTERY_ACTIVE");
    theme->STATUS.BATTERY.LOW = get_ini_hex(muos_theme, "battery", "BATTERY_LOW");
    theme->STATUS.BATTERY.NORMAL_ALPHA = get_ini_int(muos_theme, "battery", "BATTERY_NORMAL_ALPHA", 255);
    theme->STATUS.BATTERY.ACTIVE_ALPHA = get_ini_int(muos_theme, "battery", "BATTERY_ACTIVE_ALPHA", 255);
    theme->STATUS.BATTERY.LOW_ALPHA = get_ini_int(muos_theme, "battery", "BATTERY_LOW_ALPHA", 255);

    theme->STATUS.NETWORK.NORMAL = get_ini_hex(muos_theme, "network", "NETWORK_NORMAL");
    theme->STATUS.NETWORK.ACTIVE = get_ini_hex(muos_theme, "network", "NETWORK_ACTIVE");
    theme->STATUS.NETWORK.NORMAL_ALPHA = get_ini_int(muos_theme, "network", "NETWORK_NORMAL_ALPHA", 255);
    theme->STATUS.NETWORK.ACTIVE_ALPHA = get_ini_int(muos_theme, "network", "NETWORK_ACTIVE_ALPHA", 255);

    theme->STATUS.BLUETOOTH.NORMAL = get_ini_hex(muos_theme, "bluetooth", "BLUETOOTH_NORMAL");
    theme->STATUS.BLUETOOTH.ACTIVE = get_ini_hex(muos_theme, "bluetooth", "BLUETOOTH_ACTIVE");
    theme->STATUS.BLUETOOTH.NORMAL_ALPHA = get_ini_int(muos_theme, "bluetooth", "BLUETOOTH_NORMAL_ALPHA", 255);
    theme->STATUS.BLUETOOTH.ACTIVE_ALPHA = get_ini_int(muos_theme, "bluetooth", "BLUETOOTH_ACTIVE_ALPHA", 255);

    theme->DATETIME.TEXT = get_ini_hex(muos_theme, "date", "DATETIME_TEXT");
    theme->DATETIME.ALPHA = get_ini_int(muos_theme, "date", "DATETIME_ALPHA", 255);
    theme->DATETIME.PADDING_LEFT = get_ini_int(muos_theme, "date", "PADDING_LEFT", 0);

    theme->FOOTER.BACKGROUND = get_ini_hex(muos_theme, "footer", "FOOTER_BACKGROUND");
    theme->FOOTER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "footer", "FOOTER_BACKGROUND_ALPHA", 255);
    theme->FOOTER.TEXT = get_ini_hex(muos_theme, "footer", "FOOTER_TEXT");
    theme->FOOTER.TEXT_ALPHA = get_ini_int(muos_theme, "footer", "FOOTER_TEXT_ALPHA", 255);

    theme->HEADER.BACKGROUND = get_ini_hex(muos_theme, "header", "HEADER_BACKGROUND");
    theme->HEADER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "header", "HEADER_BACKGROUND_ALPHA", 255);
    theme->HEADER.TEXT = get_ini_hex(muos_theme, "header", "HEADER_TEXT");
    theme->HEADER.TEXT_ALPHA = get_ini_int(muos_theme, "header", "HEADER_TEXT_ALPHA", 255);

    theme->HELP.BACKGROUND = get_ini_hex(muos_theme, "help", "HELP_BACKGROUND");
    theme->HELP.BACKGROUND_ALPHA = get_ini_int(muos_theme, "help", "HELP_BACKGROUND_ALPHA", 255);
    theme->HELP.BORDER = get_ini_hex(muos_theme, "help", "HELP_BORDER");
    theme->HELP.BORDER_ALPHA = get_ini_int(muos_theme, "help", "HELP_BORDER_ALPHA", 255);
    theme->HELP.CONTENT = get_ini_hex(muos_theme, "help", "HELP_CONTENT");
    theme->HELP.TITLE = get_ini_hex(muos_theme, "help", "HELP_TITLE");
    theme->HELP.RADIUS = get_ini_hex(muos_theme, "help", "HELP_RADIUS");

    theme->NAV.ALIGNMENT = get_ini_int(muos_theme, "navigation", "ALIGNMENT", 255);

    theme->NAV.A.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_A_GLYPH");
    theme->NAV.A.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_GLYPH_ALPHA", 255);
    theme->NAV.A.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_A_TEXT");
    theme->NAV.A.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_TEXT_ALPHA", 255);

    theme->NAV.B.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_B_GLYPH");
    theme->NAV.B.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_GLYPH_ALPHA", 255);
    theme->NAV.B.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_B_TEXT");
    theme->NAV.B.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_TEXT_ALPHA", 255);

    theme->NAV.C.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_C_GLYPH");
    theme->NAV.C.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_GLYPH_ALPHA", 255);
    theme->NAV.C.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_C_TEXT");
    theme->NAV.C.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_TEXT_ALPHA", 255);

    theme->NAV.X.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_X_GLYPH");
    theme->NAV.X.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_GLYPH_ALPHA", 255);
    theme->NAV.X.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_X_TEXT");
    theme->NAV.X.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_TEXT_ALPHA", 255);

    theme->NAV.Y.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_Y_GLYPH");
    theme->NAV.Y.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_GLYPH_ALPHA", 255);
    theme->NAV.Y.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_Y_TEXT");
    theme->NAV.Y.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_TEXT_ALPHA", 255);

    theme->NAV.Z.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_Z_GLYPH");
    theme->NAV.Z.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_GLYPH_ALPHA", 255);
    theme->NAV.Z.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_Z_TEXT");
    theme->NAV.Z.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_TEXT_ALPHA", 255);

    theme->NAV.MENU.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_MENU_GLYPH");
    theme->NAV.MENU.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_GLYPH_ALPHA", 255);
    theme->NAV.MENU.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_MENU_TEXT");
    theme->NAV.MENU.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_TEXT_ALPHA", 255);

    theme->LIST_DEFAULT.RADIUS = get_ini_int(muos_theme, "list", "LIST_DEFAULT_RADIUS", 0);
    theme->LIST_DEFAULT.BACKGROUND = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_BACKGROUND");
    theme->LIST_DEFAULT.BACKGROUND_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_BACKGROUND_ALPHA", 255);
    theme->LIST_DEFAULT.GRADIENT_START = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_START", 0);
    theme->LIST_DEFAULT.GRADIENT_STOP = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_STOP", 200);
    theme->LIST_DEFAULT.INDICATOR = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_INDICATOR");
    theme->LIST_DEFAULT.INDICATOR_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_INDICATOR_ALPHA", 255);
    theme->LIST_DEFAULT.TEXT = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_TEXT");
    theme->LIST_DEFAULT.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_TEXT_ALPHA", 255);

    theme->LIST_DISABLED.TEXT = get_ini_hex(muos_theme, "list", "LIST_DISABLED_TEXT");
    theme->LIST_DISABLED.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_DISABLED_TEXT_ALPHA", 255);

    theme->LIST_FOCUS.BACKGROUND = get_ini_hex(muos_theme, "list", "LIST_FOCUS_BACKGROUND");
    theme->LIST_FOCUS.BACKGROUND_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_BACKGROUND_ALPHA", 255);
    theme->LIST_FOCUS.GRADIENT_START = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_START", 0);
    theme->LIST_FOCUS.GRADIENT_STOP = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_STOP", 200);
    theme->LIST_FOCUS.INDICATOR = get_ini_hex(muos_theme, "list", "LIST_FOCUS_INDICATOR");
    theme->LIST_FOCUS.INDICATOR_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_INDICATOR_ALPHA", 255);
    theme->LIST_FOCUS.TEXT = get_ini_hex(muos_theme, "list", "LIST_FOCUS_TEXT");
    theme->LIST_FOCUS.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_TEXT_ALPHA", 255);

    theme->IMAGE_LIST.ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_ALPHA", 255);
    theme->IMAGE_LIST.RADIUS = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RADIUS", 3);
    theme->IMAGE_LIST.RECOLOUR = get_ini_hex(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR");
    theme->IMAGE_LIST.RECOLOUR_ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR_ALPHA", 255);

    theme->IMAGE_PREVIEW.ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_ALPHA", 255);
    theme->IMAGE_PREVIEW.RADIUS = get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_RADIUS", 3);
    theme->IMAGE_PREVIEW.RECOLOUR = get_ini_hex(muos_theme, "image_list", "IMAGE_PREVIEW_RECOLOUR");
    theme->IMAGE_PREVIEW.RECOLOUR_ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_RECOLOUR_ALPHA", 255);

    theme->CHARGER.BACKGROUND = get_ini_hex(muos_theme, "charging", "CHARGER_BACKGROUND");
    theme->CHARGER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "charging", "CHARGER_BACKGROUND_ALPHA", 255);
    theme->CHARGER.TEXT = get_ini_hex(muos_theme, "charging", "CHARGER_TEXT");
    theme->CHARGER.TEXT_ALPHA = get_ini_int(muos_theme, "charging", "CHARGER_TEXT_ALPHA", 255);
    theme->CHARGER.Y_POS = get_ini_int(muos_theme, "charging", "CHARGER_Y_POS", 0);

    theme->VERBOSE_BOOT.BACKGROUND = get_ini_hex(muos_theme, "verbose", "VERBOSE_BOOT_BACKGROUND");
    theme->VERBOSE_BOOT.BACKGROUND_ALPHA = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_BACKGROUND_ALPHA", 255);
    theme->VERBOSE_BOOT.TEXT = get_ini_hex(muos_theme, "verbose", "VERBOSE_BOOT_TEXT");
    theme->VERBOSE_BOOT.TEXT_ALPHA = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_TEXT_ALPHA", 255);
    theme->VERBOSE_BOOT.Y_POS = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_Y_POS", 0);

    theme->OSK.BACKGROUND = get_ini_hex(muos_theme, "keyboard", "OSK_BACKGROUND");
    theme->OSK.BACKGROUND_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_BACKGROUND_ALPHA", 255);
    theme->OSK.BORDER = get_ini_hex(muos_theme, "keyboard", "OSK_BORDER");
    theme->OSK.BORDER_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_BORDER_ALPHA", 255);
    theme->OSK.RADIUS = get_ini_int(muos_theme, "keyboard", "OSK_RADIUS", 3);
    theme->OSK.TEXT = get_ini_hex(muos_theme, "keyboard", "OSK_TEXT");
    theme->OSK.TEXT_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_TEXT_ALPHA", 255);
    theme->OSK.TEXT_FOCUS = get_ini_hex(muos_theme, "keyboard", "OSK_TEXT_FOCUS");
    theme->OSK.TEXT_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_TEXT_FOCUS_ALPHA", 255);
    theme->OSK.ITEM.BACKGROUND = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND");
    theme->OSK.ITEM.BACKGROUND_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_ALPHA", 255);
    theme->OSK.ITEM.BACKGROUND_FOCUS = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_FOCUS");
    theme->OSK.ITEM.BACKGROUND_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_FOCUS_ALPHA", 255);
    theme->OSK.ITEM.BORDER = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BORDER");
    theme->OSK.ITEM.BORDER_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BORDER_ALPHA", 255);
    theme->OSK.ITEM.BORDER_FOCUS = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BORDER_FOCUS");
    theme->OSK.ITEM.BORDER_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BORDER_FOCUS_ALPHA", 255);
    theme->OSK.ITEM.RADIUS = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_RADIUS", 3);

    theme->MESSAGE.BACKGROUND = get_ini_hex(muos_theme, "notification", "MSG_BACKGROUND");
    theme->MESSAGE.BACKGROUND_ALPHA = get_ini_int(muos_theme, "notification", "MSG_BACKGROUND_ALPHA", 255);
    theme->MESSAGE.BORDER = get_ini_hex(muos_theme, "notification", "MSG_BORDER");
    theme->MESSAGE.BORDER_ALPHA = get_ini_int(muos_theme, "notification", "MSG_BORDER_ALPHA", 255);
    theme->MESSAGE.RADIUS = get_ini_int(muos_theme, "notification", "MSG_RADIUS", 3);
    theme->MESSAGE.TEXT = get_ini_hex(muos_theme, "notification", "MSG_TEXT");
    theme->MESSAGE.TEXT_ALPHA = get_ini_int(muos_theme, "notification", "MSG_TEXT_ALPHA", 255);

    theme->BAR.PANEL_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_BACKGROUND");
    theme->BAR.PANEL_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_BACKGROUND_ALPHA", 255);
    theme->BAR.PANEL_BORDER = get_ini_hex(muos_theme, "bar", "BAR_BORDER");
    theme->BAR.PANEL_BORDER_ALPHA = get_ini_int(muos_theme, "bar", "BAR_BORDER_ALPHA", 255);
    theme->BAR.PANEL_BORDER_RADIUS = get_ini_int(muos_theme, "bar", "BAR_RADIUS", 3);
    theme->BAR.PROGRESS_MAIN_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND");
    theme->BAR.PROGRESS_MAIN_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND_ALPHA", 255);
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND");
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND_ALPHA", 255);
    theme->BAR.PROGRESS_RADIUS = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_RADIUS", 3);
    theme->BAR.ICON = get_ini_hex(muos_theme, "bar", "BAR_ICON");
    theme->BAR.ICON_ALPHA = get_ini_int(muos_theme, "bar", "BAR_ICON_ALPHA", 255);

    theme->ROLL.TEXT = get_ini_hex(muos_theme, "roll", "ROLL_TEXT");
    theme->ROLL.TEXT_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_TEXT_ALPHA", 255);
    theme->ROLL.BACKGROUND = get_ini_hex(muos_theme, "roll", "ROLL_BACKGROUND");
    theme->ROLL.BACKGROUND_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_BACKGROUND_ALPHA", 255);
    theme->ROLL.RADIUS = get_ini_int(muos_theme, "roll", "ROLL_RADIUS", 3);
    theme->ROLL.SELECT_TEXT = get_ini_hex(muos_theme, "roll", "ROLL_SELECT_TEXT");
    theme->ROLL.SELECT_TEXT_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_SELECT_TEXT_ALPHA", 255);
    theme->ROLL.SELECT_BACKGROUND = get_ini_hex(muos_theme, "roll", "ROLL_SELECT_BACKGROUND");
    theme->ROLL.SELECT_BACKGROUND_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_SELECT_BACKGROUND_ALPHA", 255);
    theme->ROLL.SELECT_RADIUS = get_ini_int(muos_theme, "roll", "ROLL_SELECT_RADIUS", 3);
    theme->ROLL.BORDER_COLOUR = get_ini_hex(muos_theme, "roll", "ROLL_BORDER_COLOUR");
    theme->ROLL.BORDER_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_BORDER_ALPHA", 255);
    theme->ROLL.BORDER_RADIUS = get_ini_int(muos_theme, "roll", "ROLL_BORDER_RADIUS", 255);

    theme->MISC.STATIC_ALIGNMENT = get_ini_int(muos_theme, "misc", "STATIC_ALIGNMENT", 255);
    theme->MISC.CONTENT.PADDING_LEFT = get_ini_int(muos_theme, "misc", "CONTENT_PADDING_LEFT", 0);
    theme->MISC.CONTENT.PADDING_TOP = get_ini_int(muos_theme, "misc", "CONTENT_PADDING_TOP", 0);
    theme->MISC.CONTENT.HEIGHT = get_ini_int(muos_theme, "misc", "CONTENT_HEIGHT", 392);
    theme->MISC.CONTENT.WIDTH = get_ini_int(muos_theme, "misc", "CONTENT_WIDTH", device->SCREEN.WIDTH);
    theme->MISC.ANIMATED_BACKGROUND = get_ini_int(muos_theme, "misc", "ANIMATED_BACKGROUND", 255);
    theme->MISC.IMAGE_OVERLAY = get_ini_int(muos_theme, "misc", "IMAGE_OVERLAY", 255);
    theme->MISC.NAVIGATION_TYPE = get_ini_int(muos_theme, "misc", "NAVIGATION_TYPE", 255);

    mini_free(muos_theme);
}
