#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "theme.h"
#include "config.h"
#include "device.h"
#include "log.h"
#include "mini/mini.h"

int load_scheme(const char *theme_base, const char *device_dimension,
                const char *mux_name, char *scheme, size_t scheme_size) {
    return (snprintf(scheme, scheme_size, "%s/%sscheme/%s.txt", theme_base, device_dimension, mux_name)
            && file_exist(scheme)) ||
           (snprintf(scheme, scheme_size, "%s/%sscheme/default.txt", theme_base, device_dimension)
            && file_exist(scheme)) ||
           (snprintf(scheme, scheme_size, "%s/scheme/%s.txt", theme_base, mux_name)
            && file_exist(scheme)) ||
           (snprintf(scheme, scheme_size, "%s/scheme/default.txt", theme_base)
            && file_exist(scheme));
}

void load_theme(struct theme_config *theme, struct mux_config *config, struct mux_device *device, char *mux_name) {
    char scheme[MAX_BUFFER_SIZE];
    char device_dimension[15];
    get_device_dimension(device_dimension, sizeof(device_dimension));

    if (load_scheme(STORAGE_THEME, device_dimension, mux_name, scheme, sizeof(scheme))) {
        LOG_INFO(mux_module, "Loading STORAGE Theme Scheme: %s", scheme)
    } else if (load_scheme(INTERNAL_THEME, device_dimension, mux_name, scheme, sizeof(scheme))) {
        LOG_INFO(mux_module, "Loading INTERNAL Theme Scheme: %s", scheme)
    }

    mini_t *muos_theme = mini_try_load(scheme);

    theme->SYSTEM.BACKGROUND = get_ini_hex(muos_theme, "background", "BACKGROUND");
    theme->SYSTEM.BACKGROUND_ALPHA = get_ini_int(muos_theme, "background", "BACKGROUND_ALPHA", 255);

    theme->ANIMATION.ANIMATION_DELAY = get_ini_int(muos_theme, "animation", "ANIMATION_DELAY", 100);
    if (theme->ANIMATION.ANIMATION_DELAY < 10) theme->ANIMATION.ANIMATION_DELAY = 10;

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
    theme->FONT.LIST_PAD_LEFT = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_LEFT", 32);
    theme->FONT.LIST_PAD_RIGHT = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_RIGHT", 6);
    theme->FONT.LIST_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_TOP", 0);
    theme->FONT.LIST_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_BOTTOM", 0);

    theme->STATUS.ALIGN = get_ini_int(muos_theme, "status", "ALIGN", 1);
    theme->STATUS.PADDING_LEFT = get_ini_int(muos_theme, "status", "PADDING_LEFT", 0);
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
    theme->DATETIME.ALIGN = get_ini_int(muos_theme, "date", "DATETIME_ALIGN", 1);
    theme->DATETIME.PADDING_LEFT = get_ini_int(muos_theme, "date", "PADDING_LEFT", 0);
    theme->DATETIME.PADDING_RIGHT = get_ini_int(muos_theme, "date", "PADDING_RIGHT", 0);

    theme->FOOTER.HEIGHT = get_ini_int(muos_theme, "footer", "FOOTER_HEIGHT", 42);
    theme->FOOTER.BACKGROUND = get_ini_hex(muos_theme, "footer", "FOOTER_BACKGROUND");
    theme->FOOTER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "footer", "FOOTER_BACKGROUND_ALPHA", 255);
    theme->FOOTER.TEXT = get_ini_hex(muos_theme, "footer", "FOOTER_TEXT");
    theme->FOOTER.TEXT_ALPHA = get_ini_int(muos_theme, "footer", "FOOTER_TEXT_ALPHA", 255);

    theme->HEADER.HEIGHT = get_ini_int(muos_theme, "header", "HEADER_HEIGHT", 42);
    theme->HEADER.BACKGROUND = get_ini_hex(muos_theme, "header", "HEADER_BACKGROUND");
    theme->HEADER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "header", "HEADER_BACKGROUND_ALPHA", 255);
    theme->HEADER.TEXT = get_ini_hex(muos_theme, "header", "HEADER_TEXT");
    theme->HEADER.TEXT_ALPHA = get_ini_int(muos_theme, "header", "HEADER_TEXT_ALPHA", 255);
    theme->HEADER.TEXT_ALIGN = get_ini_int(muos_theme, "header", "HEADER_TEXT_ALIGN", 2);
    theme->HEADER.PADDING_LEFT = get_ini_int(muos_theme, "header", "PADDING_LEFT", 0);
    theme->HEADER.PADDING_RIGHT = get_ini_int(muos_theme, "header", "PADDING_RIGHT", 0);

    theme->HELP.BACKGROUND = get_ini_hex(muos_theme, "help", "HELP_BACKGROUND");
    theme->HELP.BACKGROUND_ALPHA = get_ini_int(muos_theme, "help", "HELP_BACKGROUND_ALPHA", 255);
    theme->HELP.BORDER = get_ini_hex(muos_theme, "help", "HELP_BORDER");
    theme->HELP.BORDER_ALPHA = get_ini_int(muos_theme, "help", "HELP_BORDER_ALPHA", 255);
    theme->HELP.CONTENT = get_ini_hex(muos_theme, "help", "HELP_CONTENT");
    theme->HELP.TITLE = get_ini_hex(muos_theme, "help", "HELP_TITLE");
    theme->HELP.RADIUS = get_ini_int(muos_theme, "help", "HELP_RADIUS", 3);

    theme->NAV.ALIGNMENT = get_ini_int(muos_theme, "navigation", "ALIGNMENT", 255);

    theme->NAV.A.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_A_GLYPH");
    theme->NAV.A.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_GLYPH_ALPHA", 255);
    theme->NAV.A.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_GLYPH_RECOLOUR_ALPHA", 255);
    theme->NAV.A.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_A_TEXT");
    theme->NAV.A.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_TEXT_ALPHA", 255);

    theme->NAV.B.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_B_GLYPH");
    theme->NAV.B.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_GLYPH_ALPHA", 255);
    theme->NAV.B.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_GLYPH_RECOLOUR_ALPHA", 255);
    theme->NAV.B.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_B_TEXT");
    theme->NAV.B.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_TEXT_ALPHA", 255);

    theme->NAV.C.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_C_GLYPH");
    theme->NAV.C.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_GLYPH_ALPHA", 255);
    theme->NAV.C.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_GLYPH_RECOLOUR_ALPHA", 255);
    theme->NAV.C.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_C_TEXT");
    theme->NAV.C.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_TEXT_ALPHA", 255);

    theme->NAV.X.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_X_GLYPH");
    theme->NAV.X.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_GLYPH_ALPHA", 255);
    theme->NAV.X.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_GLYPH_RECOLOUR_ALPHA", 255);
    theme->NAV.X.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_X_TEXT");
    theme->NAV.X.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_TEXT_ALPHA", 255);

    theme->NAV.Y.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_Y_GLYPH");
    theme->NAV.Y.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_GLYPH_ALPHA", 255);
    theme->NAV.Y.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_GLYPH_RECOLOUR_ALPHA", 255);
    theme->NAV.Y.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_Y_TEXT");
    theme->NAV.Y.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_TEXT_ALPHA", 255);

    theme->NAV.Z.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_Z_GLYPH");
    theme->NAV.Z.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_GLYPH_ALPHA", 255);
    theme->NAV.Z.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_GLYPH_RECOLOUR_ALPHA", 255);
    theme->NAV.Z.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_Z_TEXT");
    theme->NAV.Z.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_TEXT_ALPHA", 255);

    theme->NAV.MENU.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_MENU_GLYPH");
    theme->NAV.MENU.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_GLYPH_ALPHA", 255);
    theme->NAV.MENU.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_GLYPH_RECOLOUR_ALPHA", 255);
    theme->NAV.MENU.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_MENU_TEXT");
    theme->NAV.MENU.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_TEXT_ALPHA", 255);

    theme->GRID.NAVIGATION_TYPE = get_ini_int(muos_theme, "grid", "NAVIGATION_TYPE", 2);
    theme->GRID.BACKGROUND = get_ini_hex(muos_theme, "grid", "BACKGROUND");
    theme->GRID.BACKGROUND_ALPHA = get_ini_int(muos_theme, "grid", "BACKGROUND_ALPHA", 0);
    theme->GRID.LOCATION_X = get_ini_int(muos_theme, "grid", "LOCATION_X", 0);
    theme->GRID.LOCATION_Y = get_ini_int(muos_theme, "grid", "LOCATION_Y", 0);
    theme->GRID.COLUMN_COUNT = get_ini_int(muos_theme, "grid", "COLUMN_COUNT", 0);
    theme->GRID.ROW_COUNT = get_ini_int(muos_theme, "grid", "ROW_COUNT", 0);
    theme->GRID.ENABLED = (theme->GRID.COLUMN_COUNT > 0 && theme->GRID.ROW_COUNT > 0);

    theme->GRID.ROW_HEIGHT = get_ini_int(muos_theme, "grid", "ROW_HEIGHT", 0);
    theme->GRID.COLUMN_WIDTH = get_ini_int(muos_theme, "grid", "COLUMN_WIDTH", 0);

    theme->GRID.CELL.WIDTH = get_ini_int(muos_theme, "grid", "CELL_WIDTH", 200);
    theme->GRID.CELL.HEIGHT = get_ini_int(muos_theme, "grid", "CELL_HEIGHT", 200);
    theme->GRID.CELL.RADIUS = get_ini_int(muos_theme, "grid", "CELL_RADIUS", 10);
    theme->GRID.CELL.BORDER_WIDTH = get_ini_int(muos_theme, "grid", "CELL_BORDER_WIDTH", 5);
    theme->GRID.CELL.IMAGE_PADDING_TOP = get_ini_int(muos_theme, "grid", "CELL_IMAGE_PADDING_TOP", 5);
    theme->GRID.CELL.TEXT_PADDING_BOTTOM = get_ini_int(muos_theme, "grid", "CELL_TEXT_PADDING_BOTTOM ", 5);
    theme->GRID.CELL.TEXT_PADDING_SIDE = get_ini_int(muos_theme, "grid", "CELL_TEXT_PADDING_SIDE", 5);
    theme->GRID.CELL.TEXT_LINE_SPACING = get_ini_int(muos_theme, "grid", "CELL_TEXT_LINE_SPACING", 0);

    theme->GRID.CELL_DEFAULT.BACKGROUND = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND");
    theme->GRID.CELL_DEFAULT.BACKGROUND_ALPHA = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_ALPHA", 255);
    theme->GRID.CELL_DEFAULT.BORDER = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_BORDER");
    theme->GRID.CELL_DEFAULT.BORDER_ALPHA = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_BORDER_ALPHA", 255);
    theme->GRID.CELL_DEFAULT.IMAGE_ALPHA = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_IMAGE_ALPHA", 255);
    theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_IMAGE_RECOLOUR");
    theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR_ALPHA = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_IMAGE_RECOLOUR_ALPHA", 0);
    theme->GRID.CELL_DEFAULT.TEXT = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_TEXT");
    theme->GRID.CELL_DEFAULT.TEXT_ALPHA = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_TEXT_ALPHA", 255);

    theme->GRID.CELL_FOCUS.BACKGROUND = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_BACKGROUND");
    theme->GRID.CELL_FOCUS.BACKGROUND_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_BACKGROUND_ALPHA", 255);
    theme->GRID.CELL_FOCUS.BORDER = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_BORDER");
    theme->GRID.CELL_FOCUS.BORDER_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_BORDER_ALPHA", 255);
    theme->GRID.CELL_FOCUS.IMAGE_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_IMAGE_ALPHA", 255);
    theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_IMAGE_RECOLOUR");
    theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_IMAGE_RECOLOUR_ALPHA", 0);
    theme->GRID.CELL_FOCUS.TEXT = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_TEXT");
    theme->GRID.CELL_FOCUS.TEXT_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_TEXT_ALPHA", 255);

    theme->LIST_DEFAULT.RADIUS = get_ini_int(muos_theme, "list", "LIST_DEFAULT_RADIUS", 0);
    theme->LIST_DEFAULT.BACKGROUND = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_BACKGROUND");
    theme->LIST_DEFAULT.BACKGROUND_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_BACKGROUND_ALPHA", 255);
    theme->LIST_DEFAULT.GRADIENT_START = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_START", 0);
    theme->LIST_DEFAULT.GRADIENT_STOP = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_STOP", 200);
    theme->LIST_DEFAULT.GRADIENT_DIRECTION = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_DIRECTION", 0);
    theme->LIST_DEFAULT.INDICATOR = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_INDICATOR");
    theme->LIST_DEFAULT.INDICATOR_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_INDICATOR_ALPHA", 255);
    theme->LIST_DEFAULT.TEXT = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_TEXT");
    theme->LIST_DEFAULT.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_TEXT_ALPHA", 255);
    theme->LIST_DEFAULT.BACKGROUND_GRADIENT = (theme->LIST_DEFAULT.GRADIENT_START == 255)
                                              ? theme->LIST_DEFAULT.BACKGROUND : theme->SYSTEM.BACKGROUND;
    theme->LIST_DEFAULT.GLYPH_PADDING_LEFT = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GLYPH_PAD_LEFT", 19);
    theme->LIST_DEFAULT.GLYPH_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GLYPH_ALPHA", 255);
    theme->LIST_DEFAULT.GLYPH_RECOLOUR = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_GLYPH_RECOLOUR");
    theme->LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GLYPH_RECOLOUR_ALPHA", 0);

    theme->LIST_DISABLED.TEXT = get_ini_hex(muos_theme, "list", "LIST_DISABLED_TEXT");
    theme->LIST_DISABLED.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_DISABLED_TEXT_ALPHA", 255);

    theme->LIST_FOCUS.BACKGROUND = get_ini_hex(muos_theme, "list", "LIST_FOCUS_BACKGROUND");
    theme->LIST_FOCUS.BACKGROUND_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_BACKGROUND_ALPHA", 255);
    theme->LIST_FOCUS.GRADIENT_START = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_START", 0);
    theme->LIST_FOCUS.GRADIENT_STOP = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_STOP", 200);
    theme->LIST_FOCUS.GRADIENT_DIRECTION = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_DIRECTION", 0);
    theme->LIST_FOCUS.INDICATOR = get_ini_hex(muos_theme, "list", "LIST_FOCUS_INDICATOR");
    theme->LIST_FOCUS.INDICATOR_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_INDICATOR_ALPHA", 255);
    theme->LIST_FOCUS.TEXT = get_ini_hex(muos_theme, "list", "LIST_FOCUS_TEXT");
    theme->LIST_FOCUS.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_TEXT_ALPHA", 255);
    theme->LIST_FOCUS.BACKGROUND_GRADIENT = (theme->LIST_FOCUS.GRADIENT_START == 255) ? theme->LIST_FOCUS.BACKGROUND
                                                                                      : theme->SYSTEM.BACKGROUND;
    theme->LIST_FOCUS.GLYPH_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_GLYPH_ALPHA", 255);
    theme->LIST_FOCUS.GLYPH_RECOLOUR = get_ini_hex(muos_theme, "list", "LIST_FOCUS_GLYPH_RECOLOUR");
    theme->LIST_FOCUS.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_GLYPH_RECOLOUR_ALPHA", 0);

    theme->IMAGE_LIST.ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_ALPHA", 255);
    theme->IMAGE_LIST.RADIUS = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RADIUS", 3);
    theme->IMAGE_LIST.RECOLOUR = get_ini_hex(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR");
    theme->IMAGE_LIST.RECOLOUR_ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR_ALPHA", 255);
    theme->IMAGE_LIST.PAD_TOP = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_TOP", 0);
    theme->IMAGE_LIST.PAD_BOTTOM = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_BOTTOM", 0);
    theme->IMAGE_LIST.PAD_LEFT = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_LEFT", 0);
    theme->IMAGE_LIST.PAD_RIGHT = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_RIGHT", 0);

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
    theme->OSK.ITEM.BACKGROUND_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_FOCUS_ALPHA",
                                                         255);
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

    theme->BAR.PANEL_WIDTH = get_ini_int(muos_theme, "bar", "BAR_WIDTH", (int16_t) (device->MUX.WIDTH - 25));
    theme->BAR.PANEL_HEIGHT = get_ini_int(muos_theme, "bar", "BAR_HEIGHT", 42);
    theme->BAR.PANEL_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_BACKGROUND");
    theme->BAR.PANEL_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_BACKGROUND_ALPHA", 255);
    theme->BAR.PANEL_BORDER = get_ini_hex(muos_theme, "bar", "BAR_BORDER");
    theme->BAR.PANEL_BORDER_ALPHA = get_ini_int(muos_theme, "bar", "BAR_BORDER_ALPHA", 255);
    theme->BAR.PANEL_BORDER_RADIUS = get_ini_int(muos_theme, "bar", "BAR_RADIUS", 3);
    theme->BAR.PROGRESS_WIDTH = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_WIDTH",
                                            (int16_t) (device->MUX.WIDTH - 90));
    theme->BAR.PROGRESS_HEIGHT = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_HEIGHT", 16);
    theme->BAR.PROGRESS_MAIN_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND");
    theme->BAR.PROGRESS_MAIN_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND_ALPHA", 255);
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND");
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND_ALPHA",
                                                              255);
    theme->BAR.PROGRESS_RADIUS = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_RADIUS", 3);
    theme->BAR.ICON = get_ini_hex(muos_theme, "bar", "BAR_ICON");
    theme->BAR.ICON_ALPHA = get_ini_int(muos_theme, "bar", "BAR_ICON_ALPHA", 255);
    theme->BAR.Y_POS = get_ini_int(muos_theme, "bar", "BAR_Y_POS", (int16_t) (device->MUX.HEIGHT - 96));

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

    theme->COUNTER.ALIGNMENT = get_ini_int(muos_theme, "counter", "COUNTER_ALIGNMENT", 0);
    theme->COUNTER.PADDING_AROUND = get_ini_int(muos_theme, "counter", "COUNTER_PADDING_AROUND", 5);
    theme->COUNTER.PADDING_SIDE = get_ini_int(muos_theme, "counter", "COUNTER_PADDING_SIDE", 15);
    theme->COUNTER.PADDING_TOP = get_ini_int(muos_theme, "counter", "COUNTER_PADDING_TOP", 0);
    theme->COUNTER.BORDER_COLOUR = get_ini_hex(muos_theme, "counter", "COUNTER_BORDER_COLOUR");
    theme->COUNTER.BORDER_ALPHA = get_ini_int(muos_theme, "counter", "COUNTER_BORDER_ALPHA", 0);
    theme->COUNTER.BORDER_WIDTH = get_ini_int(muos_theme, "counter", "COUNTER_BORDER_WIDTH", 2);
    theme->COUNTER.RADIUS = get_ini_int(muos_theme, "counter", "COUNTER_RADIUS", 0);
    theme->COUNTER.BACKGROUND = get_ini_hex(muos_theme, "counter", "COUNTER_BACKGROUND");
    theme->COUNTER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "counter", "COUNTER_BACKGROUND_ALPHA", 0);
    theme->COUNTER.TEXT = get_ini_hex(muos_theme, "counter", "COUNTER_TEXT");
    theme->COUNTER.TEXT_ALPHA = get_ini_int(muos_theme, "counter", "COUNTER_TEXT_ALPHA", 0);
    theme->COUNTER.TEXT_FADE_TIME = get_ini_int(muos_theme, "counter", "COUNTER_TEXT_FADE_TIME", 0);
    strncpy(theme->COUNTER.TEXT_SEPARATOR, get_ini_string(muos_theme, "counter", "COUNTER_TEXT_SEPARATOR", " / "),
            MAX_BUFFER_SIZE - 1);
    theme->COUNTER.TEXT_SEPARATOR[MAX_BUFFER_SIZE - 1] = '\0';

    theme->MISC.STATIC_ALIGNMENT = get_ini_int(muos_theme, "misc", "STATIC_ALIGNMENT", 255);
    theme->MUX.ITEM.COUNT = get_ini_int(muos_theme, "misc", "CONTENT_ITEM_COUNT", 11);
    theme->MUX.ITEM.HEIGHT = get_ini_int(muos_theme, "misc", "CONTENT_ITEM_HEIGHT", 0);
    theme->MISC.CONTENT.SIZE_TO_CONTENT = get_ini_int(muos_theme, "misc", "CONTENT_SIZE_TO_CONTENT", 0);
    theme->MISC.CONTENT.ALIGNMENT = get_ini_int(muos_theme, "misc", "CONTENT_ALIGNMENT", 0);
    theme->MISC.CONTENT.PADDING_LEFT = get_ini_int(muos_theme, "misc", "CONTENT_PADDING_LEFT", 0);
    theme->MISC.CONTENT.PADDING_TOP = get_ini_int(muos_theme, "misc", "CONTENT_PADDING_TOP", 0);
    int default_content_height = device->MUX.HEIGHT - theme->HEADER.HEIGHT - theme->FOOTER.HEIGHT - 4;
    theme->MISC.CONTENT.HEIGHT = get_ini_int(muos_theme, "misc", "CONTENT_HEIGHT", default_content_height);
    theme->MISC.CONTENT.WIDTH = get_ini_int(muos_theme, "misc", "CONTENT_WIDTH", device->MUX.WIDTH);
    theme->MISC.ANIMATED_BACKGROUND = get_ini_int(muos_theme, "misc", "ANIMATED_BACKGROUND", 0);
    theme->MISC.RANDOM_BACKGROUND = get_ini_int(muos_theme, "misc", "RANDOM_BACKGROUND", 0);
    theme->MISC.IMAGE_OVERLAY = get_ini_int(muos_theme, "misc", "IMAGE_OVERLAY", 0);
    theme->MISC.NAVIGATION_TYPE = get_ini_int(muos_theme, "misc", "NAVIGATION_TYPE", 0);

    if (theme->MISC.CONTENT.HEIGHT > device->MUX.HEIGHT) theme->MISC.CONTENT.HEIGHT = device->MUX.HEIGHT;
    if (theme->MUX.ITEM.COUNT < 1) theme->MUX.ITEM.COUNT = 1;
    if (theme->MUX.ITEM.HEIGHT > 0) {
        theme->MUX.ITEM.PANEL = theme->MUX.ITEM.HEIGHT + 2;
        theme->MUX.ITEM.COUNT = theme->MISC.CONTENT.HEIGHT / theme->MUX.ITEM.PANEL;
    } else {
        theme->MUX.ITEM.PANEL = theme->MISC.CONTENT.HEIGHT / theme->MUX.ITEM.COUNT;
        theme->MUX.ITEM.HEIGHT = theme->MUX.ITEM.PANEL - 2;
    }
    // Adjusts height if user picks a height that is not evenly divisible by item count.
    // Prevents seeing a few pixels of the next game.
    theme->MISC.CONTENT.HEIGHT = theme->MUX.ITEM.PANEL * theme->MUX.ITEM.COUNT;

    mini_free(muos_theme);

    char scheme_override[MAX_BUFFER_SIZE];
    snprintf(scheme_override, sizeof(scheme), "%s/theme/override/%s.txt", STORAGE_PATH, mux_name);
    if (file_exist(scheme_override)) {
        mini_t *muos_theme_overrides = mini_try_load(scheme_override);
        int16_t pad_right = get_ini_int(muos_theme_overrides, "font", "FONT_LIST_PAD_RIGHT", -1);
        int16_t list_default_radius = get_ini_int(muos_theme_overrides, "list", "LIST_DEFAULT_RADIUS", -1);
        int16_t content_width = get_ini_int(muos_theme_overrides, "misc", "CONTENT_WIDTH", -1);
        int16_t size_to_content = get_ini_int(muos_theme_overrides, "misc", "CONTENT_SIZE_TO_CONTENT", -1);
        int16_t content_left_padding = get_ini_int(muos_theme_overrides, "misc", "CONTENT_PADDING_LEFT", -1);

        if (pad_right > -1) theme->FONT.LIST_PAD_RIGHT = pad_right;
        if (list_default_radius > -1) theme->LIST_DEFAULT.RADIUS = list_default_radius;
        if (content_width > -1) theme->MISC.CONTENT.WIDTH = content_width;
        if (size_to_content > -1) theme->MISC.CONTENT.SIZE_TO_CONTENT = size_to_content;
        if (content_left_padding > -1) theme->MISC.CONTENT.PADDING_LEFT = content_left_padding;

        mini_free(muos_theme_overrides);
    }
}

void apply_text_long_dot(struct theme_config *theme, lv_obj_t *ui_pnlContent,
                         lv_obj_t *ui_lblItem, const char *item_text) {
    lv_label_set_long_mode(ui_lblItem, LV_LABEL_LONG_WRAP);
    const lv_font_t *font = lv_obj_get_style_text_font(ui_pnlContent, LV_PART_MAIN);
    const lv_coord_t letter_space = lv_obj_get_style_text_letter_space(ui_pnlContent, LV_PART_MAIN);
    lv_coord_t act_line_length = lv_txt_get_width(item_text, strlen(item_text), font, letter_space,
                                                  LV_TEXT_FLAG_EXPAND);
    int max_item_width = theme->MISC.CONTENT.WIDTH - theme->FONT.LIST_PAD_LEFT - theme->FONT.LIST_PAD_RIGHT;

    if (act_line_length > max_item_width) {
        int len = strlen(item_text);
        for (int i = len; i >= 0; i--) {
            char *new_string = (char *) malloc(i + 4);
            strncpy(new_string, item_text, i);
            new_string[i] = '\0';
            strcat(new_string, "...");

            if (max_item_width >=
                lv_txt_get_width(new_string, strlen(new_string), font, letter_space, LV_TEXT_FLAG_EXPAND)) {
                lv_label_set_text(ui_lblItem, new_string);
                free(new_string);
                return;
            }
            free(new_string);
        }
    }
}

void apply_size_to_content(struct theme_config *theme, lv_obj_t *ui_pnlContent, lv_obj_t *ui_lblItem,
                           lv_obj_t *ui_lblItemGlyph, const char *item_text) {
    if (theme->MISC.CONTENT.SIZE_TO_CONTENT) {
        lv_obj_t *ui_pnlItem = lv_obj_get_parent(ui_lblItem);
        lv_obj_set_width(ui_pnlItem, LV_SIZE_CONTENT);
        lv_obj_get_style_max_width(ui_pnlItem, theme->MISC.CONTENT.WIDTH);

        const lv_font_t *font = lv_obj_get_style_text_font(ui_pnlContent, LV_PART_MAIN);
        const lv_coord_t letter_space = lv_obj_get_style_text_letter_space(ui_pnlContent, LV_PART_MAIN);
        lv_coord_t act_line_length = lv_txt_get_width(item_text, strlen(item_text), font, letter_space,
                                                      LV_TEXT_FLAG_EXPAND);
        int item_width = LV_MIN(theme->FONT.LIST_PAD_LEFT + act_line_length + theme->FONT.LIST_PAD_RIGHT,
                                theme->MISC.CONTENT.WIDTH - 10); //-10: compensate for 5 pixel border
        // When using size to content right padding needs to be zero to prevent text from wrapping.
        // The overall width of the control will include the right padding
        lv_obj_set_style_pad_right(ui_lblItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_width(ui_lblItem, item_width);
        lv_obj_set_x(ui_lblItemGlyph, theme->LIST_DEFAULT.GLYPH_PADDING_LEFT - (item_width / 2) -
                                      5); // - 5 at end to compensate for border width
    }
}

void apply_theme_list_panel(struct theme_config *theme, struct mux_device *device, lv_obj_t *ui_pnlList) {
    lv_obj_set_width(ui_pnlList, theme->MISC.CONTENT.WIDTH);
    lv_obj_set_height(ui_pnlList, theme->MUX.ITEM.HEIGHT);
    lv_obj_set_scrollbar_mode(ui_pnlList, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_border_width(ui_pnlList, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlList, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_pnlList, lv_color_hex(theme->LIST_DEFAULT.BACKGROUND_GRADIENT),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_pnlList, lv_color_hex(theme->LIST_FOCUS.BACKGROUND_GRADIENT),
                                   LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_main_stop(ui_pnlList, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_pnlList,
        (theme->LIST_DEFAULT.GRADIENT_DIRECTION == 1) ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_pnlList, lv_color_hex(theme->LIST_DEFAULT.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlList, theme->LIST_DEFAULT.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_pnlList, theme->LIST_DEFAULT.GRADIENT_START,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_pnlList, theme->LIST_DEFAULT.GRADIENT_STOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_pnlList, lv_color_hex(theme->LIST_DEFAULT.INDICATOR),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlList, theme->LIST_DEFAULT.INDICATOR_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlList, lv_color_hex(theme->LIST_FOCUS.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(ui_pnlList, theme->LIST_FOCUS.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_main_stop(ui_pnlList, theme->LIST_FOCUS.GRADIENT_START, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_grad_stop(ui_pnlList, theme->LIST_FOCUS.GRADIENT_STOP, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_grad_dir(ui_pnlList,
        (theme->LIST_FOCUS.GRADIENT_DIRECTION == 1) ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui_pnlList, lv_color_hex(theme->LIST_FOCUS.INDICATOR),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(ui_pnlList, theme->LIST_FOCUS.INDICATOR_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_pad_left(ui_pnlList, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlList, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlList, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlList, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlList, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlList, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_pnlList, theme->LIST_DEFAULT.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void apply_theme_list_item(struct theme_config *theme, lv_obj_t *ui_lblItem, const char *item_text,
                           bool enable_scrolling_text, bool is_config_menu) {
    lv_label_set_text(ui_lblItem, item_text);

    if (enable_scrolling_text) {
        static lv_anim_t item_anim;
        static lv_style_t item_style;
        lv_anim_init(&item_anim);
        lv_anim_set_delay(&item_anim, 250);
        lv_style_init(&item_style);
        lv_style_set_anim(&item_style, &item_anim);
        lv_obj_add_style(ui_lblItem, &item_style, LV_PART_MAIN);
        lv_obj_set_style_anim_speed(ui_lblItem, 70, LV_PART_MAIN);
    }
    lv_label_set_long_mode(ui_lblItem, LV_LABEL_LONG_WRAP);

    lv_obj_set_width(ui_lblItem, theme->MISC.CONTENT.WIDTH - theme->FONT.LIST_PAD_RIGHT);
    const lv_font_t *font = lv_obj_get_style_text_font(ui_lblItem, LV_PART_MAIN);
    lv_coord_t font_height = lv_font_get_line_height(font);
    lv_obj_set_height(ui_lblItem, font_height);
    lv_obj_set_align(ui_lblItem, LV_ALIGN_LEFT_MID);

    lv_obj_set_style_text_color(ui_lblItem, lv_color_hex(theme->LIST_DEFAULT.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblItem, theme->LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblItem, lv_color_hex(theme->LIST_FOCUS.TEXT),
                                LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblItem, theme->LIST_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_pad_left(ui_lblItem, theme->FONT.LIST_PAD_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblItem, theme->FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void apply_theme_list_value(struct theme_config *theme, lv_obj_t *ui_lblItemValue, char *item_text) {
    lv_label_set_text(ui_lblItemValue, item_text);

    lv_obj_set_width(ui_lblItemValue, theme->MISC.CONTENT.WIDTH);
    const lv_font_t *font = lv_obj_get_style_text_font(ui_lblItemValue, LV_PART_MAIN);
    lv_coord_t font_height = lv_font_get_line_height(font);
    lv_obj_set_height(ui_lblItemValue, font_height);
    lv_obj_set_align(ui_lblItemValue, LV_ALIGN_RIGHT_MID);

    lv_obj_set_style_text_color(ui_lblItemValue, lv_color_hex(theme->LIST_DEFAULT.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblItemValue, theme->LIST_DEFAULT.TEXT_ALPHA,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_lblItemValue, lv_color_hex(theme->LIST_FOCUS.TEXT),
                                LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblItemValue, theme->LIST_FOCUS.TEXT_ALPHA,
                              LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_pad_left(ui_lblItemValue, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblItemValue, theme->FONT.LIST_PAD_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblItemValue, theme->FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblItemValue, theme->FONT.LIST_PAD_BOTTOM,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_line_space(ui_lblItemValue, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblItemValue, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(ui_lblItemValue, LV_LABEL_LONG_WRAP);

    lv_obj_set_style_radius(ui_lblItemValue, theme->LIST_DEFAULT.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void apply_theme_list_drop_down(struct theme_config *theme, lv_obj_t *ui_lblItemDropDown, char *options) {
    lv_dropdown_set_dir(ui_lblItemDropDown, LV_DIR_LEFT);
    if (options != NULL) lv_dropdown_set_options(ui_lblItemDropDown, options);
    lv_dropdown_set_selected_highlight(ui_lblItemDropDown, false);
    lv_obj_set_width(ui_lblItemDropDown, theme->MISC.CONTENT.WIDTH);
    const lv_font_t *font = lv_obj_get_style_text_font(ui_lblItemDropDown, LV_PART_MAIN);
    lv_coord_t font_height = lv_font_get_line_height(font);
    lv_obj_set_height(ui_lblItemDropDown, font_height);
    lv_obj_set_align(ui_lblItemDropDown, LV_ALIGN_RIGHT_MID);
    lv_obj_add_flag(ui_lblItemDropDown, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_scrollbar_mode(ui_lblItemDropDown, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lblItemDropDown, LV_DIR_RIGHT);
    lv_obj_set_style_text_color(ui_lblItemDropDown, lv_color_hex(theme->LIST_DEFAULT.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblItemDropDown, theme->LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblItemDropDown, lv_color_hex(theme->LIST_FOCUS.TEXT),
                                LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblItemDropDown, theme->LIST_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(ui_lblItemDropDown, lv_color_hex(0x403A03), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblItemDropDown, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblItemDropDown, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblItemDropDown, theme->FONT.LIST_PAD_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblItemDropDown, theme->FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblItemDropDown, theme->FONT.LIST_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblItemDropDown, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_SCROLLED);
    lv_obj_set_style_text_opa(ui_lblItemDropDown, 255, LV_PART_MAIN | LV_STATE_SCROLLED);
    lv_obj_set_style_text_color(ui_lblItemDropDown, lv_color_hex(0x808080), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblItemDropDown, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lv_dropdown_get_list(ui_lblItemDropDown), lv_color_hex(0x02080D),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lv_dropdown_get_list(ui_lblItemDropDown), 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(lv_dropdown_get_list(ui_lblItemDropDown), lv_color_hex(0xF8E008),
                              LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lv_dropdown_get_list(ui_lblItemDropDown), 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
}

void apply_theme_list_glyph(struct theme_config *theme, lv_obj_t *ui_lblItemGlyph,
                            const char *screen_name, char *item_glyph) {
    if (theme->LIST_DEFAULT.GLYPH_ALPHA == 0 && theme->LIST_FOCUS.GLYPH_ALPHA == 0) return;

    char glyph_image_path[MAX_BUFFER_SIZE];
    char glyph_image_embed[MAX_BUFFER_SIZE];
    char device_dimension[15];
    get_device_dimension(device_dimension, sizeof(device_dimension));
    if ((snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/%sglyph/%s/%s.png",
                  STORAGE_THEME, device_dimension, screen_name, item_glyph) >= 0 && file_exist(glyph_image_path)) ||
        (snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/glyph/%s/%s.png",
                  STORAGE_THEME, screen_name, item_glyph) >= 0 && file_exist(glyph_image_path)) ||
        (snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/%sglyph/%s/%s.png",
                  INTERNAL_THEME, device_dimension, screen_name, item_glyph) >= 0 && file_exist(glyph_image_path)) ||
        (snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/glyph/%s/%s.png",
                  INTERNAL_THEME, screen_name, item_glyph) >= 0 &&
         file_exist(glyph_image_path))) {

        snprintf(glyph_image_embed, sizeof(glyph_image_embed), "M:%s", glyph_image_path);
    }

    if (!file_exist(glyph_image_path)) return;

    lv_img_set_src(ui_lblItemGlyph, glyph_image_embed);

    lv_obj_set_x(ui_lblItemGlyph, theme->LIST_DEFAULT.GLYPH_PADDING_LEFT - (theme->MISC.CONTENT.WIDTH / 2));
    lv_obj_set_align(ui_lblItemGlyph, LV_ALIGN_CENTER);

    lv_obj_set_style_img_opa(ui_lblItemGlyph, theme->LIST_DEFAULT.GLYPH_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui_lblItemGlyph, theme->LIST_FOCUS.GLYPH_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_img_recolor(ui_lblItemGlyph, lv_color_hex(theme->LIST_DEFAULT.GLYPH_RECOLOUR),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_lblItemGlyph, theme->LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor(ui_lblItemGlyph, lv_color_hex(theme->LIST_FOCUS.GLYPH_RECOLOUR),
                                 LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_img_recolor_opa(ui_lblItemGlyph, theme->LIST_FOCUS.GLYPH_RECOLOUR_ALPHA,
                                     LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_pad_top(ui_lblItemGlyph, theme->FONT.LIST_ICON_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblItemGlyph, theme->FONT.LIST_ICON_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
}
