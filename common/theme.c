#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "theme.h"
#include "config.h"
#include "device.h"
#include "log.h"
#include "mini/mini.h"

static lv_style_t style_list_panel_default;
static lv_style_t style_list_panel_focused;

static lv_anim_t style_list_item_animation;
static lv_style_t style_list_item_default;
static lv_style_t style_list_item_focused;

static lv_style_t style_list_glyph_default;
static lv_style_t style_list_glyph_focused;

int load_scheme(const char *theme_base, const char *mux_dimension, const char *file_name, char *scheme, size_t scheme_size) {
    return (snprintf(scheme, scheme_size, "%s/%sscheme/%s.ini", theme_base, mux_dimension, file_name)
            && file_exist(scheme)) ||
           (snprintf(scheme, scheme_size, "%s/scheme/%s.ini", theme_base, file_name)
            && file_exist(scheme));
}

void init_theme_config(struct theme_config *theme, struct mux_device *device) {
    theme->SYSTEM.BACKGROUND = 0xC69200;
    theme->SYSTEM.BACKGROUND_ALPHA = 255;
    theme->SYSTEM.BACKGROUND_GRADIENT_COLOR = 0xC69200;
    theme->SYSTEM.BACKGROUND_GRADIENT_START = 0;
    theme->SYSTEM.BACKGROUND_GRADIENT_STOP = 200;
    theme->SYSTEM.BACKGROUND_GRADIENT_DIRECTION = 0;

    theme->ANIMATION.ANIMATION_DELAY = 100;
    theme->ANIMATION.ANIMATION_REPEAT = 0;

    theme->FONT.HEADER_PAD_TOP = 0;
    theme->FONT.HEADER_PAD_BOTTOM = 0;
    theme->FONT.HEADER_ICON_PAD_TOP = 0;
    theme->FONT.HEADER_ICON_PAD_BOTTOM = 0;
    theme->FONT.FOOTER_PAD_TOP = 0;
    theme->FONT.FOOTER_PAD_BOTTOM = 0;
    theme->FONT.FOOTER_ICON_PAD_TOP = 0;
    theme->FONT.FOOTER_ICON_PAD_BOTTOM = 0;
    theme->FONT.MESSAGE_PAD_TOP = 0;
    theme->FONT.MESSAGE_PAD_BOTTOM = 0;
    theme->FONT.MESSAGE_ICON_PAD_TOP = 0;
    theme->FONT.MESSAGE_ICON_PAD_BOTTOM = 0;
    theme->FONT.LIST_PAD_TOP = 0;
    theme->FONT.LIST_PAD_BOTTOM = 0;
    theme->FONT.LIST_PAD_LEFT = 32;
    theme->FONT.LIST_PAD_RIGHT = 6;
    theme->FONT.LIST_ICON_PAD_TOP = 0;
    theme->FONT.LIST_ICON_PAD_BOTTOM = 0;

    theme->STATUS.ALIGN = 1;
    theme->STATUS.PADDING_LEFT = 0;
    theme->STATUS.PADDING_RIGHT = 0;

    theme->STATUS.BATTERY.NORMAL = 0xFFFFFF;
    theme->STATUS.BATTERY.ACTIVE = 0x85F718;
    theme->STATUS.BATTERY.LOW = 0xD35C54;
    theme->STATUS.BATTERY.NORMAL_ALPHA = 255;
    theme->STATUS.BATTERY.ACTIVE_ALPHA = 255;
    theme->STATUS.BATTERY.LOW_ALPHA = 255;

    theme->STATUS.NETWORK.NORMAL = 0x282525;
    theme->STATUS.NETWORK.ACTIVE = 0xF7E318;
    theme->STATUS.NETWORK.NORMAL_ALPHA = 255;
    theme->STATUS.NETWORK.ACTIVE_ALPHA = 255;

    theme->STATUS.BLUETOOTH.NORMAL = 0x282525;
    theme->STATUS.BLUETOOTH.ACTIVE = 0xF7E318;
    theme->STATUS.BLUETOOTH.NORMAL_ALPHA = 255;
    theme->STATUS.BLUETOOTH.ACTIVE_ALPHA = 255;

    theme->DATETIME.TEXT = 0xFFFFFF;
    theme->DATETIME.ALPHA = 255;
    theme->DATETIME.ALIGN = 1;
    theme->DATETIME.PADDING_LEFT = 0;
    theme->DATETIME.PADDING_RIGHT = 0;

    theme->FOOTER.HEIGHT = 42;
    theme->FOOTER.BACKGROUND = 0x100808;
    theme->FOOTER.BACKGROUND_ALPHA = 255;
    theme->FOOTER.TEXT = 0xFFFFFF;
    theme->FOOTER.TEXT_ALPHA = 255;

    theme->HEADER.HEIGHT = 42;
    theme->HEADER.BACKGROUND = 0x100808;
    theme->HEADER.BACKGROUND_ALPHA = 255;
    theme->HEADER.TEXT = 0xFFFFFF;
    theme->HEADER.TEXT_ALPHA = 255;
    theme->HEADER.TEXT_ALIGN = 2;
    theme->HEADER.PADDING_LEFT = 0;
    theme->HEADER.PADDING_RIGHT = 0;

    theme->HELP.BACKGROUND = 0x282525;
    theme->HELP.BACKGROUND_ALPHA = 255;
    theme->HELP.BORDER = 0x100808;
    theme->HELP.BORDER_ALPHA = 255;
    theme->HELP.CONTENT = 0xA5B2B5;
    theme->HELP.TITLE = 0xF7E318;
    theme->HELP.RADIUS = 3;

    theme->NAV.ALIGNMENT = 255;

    theme->NAV.A.GLYPH = 0xF7E318;
    theme->NAV.A.GLYPH_ALPHA = 255;
    theme->NAV.A.GLYPH_RECOLOUR_ALPHA = 255;
    theme->NAV.A.TEXT = 0xFFFFFF;
    theme->NAV.A.TEXT_ALPHA = 255;

    theme->NAV.B.GLYPH = 0xF7E318;
    theme->NAV.B.GLYPH_ALPHA = 255;
    theme->NAV.B.GLYPH_RECOLOUR_ALPHA = 255;
    theme->NAV.B.TEXT = 0xFFFFFF;
    theme->NAV.B.TEXT_ALPHA = 255;

    theme->NAV.C.GLYPH = 0xF7E318;
    theme->NAV.C.GLYPH_ALPHA = 255;
    theme->NAV.C.GLYPH_RECOLOUR_ALPHA = 255;
    theme->NAV.C.TEXT = 0xFFFFFF;
    theme->NAV.C.TEXT_ALPHA = 255;

    theme->NAV.X.GLYPH = 0xF7E318;
    theme->NAV.X.GLYPH_ALPHA = 255;
    theme->NAV.X.GLYPH_RECOLOUR_ALPHA = 255;
    theme->NAV.X.TEXT = 0xFFFFFF;
    theme->NAV.X.TEXT_ALPHA = 255;

    theme->NAV.Y.GLYPH = 0xF7E318;
    theme->NAV.Y.GLYPH_ALPHA = 255;
    theme->NAV.Y.GLYPH_RECOLOUR_ALPHA = 255;
    theme->NAV.Y.TEXT = 0xFFFFFF;
    theme->NAV.Y.TEXT_ALPHA = 255;

    theme->NAV.Z.GLYPH = 0xF7E318;
    theme->NAV.Z.GLYPH_ALPHA = 255;
    theme->NAV.Z.GLYPH_RECOLOUR_ALPHA = 255;
    theme->NAV.Z.TEXT = 0xFFFFFF;
    theme->NAV.Z.TEXT_ALPHA = 255;

    theme->NAV.MENU.GLYPH = 0xF7E318;
    theme->NAV.MENU.GLYPH_ALPHA = 255;
    theme->NAV.MENU.GLYPH_RECOLOUR_ALPHA = 255;
    theme->NAV.MENU.TEXT = 0xFFFFFF;
    theme->NAV.MENU.TEXT_ALPHA = 255;

    theme->GRID.NAVIGATION_TYPE = 2;
    theme->GRID.BACKGROUND = 0xC69200;
    theme->GRID.BACKGROUND_ALPHA = 0;
    theme->GRID.LOCATION_X = 0;
    theme->GRID.LOCATION_Y = 0;
    theme->GRID.COLUMN_COUNT = 0;
    theme->GRID.ROW_COUNT = 0;
    theme->GRID.ENABLED = false;

    theme->GRID.CURRENT_ITEM_LABEL.ALIGNMENT = LV_ALIGN_BOTTOM_MID;
    theme->GRID.CURRENT_ITEM_LABEL.WIDTH = (int16_t) (device->MUX.WIDTH * .8);
    theme->GRID.CURRENT_ITEM_LABEL.HEIGHT = 0;
    theme->GRID.CURRENT_ITEM_LABEL.OFFSET_X = 0;
    theme->GRID.CURRENT_ITEM_LABEL.OFFSET_Y = -(theme->FOOTER.HEIGHT * 2);
    theme->GRID.CURRENT_ITEM_LABEL.RADIUS = 10;
    theme->GRID.CURRENT_ITEM_LABEL.BORDER_WIDTH = 5;
    theme->GRID.CURRENT_ITEM_LABEL.BORDER = 0xF7E318;
    theme->GRID.CURRENT_ITEM_LABEL.BORDER_ALPHA = 0;
    theme->GRID.CURRENT_ITEM_LABEL.BACKGROUND = 0xF7E318;
    theme->GRID.CURRENT_ITEM_LABEL.BACKGROUND_ALPHA = 0;
    theme->GRID.CURRENT_ITEM_LABEL.TEXT = 0x100808;
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_ALPHA = 0;
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_ALIGNMENT = LV_TEXT_ALIGN_CENTER;
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_LINE_SPACING = 0;
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_BOTTOM = 0;
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_LEFT = 0;
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_RIGHT = 0;
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_TOP = 0;

    theme->GRID.ROW_HEIGHT = 0;
    theme->GRID.COLUMN_WIDTH = 0;

    theme->GRID.CELL.WIDTH = 200;
    theme->GRID.CELL.HEIGHT = 200;
    theme->GRID.CELL.RADIUS = 10;
    theme->GRID.CELL.BORDER_WIDTH = 5;
    theme->GRID.CELL.IMAGE_PADDING_TOP = 5;
    theme->GRID.CELL.SHADOW = 0x000000;
    theme->GRID.CELL.SHADOW_WIDTH = 0;
    theme->GRID.CELL.SHADOW_X_OFFSET = 10;
    theme->GRID.CELL.SHADOW_Y_OFFSET = 10;
    theme->GRID.CELL.TEXT_PADDING_BOTTOM = 5;
    theme->GRID.CELL.TEXT_PADDING_SIDE = 5;
    theme->GRID.CELL.TEXT_LINE_SPACING = 0;

    theme->GRID.CELL_DEFAULT.BACKGROUND = 0xC69200;
    theme->GRID.CELL_DEFAULT.BACKGROUND_ALPHA = 255;
    theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_COLOR = 0xC69200;
    theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_START = 0;
    theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_STOP = 200;
    theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_DIRECTION = 0;
    theme->GRID.CELL_DEFAULT.BORDER = 0xC69200;
    theme->GRID.CELL_DEFAULT.BORDER_ALPHA = 255;
    theme->GRID.CELL_DEFAULT.IMAGE_ALPHA = 255;
    theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR = 0xFFFFFF;
    theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR_ALPHA = 0;
    theme->GRID.CELL_DEFAULT.TEXT = 0xFFFFFF;
    theme->GRID.CELL_DEFAULT.TEXT_ALPHA = 255;

    theme->GRID.CELL_FOCUS.BACKGROUND = 0xF7E318;
    theme->GRID.CELL_FOCUS.BACKGROUND_ALPHA = 255;
    theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_COLOR = 0xF7E318;
    theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_START = 0;
    theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_STOP = 200;
    theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_DIRECTION = 0;
    theme->GRID.CELL_FOCUS.BORDER = 0xF7E318;
    theme->GRID.CELL_FOCUS.BORDER_ALPHA = 255;
    theme->GRID.CELL_FOCUS.IMAGE_ALPHA = 255;
    theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR = 0xFFFFFF;
    theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR_ALPHA = 0;
    theme->GRID.CELL_FOCUS.TEXT = 0x100808;
    theme->GRID.CELL_FOCUS.TEXT_ALPHA = 255;

    theme->LIST_DEFAULT.RADIUS = 0;
    theme->LIST_DEFAULT.BACKGROUND = 0xC69200;
    theme->LIST_DEFAULT.BACKGROUND_ALPHA = 255;
    theme->LIST_DEFAULT.GRADIENT_START = 0;
    theme->LIST_DEFAULT.GRADIENT_STOP = 200;
    theme->LIST_DEFAULT.GRADIENT_DIRECTION = 0;
    theme->LIST_DEFAULT.BORDER_WIDTH = 5;
    theme->LIST_DEFAULT.BORDER_SIDE = LV_BORDER_SIDE_LEFT;
    theme->LIST_DEFAULT.INDICATOR = 0xA5B2B5;
    theme->LIST_DEFAULT.INDICATOR_ALPHA = 255;
    theme->LIST_DEFAULT.TEXT = 0xFFFFFF;
    theme->LIST_DEFAULT.TEXT_ALPHA = 255;
    theme->LIST_DEFAULT.BACKGROUND_GRADIENT = theme->SYSTEM.BACKGROUND;
    theme->LIST_DEFAULT.GLYPH_PADDING_LEFT = 19;
    theme->LIST_DEFAULT.GLYPH_ALPHA = 255;
    theme->LIST_DEFAULT.GLYPH_RECOLOUR = 0xFFFFFF;
    theme->LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA = 0;
    theme->LIST_DEFAULT.LABEL_LONG_MODE = LV_LABEL_LONG_SCROLL_CIRCULAR;

    theme->LIST_DISABLED.TEXT = 0x808080;
    theme->LIST_DISABLED.TEXT_ALPHA = 255;

    theme->LIST_FOCUS.BACKGROUND = 0xF7E318;
    theme->LIST_FOCUS.BACKGROUND_ALPHA = 255;
    theme->LIST_FOCUS.GRADIENT_START = 0;
    theme->LIST_FOCUS.GRADIENT_STOP = 200;
    theme->LIST_FOCUS.GRADIENT_DIRECTION = 0;
    theme->LIST_FOCUS.BORDER_WIDTH = 5;
    theme->LIST_FOCUS.BORDER_SIDE = LV_BORDER_SIDE_LEFT;
    theme->LIST_FOCUS.INDICATOR = 0xF7E318;
    theme->LIST_FOCUS.INDICATOR_ALPHA = 255;
    theme->LIST_FOCUS.TEXT = 0x100808;
    theme->LIST_FOCUS.TEXT_ALPHA = 255;
    theme->LIST_FOCUS.BACKGROUND_GRADIENT = theme->SYSTEM.BACKGROUND;
    theme->LIST_FOCUS.GLYPH_ALPHA = 255;
    theme->LIST_FOCUS.GLYPH_RECOLOUR = 0x100808;
    theme->LIST_FOCUS.GLYPH_RECOLOUR_ALPHA = 0;

    theme->IMAGE_LIST.ALPHA = 255;
    theme->IMAGE_LIST.RADIUS = 3;
    theme->IMAGE_LIST.RECOLOUR = 0xF7E318;
    theme->IMAGE_LIST.RECOLOUR_ALPHA = 255;
    theme->IMAGE_LIST.PAD_TOP = 0;
    theme->IMAGE_LIST.PAD_BOTTOM = 0;
    theme->IMAGE_LIST.PAD_LEFT = 0;
    theme->IMAGE_LIST.PAD_RIGHT = 0;

    theme->IMAGE_PREVIEW.ALPHA = 255;
    theme->IMAGE_PREVIEW.RADIUS = 3;
    theme->IMAGE_PREVIEW.RECOLOUR = 0xF7E318;
    theme->IMAGE_PREVIEW.RECOLOUR_ALPHA = 255;

    theme->CHARGER.BACKGROUND = 0x282525;
    theme->CHARGER.BACKGROUND_ALPHA = 255;
    theme->CHARGER.TEXT = 0xFFFFFF;
    theme->CHARGER.TEXT_ALPHA = 255;
    theme->CHARGER.Y_POS = 0;

    theme->VERBOSE_BOOT.BACKGROUND = 0x000000;
    theme->VERBOSE_BOOT.BACKGROUND_ALPHA = 255;
    theme->VERBOSE_BOOT.TEXT = 0xFFFFFF;
    theme->VERBOSE_BOOT.TEXT_ALPHA = 255;
    theme->VERBOSE_BOOT.Y_POS = 0;

    theme->OSK.BACKGROUND = 0x100808;
    theme->OSK.BACKGROUND_ALPHA = 255;
    theme->OSK.BORDER = 0x100808;
    theme->OSK.BORDER_ALPHA = 255;
    theme->OSK.RADIUS = 3;
    theme->OSK.TEXT = 0xFFFFFF;
    theme->OSK.TEXT_ALPHA = 255;
    theme->OSK.TEXT_FOCUS = 0x100808;
    theme->OSK.TEXT_FOCUS_ALPHA = 255;
    theme->OSK.ITEM.BACKGROUND = 0x282525;
    theme->OSK.ITEM.BACKGROUND_ALPHA = 255;
    theme->OSK.ITEM.BACKGROUND_FOCUS = 0xF7E318;
    theme->OSK.ITEM.BACKGROUND_FOCUS_ALPHA = 255;
    theme->OSK.ITEM.BORDER = 0x282525;
    theme->OSK.ITEM.BORDER_ALPHA = 255;
    theme->OSK.ITEM.BORDER_FOCUS = 0xF7E318;
    theme->OSK.ITEM.BORDER_FOCUS_ALPHA = 255;
    theme->OSK.ITEM.RADIUS = 3;

    theme->MESSAGE.BACKGROUND = 0x100808;
    theme->MESSAGE.BACKGROUND_ALPHA = 255;
    theme->MESSAGE.BORDER = 0x100808;
    theme->MESSAGE.BORDER_ALPHA = 255;
    theme->MESSAGE.RADIUS = 3;
    theme->MESSAGE.TEXT = 0xFFFFFF;
    theme->MESSAGE.TEXT_ALPHA = 255;

    theme->BAR.PANEL_WIDTH = (int16_t) (device->MUX.WIDTH - 25);
    theme->BAR.PANEL_HEIGHT = 42;
    theme->BAR.PANEL_BACKGROUND = 0x100808;
    theme->BAR.PANEL_BACKGROUND_ALPHA = 255;
    theme->BAR.PANEL_BORDER = 0x100808;
    theme->BAR.PANEL_BORDER_ALPHA = 255;
    theme->BAR.PANEL_BORDER_RADIUS = 3;
    theme->BAR.PROGRESS_WIDTH = (int16_t) (device->MUX.WIDTH - 90);
    theme->BAR.PROGRESS_HEIGHT = 16;
    theme->BAR.PROGRESS_MAIN_BACKGROUND = 0x7E730C;
    theme->BAR.PROGRESS_MAIN_BACKGROUND_ALPHA = 255;
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND = 0xF7E318;
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA = 255;
    theme->BAR.PROGRESS_RADIUS = 3;
    theme->BAR.ICON = 0xF7E318;
    theme->BAR.ICON_ALPHA = 255;
    theme->BAR.Y_POS = (int16_t) (device->MUX.HEIGHT - 96);

    theme->ROLL.TEXT = 0x7E730C;
    theme->ROLL.TEXT_ALPHA = 255;
    theme->ROLL.BACKGROUND = 0x100808;
    theme->ROLL.BACKGROUND_ALPHA = 255;
    theme->ROLL.RADIUS = 3;
    theme->ROLL.SELECT_TEXT = 0xF7E318;
    theme->ROLL.SELECT_TEXT_ALPHA = 255;
    theme->ROLL.SELECT_BACKGROUND = 0x7E730C;
    theme->ROLL.SELECT_BACKGROUND_ALPHA = 255;
    theme->ROLL.SELECT_RADIUS = 3;
    theme->ROLL.BORDER_COLOUR = 0x100808;
    theme->ROLL.BORDER_ALPHA = 255;
    theme->ROLL.BORDER_RADIUS = 255;

    theme->COUNTER.ALIGNMENT = 0;
    theme->COUNTER.PADDING_AROUND = 5;
    theme->COUNTER.PADDING_SIDE = 15;
    theme->COUNTER.PADDING_TOP = 0;
    theme->COUNTER.BORDER_COLOUR = 0x100808;
    theme->COUNTER.BORDER_ALPHA = 0;
    theme->COUNTER.BORDER_WIDTH = 2;
    theme->COUNTER.RADIUS = 0;
    theme->COUNTER.BACKGROUND = 0x100808;
    theme->COUNTER.BACKGROUND_ALPHA = 0;
    theme->COUNTER.TEXT = 0xFFFFFF;
    theme->COUNTER.TEXT_ALPHA = 0;
    theme->COUNTER.TEXT_FADE_TIME = 0;
    strncpy(theme->COUNTER.TEXT_SEPARATOR, " / ", MAX_BUFFER_SIZE - 1);
    theme->COUNTER.TEXT_SEPARATOR[MAX_BUFFER_SIZE - 1] = '\0';

    theme->MISC.STATIC_ALIGNMENT = 255;
    theme->MUX.ITEM.COUNT = 11;
    theme->MUX.ITEM.HEIGHT = 0;
    theme->MISC.CONTENT.SIZE_TO_CONTENT = 0;
    theme->MISC.CONTENT.ALIGNMENT = 0;
    theme->MISC.CONTENT.PADDING_LEFT = 0;
    theme->MISC.CONTENT.PADDING_TOP = 0;
    int default_content_height = device->MUX.HEIGHT - theme->HEADER.HEIGHT - theme->FOOTER.HEIGHT - 4;
    theme->MISC.CONTENT.HEIGHT = default_content_height;
    theme->MISC.CONTENT.WIDTH = device->MUX.WIDTH;
    theme->MISC.ANIMATED_BACKGROUND = 0;
    theme->MISC.RANDOM_BACKGROUND = 0;
    theme->MISC.IMAGE_OVERLAY = 0;
    theme->MISC.NAVIGATION_TYPE = 0;
    theme->MISC.ANTIALIASING = 1;

    strncpy(theme->TERMINAL.FOREGROUND, "FFFFFF", MAX_BUFFER_SIZE - 1);
    theme->TERMINAL.FOREGROUND[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(theme->TERMINAL.BACKGROUND, "000000", MAX_BUFFER_SIZE - 1);
    theme->TERMINAL.BACKGROUND[MAX_BUFFER_SIZE - 1] = '\0';
}

void load_theme_from_scheme(const char *scheme, struct theme_config *theme, struct mux_device *device) {
    mini_t *muos_theme = mini_try_load(scheme);

    theme->SYSTEM.BACKGROUND = get_ini_hex(muos_theme, "background", "BACKGROUND", theme->SYSTEM.BACKGROUND);
    theme->SYSTEM.BACKGROUND_ALPHA = get_ini_int(muos_theme, "background", "BACKGROUND_ALPHA", theme->SYSTEM.BACKGROUND_ALPHA);
    theme->SYSTEM.BACKGROUND_GRADIENT_COLOR = get_ini_hex(muos_theme, "background", "BACKGROUND_GRADIENT_COLOR", theme->SYSTEM.BACKGROUND_GRADIENT_COLOR);
    theme->SYSTEM.BACKGROUND_GRADIENT_START = get_ini_int(muos_theme, "background", "BACKGROUND_GRADIENT_START", theme->SYSTEM.BACKGROUND_GRADIENT_START);
    theme->SYSTEM.BACKGROUND_GRADIENT_STOP = get_ini_int(muos_theme, "background", "BACKGROUND_GRADIENT_STOP", theme->SYSTEM.BACKGROUND_GRADIENT_STOP);
    theme->SYSTEM.BACKGROUND_GRADIENT_DIRECTION = get_ini_int(muos_theme, "background", "BACKGROUND_GRADIENT_DIRECTION", theme->SYSTEM.BACKGROUND_GRADIENT_DIRECTION);

    theme->ANIMATION.ANIMATION_DELAY = get_ini_int(muos_theme, "animation", "ANIMATION_DELAY", theme->ANIMATION.ANIMATION_DELAY);
    if (theme->ANIMATION.ANIMATION_DELAY < 10) theme->ANIMATION.ANIMATION_DELAY = 10;
    theme->ANIMATION.ANIMATION_REPEAT = get_ini_int(muos_theme, "animation", "ANIMATION_REPEAT", theme->ANIMATION.ANIMATION_REPEAT);

    theme->FONT.HEADER_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_HEADER_PAD_TOP", theme->FONT.HEADER_PAD_TOP);
    theme->FONT.HEADER_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_HEADER_PAD_BOTTOM", theme->FONT.HEADER_PAD_BOTTOM);
    theme->FONT.HEADER_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_HEADER_ICON_PAD_TOP", theme->FONT.HEADER_ICON_PAD_TOP);
    theme->FONT.HEADER_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_HEADER_ICON_PAD_BOTTOM", theme->FONT.HEADER_ICON_PAD_BOTTOM);
    theme->FONT.FOOTER_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_FOOTER_PAD_TOP", theme->FONT.FOOTER_PAD_TOP);
    theme->FONT.FOOTER_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_FOOTER_PAD_BOTTOM", theme->FONT.FOOTER_PAD_BOTTOM);
    theme->FONT.FOOTER_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_FOOTER_ICON_PAD_TOP", theme->FONT.FOOTER_ICON_PAD_TOP);
    theme->FONT.FOOTER_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_FOOTER_ICON_PAD_BOTTOM", theme->FONT.FOOTER_ICON_PAD_BOTTOM);
    theme->FONT.MESSAGE_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_MESSAGE_PAD_TOP", theme->FONT.MESSAGE_PAD_TOP);
    theme->FONT.MESSAGE_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_MESSAGE_PAD_BOTTOM", theme->FONT.MESSAGE_PAD_BOTTOM);
    theme->FONT.MESSAGE_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_MESSAGE_ICON_PAD_TOP", theme->FONT.MESSAGE_ICON_PAD_TOP);
    theme->FONT.MESSAGE_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_MESSAGE_ICON_PAD_BOTTOM", theme->FONT.MESSAGE_ICON_PAD_BOTTOM);
    theme->FONT.LIST_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_TOP", theme->FONT.LIST_PAD_TOP);
    theme->FONT.LIST_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_BOTTOM", theme->FONT.LIST_PAD_BOTTOM);
    theme->FONT.LIST_PAD_LEFT = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_LEFT", theme->FONT.LIST_PAD_LEFT);
    theme->FONT.LIST_PAD_RIGHT = get_ini_int(muos_theme, "font", "FONT_LIST_PAD_RIGHT", theme->FONT.LIST_PAD_RIGHT);
    theme->FONT.LIST_ICON_PAD_TOP = get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_TOP", theme->FONT.LIST_ICON_PAD_TOP);
    theme->FONT.LIST_ICON_PAD_BOTTOM = get_ini_int(muos_theme, "font", "FONT_LIST_ICON_PAD_BOTTOM", theme->FONT.LIST_ICON_PAD_BOTTOM);

    theme->STATUS.ALIGN = get_ini_int(muos_theme, "status", "ALIGN", theme->STATUS.ALIGN);
    theme->STATUS.PADDING_LEFT = get_ini_int(muos_theme, "status", "PADDING_LEFT", theme->STATUS.PADDING_LEFT);
    theme->STATUS.PADDING_RIGHT = get_ini_int(muos_theme, "status", "PADDING_RIGHT", theme->STATUS.PADDING_RIGHT);

    theme->STATUS.BATTERY.NORMAL = get_ini_hex(muos_theme, "battery", "BATTERY_NORMAL", theme->STATUS.BATTERY.NORMAL);
    theme->STATUS.BATTERY.ACTIVE = get_ini_hex(muos_theme, "battery", "BATTERY_ACTIVE", theme->STATUS.BATTERY.ACTIVE);
    theme->STATUS.BATTERY.LOW = get_ini_hex(muos_theme, "battery", "BATTERY_LOW", theme->STATUS.BATTERY.LOW);
    theme->STATUS.BATTERY.NORMAL_ALPHA = get_ini_int(muos_theme, "battery", "BATTERY_NORMAL_ALPHA", theme->STATUS.BATTERY.NORMAL_ALPHA);
    theme->STATUS.BATTERY.ACTIVE_ALPHA = get_ini_int(muos_theme, "battery", "BATTERY_ACTIVE_ALPHA", theme->STATUS.BATTERY.ACTIVE_ALPHA);
    theme->STATUS.BATTERY.LOW_ALPHA = get_ini_int(muos_theme, "battery", "BATTERY_LOW_ALPHA", theme->STATUS.BATTERY.LOW_ALPHA);

    theme->STATUS.NETWORK.NORMAL = get_ini_hex(muos_theme, "network", "NETWORK_NORMAL", theme->STATUS.NETWORK.NORMAL);
    theme->STATUS.NETWORK.ACTIVE = get_ini_hex(muos_theme, "network", "NETWORK_ACTIVE", theme->STATUS.NETWORK.ACTIVE);
    theme->STATUS.NETWORK.NORMAL_ALPHA = get_ini_int(muos_theme, "network", "NETWORK_NORMAL_ALPHA", theme->STATUS.NETWORK.NORMAL_ALPHA);
    theme->STATUS.NETWORK.ACTIVE_ALPHA = get_ini_int(muos_theme, "network", "NETWORK_ACTIVE_ALPHA", theme->STATUS.NETWORK.ACTIVE_ALPHA);

    theme->STATUS.BLUETOOTH.NORMAL = get_ini_hex(muos_theme, "bluetooth", "BLUETOOTH_NORMAL", theme->STATUS.BLUETOOTH.NORMAL);
    theme->STATUS.BLUETOOTH.ACTIVE = get_ini_hex(muos_theme, "bluetooth", "BLUETOOTH_ACTIVE", theme->STATUS.BLUETOOTH.ACTIVE);
    theme->STATUS.BLUETOOTH.NORMAL_ALPHA = get_ini_int(muos_theme, "bluetooth", "BLUETOOTH_NORMAL_ALPHA", theme->STATUS.BLUETOOTH.NORMAL_ALPHA);
    theme->STATUS.BLUETOOTH.ACTIVE_ALPHA = get_ini_int(muos_theme, "bluetooth", "BLUETOOTH_ACTIVE_ALPHA", theme->STATUS.BLUETOOTH.ACTIVE_ALPHA);

    theme->DATETIME.TEXT = get_ini_hex(muos_theme, "date", "DATETIME_TEXT", theme->DATETIME.TEXT);
    theme->DATETIME.ALPHA = get_ini_int(muos_theme, "date", "DATETIME_ALPHA", theme->DATETIME.ALPHA);
    theme->DATETIME.ALIGN = get_ini_int(muos_theme, "date", "DATETIME_ALIGN", theme->DATETIME.ALIGN);
    theme->DATETIME.PADDING_LEFT = get_ini_int(muos_theme, "date", "PADDING_LEFT", theme->DATETIME.PADDING_LEFT);
    theme->DATETIME.PADDING_RIGHT = get_ini_int(muos_theme, "date", "PADDING_RIGHT", theme->DATETIME.PADDING_RIGHT);

    theme->FOOTER.HEIGHT = get_ini_int(muos_theme, "footer", "FOOTER_HEIGHT", theme->FOOTER.HEIGHT);
    theme->FOOTER.BACKGROUND = get_ini_hex(muos_theme, "footer", "FOOTER_BACKGROUND", theme->FOOTER.BACKGROUND);
    theme->FOOTER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "footer", "FOOTER_BACKGROUND_ALPHA", theme->FOOTER.BACKGROUND_ALPHA);
    theme->FOOTER.TEXT = get_ini_hex(muos_theme, "footer", "FOOTER_TEXT", theme->FOOTER.TEXT);
    theme->FOOTER.TEXT_ALPHA = get_ini_int(muos_theme, "footer", "FOOTER_TEXT_ALPHA", theme->FOOTER.TEXT_ALPHA);

    theme->HEADER.HEIGHT = get_ini_int(muos_theme, "header", "HEADER_HEIGHT", theme->HEADER.HEIGHT);
    theme->HEADER.BACKGROUND = get_ini_hex(muos_theme, "header", "HEADER_BACKGROUND", theme->HEADER.BACKGROUND);
    theme->HEADER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "header", "HEADER_BACKGROUND_ALPHA", theme->HEADER.BACKGROUND_ALPHA);
    theme->HEADER.TEXT = get_ini_hex(muos_theme, "header", "HEADER_TEXT", theme->HEADER.TEXT);
    theme->HEADER.TEXT_ALPHA = get_ini_int(muos_theme, "header", "HEADER_TEXT_ALPHA", theme->HEADER.TEXT_ALPHA);
    theme->HEADER.TEXT_ALIGN = get_ini_int(muos_theme, "header", "HEADER_TEXT_ALIGN", theme->HEADER.TEXT_ALIGN);
    theme->HEADER.PADDING_LEFT = get_ini_int(muos_theme, "header", "PADDING_LEFT", theme->HEADER.PADDING_LEFT);
    theme->HEADER.PADDING_RIGHT = get_ini_int(muos_theme, "header", "PADDING_RIGHT", theme->HEADER.PADDING_RIGHT);

    theme->HELP.BACKGROUND = get_ini_hex(muos_theme, "help", "HELP_BACKGROUND", theme->HELP.BACKGROUND);
    theme->HELP.BACKGROUND_ALPHA = get_ini_int(muos_theme, "help", "HELP_BACKGROUND_ALPHA", theme->HELP.BACKGROUND_ALPHA);
    theme->HELP.BORDER = get_ini_hex(muos_theme, "help", "HELP_BORDER", theme->HELP.BORDER);
    theme->HELP.BORDER_ALPHA = get_ini_int(muos_theme, "help", "HELP_BORDER_ALPHA", theme->HELP.BORDER_ALPHA);
    theme->HELP.CONTENT = get_ini_hex(muos_theme, "help", "HELP_CONTENT", theme->HELP.CONTENT);
    theme->HELP.TITLE = get_ini_hex(muos_theme, "help", "HELP_TITLE", theme->HELP.TITLE);
    theme->HELP.RADIUS = get_ini_int(muos_theme, "help", "HELP_RADIUS", theme->HELP.RADIUS);

    theme->NAV.ALIGNMENT = get_ini_int(muos_theme, "navigation", "ALIGNMENT", theme->NAV.ALIGNMENT);

    theme->NAV.A.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_A_GLYPH", theme->NAV.A.GLYPH);
    theme->NAV.A.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_GLYPH_ALPHA", theme->NAV.A.GLYPH_ALPHA);
    theme->NAV.A.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_GLYPH_RECOLOUR_ALPHA", theme->NAV.A.GLYPH_RECOLOUR_ALPHA);
    theme->NAV.A.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_A_TEXT", theme->NAV.A.TEXT);
    theme->NAV.A.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_A_TEXT_ALPHA", theme->NAV.A.TEXT_ALPHA);

    theme->NAV.B.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_B_GLYPH", theme->NAV.B.GLYPH);
    theme->NAV.B.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_GLYPH_ALPHA", theme->NAV.B.GLYPH_ALPHA);
    theme->NAV.B.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_GLYPH_RECOLOUR_ALPHA", theme->NAV.B.GLYPH_RECOLOUR_ALPHA);
    theme->NAV.B.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_B_TEXT", theme->NAV.B.TEXT);
    theme->NAV.B.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_B_TEXT_ALPHA", theme->NAV.B.TEXT_ALPHA);

    theme->NAV.C.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_C_GLYPH", theme->NAV.C.GLYPH);
    theme->NAV.C.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_GLYPH_ALPHA", theme->NAV.C.GLYPH_ALPHA);
    theme->NAV.C.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_GLYPH_RECOLOUR_ALPHA", theme->NAV.C.GLYPH_RECOLOUR_ALPHA);
    theme->NAV.C.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_C_TEXT", theme->NAV.C.TEXT);
    theme->NAV.C.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_C_TEXT_ALPHA", theme->NAV.C.TEXT_ALPHA);

    theme->NAV.X.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_X_GLYPH", theme->NAV.X.GLYPH);
    theme->NAV.X.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_GLYPH_ALPHA", theme->NAV.X.GLYPH_ALPHA);
    theme->NAV.X.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_GLYPH_RECOLOUR_ALPHA", theme->NAV.X.GLYPH_RECOLOUR_ALPHA);
    theme->NAV.X.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_X_TEXT", theme->NAV.X.TEXT);
    theme->NAV.X.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_X_TEXT_ALPHA", theme->NAV.X.TEXT_ALPHA);

    theme->NAV.Y.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_Y_GLYPH", theme->NAV.Y.GLYPH);
    theme->NAV.Y.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_GLYPH_ALPHA", theme->NAV.Y.GLYPH_ALPHA);
    theme->NAV.Y.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_GLYPH_RECOLOUR_ALPHA", theme->NAV.Y.GLYPH_RECOLOUR_ALPHA);
    theme->NAV.Y.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_Y_TEXT", theme->NAV.Y.TEXT);
    theme->NAV.Y.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Y_TEXT_ALPHA", theme->NAV.Y.TEXT_ALPHA);

    theme->NAV.Z.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_Z_GLYPH", theme->NAV.Z.GLYPH);
    theme->NAV.Z.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_GLYPH_ALPHA", theme->NAV.Z.GLYPH_ALPHA);
    theme->NAV.Z.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_GLYPH_RECOLOUR_ALPHA", theme->NAV.Z.GLYPH_RECOLOUR_ALPHA);
    theme->NAV.Z.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_Z_TEXT", theme->NAV.Z.TEXT);
    theme->NAV.Z.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_Z_TEXT_ALPHA", theme->NAV.Z.TEXT_ALPHA);

    theme->NAV.MENU.GLYPH = get_ini_hex(muos_theme, "navigation", "NAV_MENU_GLYPH", theme->NAV.MENU.GLYPH);
    theme->NAV.MENU.GLYPH_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_GLYPH_ALPHA", theme->NAV.MENU.GLYPH_ALPHA);
    theme->NAV.MENU.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_GLYPH_RECOLOUR_ALPHA", theme->NAV.MENU.GLYPH_RECOLOUR_ALPHA);
    theme->NAV.MENU.TEXT = get_ini_hex(muos_theme, "navigation", "NAV_MENU_TEXT", theme->NAV.MENU.TEXT);
    theme->NAV.MENU.TEXT_ALPHA = get_ini_int(muos_theme, "navigation", "NAV_MENU_TEXT_ALPHA", theme->NAV.MENU.TEXT_ALPHA);

    theme->GRID.NAVIGATION_TYPE = get_ini_int(muos_theme, "grid", "NAVIGATION_TYPE", theme->GRID.NAVIGATION_TYPE);
    theme->GRID.BACKGROUND = get_ini_hex(muos_theme, "grid", "BACKGROUND", theme->GRID.BACKGROUND);
    theme->GRID.BACKGROUND_ALPHA = get_ini_int(muos_theme, "grid", "BACKGROUND_ALPHA", theme->GRID.BACKGROUND_ALPHA);
    theme->GRID.LOCATION_X = get_ini_int(muos_theme, "grid", "LOCATION_X", theme->GRID.LOCATION_X);
    theme->GRID.LOCATION_Y = get_ini_int(muos_theme, "grid", "LOCATION_Y", theme->GRID.LOCATION_Y);
    theme->GRID.COLUMN_COUNT = get_ini_int(muos_theme, "grid", "COLUMN_COUNT", theme->GRID.COLUMN_COUNT);
    theme->GRID.ROW_COUNT = get_ini_int(muos_theme, "grid", "ROW_COUNT", theme->GRID.ROW_COUNT);

    theme->GRID.CURRENT_ITEM_LABEL.ALIGNMENT = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_ALIGNMENT",
                                                           theme->GRID.CURRENT_ITEM_LABEL.ALIGNMENT);
    theme->GRID.CURRENT_ITEM_LABEL.WIDTH = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_WIDTH",
                                                       theme->GRID.CURRENT_ITEM_LABEL.WIDTH);
    theme->GRID.CURRENT_ITEM_LABEL.HEIGHT = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_HEIGHT", theme->GRID.CURRENT_ITEM_LABEL.HEIGHT);
    theme->GRID.CURRENT_ITEM_LABEL.OFFSET_X = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_OFFSET_X", theme->GRID.CURRENT_ITEM_LABEL.OFFSET_X);
    theme->GRID.CURRENT_ITEM_LABEL.OFFSET_Y = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_OFFSET_Y",
                                                          theme->GRID.CURRENT_ITEM_LABEL.OFFSET_Y);
    theme->GRID.CURRENT_ITEM_LABEL.RADIUS = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_RADIUS", theme->GRID.CURRENT_ITEM_LABEL.RADIUS);
    theme->GRID.CURRENT_ITEM_LABEL.BORDER_WIDTH = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_BORDER_WIDTH", theme->GRID.CURRENT_ITEM_LABEL.BORDER_WIDTH);
    theme->GRID.CURRENT_ITEM_LABEL.BORDER = get_ini_hex(muos_theme, "grid", "CURRENT_ITEM_LABEL_BORDER", theme->GRID.CURRENT_ITEM_LABEL.BORDER);
    theme->GRID.CURRENT_ITEM_LABEL.BORDER_ALPHA = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_BORDER_ALPHA", theme->GRID.CURRENT_ITEM_LABEL.BORDER_ALPHA);
    theme->GRID.CURRENT_ITEM_LABEL.BACKGROUND = get_ini_hex(muos_theme, "grid", "CURRENT_ITEM_LABEL_BACKGROUND", theme->GRID.CURRENT_ITEM_LABEL.BACKGROUND);
    theme->GRID.CURRENT_ITEM_LABEL.BACKGROUND_ALPHA = get_ini_int(muos_theme, "grid",
                                                                  "CURRENT_ITEM_LABEL_BACKGROUND_ALPHA", theme->GRID.CURRENT_ITEM_LABEL.BACKGROUND_ALPHA);
    theme->GRID.CURRENT_ITEM_LABEL.TEXT = get_ini_hex(muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT", theme->GRID.CURRENT_ITEM_LABEL.TEXT);
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_ALPHA = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT_ALPHA", theme->GRID.CURRENT_ITEM_LABEL.TEXT_ALPHA);
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_ALIGNMENT = get_ini_int(muos_theme, "grid", "CURRENT_ITEM_LABEL_TEXT_ALIGNMENT",
                                                                theme->GRID.CURRENT_ITEM_LABEL.TEXT_ALIGNMENT);
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_LINE_SPACING = get_ini_int(muos_theme, "grid",
                                                                   "CURRENT_ITEM_LABEL_TEXT_LINE_SPACING", theme->GRID.CURRENT_ITEM_LABEL.TEXT_LINE_SPACING);
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_BOTTOM = get_ini_int(muos_theme, "grid",
                                                                     "CURRENT_ITEM_LABEL_TEXT_PADDING_BOTTOM", theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_BOTTOM);
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_LEFT = get_ini_int(muos_theme, "grid",
                                                                   "CURRENT_ITEM_LABEL_TEXT_PADDING_LEFT", theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_LEFT);
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_RIGHT = get_ini_int(muos_theme, "grid",
                                                                    "CURRENT_ITEM_LABEL_TEXT_PADDING_RIGHT", theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_RIGHT);
    theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_TOP = get_ini_int(muos_theme, "grid",
                                                                  "CURRENT_ITEM_LABEL_TEXT_PADDING_TOP", theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_TOP);

    theme->GRID.ROW_HEIGHT = get_ini_int(muos_theme, "grid", "ROW_HEIGHT", theme->GRID.ROW_HEIGHT);
    theme->GRID.COLUMN_WIDTH = get_ini_int(muos_theme, "grid", "COLUMN_WIDTH", theme->GRID.COLUMN_WIDTH);

    theme->GRID.CELL.WIDTH = get_ini_int(muos_theme, "grid", "CELL_WIDTH", theme->GRID.CELL.WIDTH);
    theme->GRID.CELL.HEIGHT = get_ini_int(muos_theme, "grid", "CELL_HEIGHT", theme->GRID.CELL.HEIGHT);
    theme->GRID.CELL.RADIUS = get_ini_int(muos_theme, "grid", "CELL_RADIUS", theme->GRID.CELL.RADIUS);
    theme->GRID.CELL.BORDER_WIDTH = get_ini_int(muos_theme, "grid", "CELL_BORDER_WIDTH", theme->GRID.CELL.BORDER_WIDTH);
    theme->GRID.CELL.IMAGE_PADDING_TOP = get_ini_int(muos_theme, "grid", "CELL_IMAGE_PADDING_TOP", theme->GRID.CELL.IMAGE_PADDING_TOP);
    theme->GRID.CELL.TEXT_PADDING_BOTTOM = get_ini_int(muos_theme, "grid", "CELL_TEXT_PADDING_BOTTOM", theme->GRID.CELL.TEXT_PADDING_BOTTOM);
    theme->GRID.CELL.TEXT_PADDING_SIDE = get_ini_int(muos_theme, "grid", "CELL_TEXT_PADDING_SIDE", theme->GRID.CELL.TEXT_PADDING_SIDE);
    theme->GRID.CELL.TEXT_LINE_SPACING = get_ini_int(muos_theme, "grid", "CELL_TEXT_LINE_SPACING", theme->GRID.CELL.TEXT_LINE_SPACING);
    theme->GRID.CELL.SHADOW = get_ini_hex(muos_theme, "grid", "CELL_SHADOW", theme->GRID.CELL.SHADOW);
    theme->GRID.CELL.SHADOW_WIDTH = get_ini_int(muos_theme, "grid", "CELL_SHADOW_WIDTH", theme->GRID.CELL.SHADOW_WIDTH);
    theme->GRID.CELL.SHADOW_X_OFFSET = get_ini_int(muos_theme, "grid", "CELL_SHADOW_X_OFFSET", theme->GRID.CELL.SHADOW_X_OFFSET);
    theme->GRID.CELL.SHADOW_Y_OFFSET = get_ini_int(muos_theme, "grid", "CELL_SHADOW_Y_OFFSET", theme->GRID.CELL.SHADOW_Y_OFFSET);

    theme->GRID.CELL_DEFAULT.BACKGROUND = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND", theme->GRID.CELL_DEFAULT.BACKGROUND);
    theme->GRID.CELL_DEFAULT.BACKGROUND_ALPHA = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_ALPHA", theme->GRID.CELL_DEFAULT.BACKGROUND_ALPHA);
    theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_COLOR = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_GRADIENT_COLOR", theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_COLOR);
    theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_START = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_GRADIENT_START", theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_START);
    theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_STOP = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_GRADIENT_STOP", theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_STOP);
    theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_DIRECTION = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_BACKGROUND_GRADIENT_DIRECTION", theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_DIRECTION);
    theme->GRID.CELL_DEFAULT.BORDER = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_BORDER", theme->GRID.CELL_DEFAULT.BORDER);
    theme->GRID.CELL_DEFAULT.BORDER_ALPHA = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_BORDER_ALPHA", theme->GRID.CELL_DEFAULT.BORDER_ALPHA);
    theme->GRID.CELL_DEFAULT.IMAGE_ALPHA = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_IMAGE_ALPHA", theme->GRID.CELL_DEFAULT.IMAGE_ALPHA);
    theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_IMAGE_RECOLOUR", theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR);
    theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR_ALPHA = get_ini_int(muos_theme, "grid",
                                                                "CELL_DEFAULT_IMAGE_RECOLOUR_ALPHA", theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR_ALPHA);
    theme->GRID.CELL_DEFAULT.TEXT = get_ini_hex(muos_theme, "grid", "CELL_DEFAULT_TEXT", theme->GRID.CELL_DEFAULT.TEXT);
    theme->GRID.CELL_DEFAULT.TEXT_ALPHA = get_ini_int(muos_theme, "grid", "CELL_DEFAULT_TEXT_ALPHA", theme->GRID.CELL_DEFAULT.TEXT_ALPHA);

    theme->GRID.CELL_FOCUS.BACKGROUND = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_BACKGROUND", theme->GRID.CELL_FOCUS.BACKGROUND);
    theme->GRID.CELL_FOCUS.BACKGROUND_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_BACKGROUND_ALPHA", theme->GRID.CELL_FOCUS.BACKGROUND_ALPHA);
    theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_COLOR = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_BACKGROUND_GRADIENT_COLOR", theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_COLOR);
    theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_START = get_ini_int(muos_theme, "grid", "CELL_FOCUS_BACKGROUND_GRADIENT_START", theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_START);
    theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_STOP = get_ini_int(muos_theme, "grid", "CELL_FOCUS_BACKGROUND_GRADIENT_STOP", theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_STOP);
    theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_DIRECTION = get_ini_int(muos_theme, "grid", "CELL_FOCUS_BACKGROUND_GRADIENT_DIRECTION", theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_DIRECTION);
    theme->GRID.CELL_FOCUS.BORDER = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_BORDER", theme->GRID.CELL_FOCUS.BORDER);
    theme->GRID.CELL_FOCUS.BORDER_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_BORDER_ALPHA", theme->GRID.CELL_FOCUS.BORDER_ALPHA);
    theme->GRID.CELL_FOCUS.IMAGE_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_IMAGE_ALPHA", theme->GRID.CELL_FOCUS.IMAGE_ALPHA);
    theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_IMAGE_RECOLOUR", theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR);
    theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_IMAGE_RECOLOUR_ALPHA", theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR_ALPHA);
    theme->GRID.CELL_FOCUS.TEXT = get_ini_hex(muos_theme, "grid", "CELL_FOCUS_TEXT", theme->GRID.CELL_FOCUS.TEXT);
    theme->GRID.CELL_FOCUS.TEXT_ALPHA = get_ini_int(muos_theme, "grid", "CELL_FOCUS_TEXT_ALPHA", theme->GRID.CELL_FOCUS.TEXT_ALPHA);

    theme->LIST_DEFAULT.RADIUS = get_ini_int(muos_theme, "list", "LIST_DEFAULT_RADIUS", theme->LIST_DEFAULT.RADIUS);
    theme->LIST_DEFAULT.BACKGROUND = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_BACKGROUND", theme->LIST_DEFAULT.BACKGROUND);
    theme->LIST_DEFAULT.BACKGROUND_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_BACKGROUND_ALPHA", theme->LIST_DEFAULT.BACKGROUND_ALPHA);
    theme->LIST_DEFAULT.GRADIENT_START = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_START", theme->LIST_DEFAULT.GRADIENT_START);
    theme->LIST_DEFAULT.GRADIENT_STOP = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_STOP", theme->LIST_DEFAULT.GRADIENT_STOP);
    theme->LIST_DEFAULT.GRADIENT_DIRECTION = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GRADIENT_DIRECTION", theme->LIST_DEFAULT.GRADIENT_DIRECTION);
    theme->LIST_DEFAULT.BORDER_WIDTH = get_ini_int(muos_theme, "list", "LIST_DEFAULT_BORDER_WIDTH", theme->LIST_DEFAULT.BORDER_WIDTH);
    theme->LIST_DEFAULT.BORDER_SIDE = get_ini_int(muos_theme, "list", "LIST_DEFAULT_BORDER_SIDE", theme->LIST_DEFAULT.BORDER_SIDE);
    theme->LIST_DEFAULT.INDICATOR = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_INDICATOR", theme->LIST_DEFAULT.INDICATOR);
    theme->LIST_DEFAULT.INDICATOR_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_INDICATOR_ALPHA", theme->LIST_DEFAULT.INDICATOR_ALPHA);
    theme->LIST_DEFAULT.TEXT = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_TEXT", theme->LIST_DEFAULT.TEXT);
    theme->LIST_DEFAULT.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_TEXT_ALPHA", theme->LIST_DEFAULT.TEXT_ALPHA);
    theme->LIST_DEFAULT.BACKGROUND_GRADIENT = (theme->LIST_DEFAULT.GRADIENT_START == 255)
                                              ? theme->LIST_DEFAULT.BACKGROUND : theme->SYSTEM.BACKGROUND;
    theme->LIST_DEFAULT.GLYPH_PADDING_LEFT = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GLYPH_PAD_LEFT", theme->LIST_DEFAULT.GLYPH_PADDING_LEFT);
    theme->LIST_DEFAULT.GLYPH_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GLYPH_ALPHA", theme->LIST_DEFAULT.GLYPH_ALPHA);
    theme->LIST_DEFAULT.GLYPH_RECOLOUR = get_ini_hex(muos_theme, "list", "LIST_DEFAULT_GLYPH_RECOLOUR", theme->LIST_DEFAULT.GLYPH_RECOLOUR);
    theme->LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "list", "LIST_DEFAULT_GLYPH_RECOLOUR_ALPHA", theme->LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA);
    theme->LIST_DEFAULT.LABEL_LONG_MODE = get_ini_int(muos_theme, "list", "LIST_DEFAULT_LABEL_LONG_MODE",
                                                      theme->LIST_DEFAULT.LABEL_LONG_MODE);

    theme->LIST_DISABLED.TEXT = get_ini_hex(muos_theme, "list", "LIST_DISABLED_TEXT", theme->LIST_DISABLED.TEXT);
    theme->LIST_DISABLED.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_DISABLED_TEXT_ALPHA", theme->LIST_DISABLED.TEXT_ALPHA);

    theme->LIST_FOCUS.BACKGROUND = get_ini_hex(muos_theme, "list", "LIST_FOCUS_BACKGROUND", theme->LIST_FOCUS.BACKGROUND);
    theme->LIST_FOCUS.BACKGROUND_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_BACKGROUND_ALPHA", theme->LIST_FOCUS.BACKGROUND_ALPHA);
    theme->LIST_FOCUS.GRADIENT_START = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_START", theme->LIST_FOCUS.GRADIENT_START);
    theme->LIST_FOCUS.GRADIENT_STOP = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_STOP", theme->LIST_FOCUS.GRADIENT_STOP);
    theme->LIST_FOCUS.GRADIENT_DIRECTION = get_ini_int(muos_theme, "list", "LIST_FOCUS_GRADIENT_DIRECTION", theme->LIST_FOCUS.GRADIENT_DIRECTION);
    theme->LIST_FOCUS.BORDER_WIDTH = get_ini_int(muos_theme, "list", "LIST_FOCUS_BORDER_WIDTH", theme->LIST_FOCUS.BORDER_WIDTH);
    theme->LIST_FOCUS.BORDER_SIDE = get_ini_int(muos_theme, "list", "LIST_FOCUS_BORDER_SIDE", theme->LIST_FOCUS.BORDER_SIDE);
    theme->LIST_FOCUS.INDICATOR = get_ini_hex(muos_theme, "list", "LIST_FOCUS_INDICATOR", theme->LIST_FOCUS.INDICATOR);
    theme->LIST_FOCUS.INDICATOR_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_INDICATOR_ALPHA", theme->LIST_FOCUS.INDICATOR_ALPHA);
    theme->LIST_FOCUS.TEXT = get_ini_hex(muos_theme, "list", "LIST_FOCUS_TEXT", theme->LIST_FOCUS.TEXT);
    theme->LIST_FOCUS.TEXT_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_TEXT_ALPHA", theme->LIST_FOCUS.TEXT_ALPHA);
    theme->LIST_FOCUS.BACKGROUND_GRADIENT = (theme->LIST_FOCUS.GRADIENT_START == 255) ? theme->LIST_FOCUS.BACKGROUND
                                                                                      : theme->SYSTEM.BACKGROUND;
    theme->LIST_FOCUS.GLYPH_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_GLYPH_ALPHA", theme->LIST_FOCUS.GLYPH_ALPHA);
    theme->LIST_FOCUS.GLYPH_RECOLOUR = get_ini_hex(muos_theme, "list", "LIST_FOCUS_GLYPH_RECOLOUR", theme->LIST_FOCUS.GLYPH_RECOLOUR);
    theme->LIST_FOCUS.GLYPH_RECOLOUR_ALPHA = get_ini_int(muos_theme, "list", "LIST_FOCUS_GLYPH_RECOLOUR_ALPHA", theme->LIST_FOCUS.GLYPH_RECOLOUR_ALPHA);

    theme->IMAGE_LIST.ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_ALPHA", theme->IMAGE_LIST.ALPHA);
    theme->IMAGE_LIST.RADIUS = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RADIUS", theme->IMAGE_LIST.RADIUS);
    theme->IMAGE_LIST.RECOLOUR = get_ini_hex(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR", theme->IMAGE_LIST.RECOLOUR);
    theme->IMAGE_LIST.RECOLOUR_ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_RECOLOUR_ALPHA", theme->IMAGE_LIST.RECOLOUR_ALPHA);
    theme->IMAGE_LIST.PAD_TOP = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_TOP", theme->IMAGE_LIST.PAD_TOP);
    theme->IMAGE_LIST.PAD_BOTTOM = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_BOTTOM", theme->IMAGE_LIST.PAD_BOTTOM);
    theme->IMAGE_LIST.PAD_LEFT = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_LEFT", theme->IMAGE_LIST.PAD_LEFT);
    theme->IMAGE_LIST.PAD_RIGHT = get_ini_int(muos_theme, "image_list", "IMAGE_LIST_PAD_RIGHT", theme->IMAGE_LIST.PAD_RIGHT);

    theme->IMAGE_PREVIEW.ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_ALPHA", theme->IMAGE_PREVIEW.ALPHA);
    theme->IMAGE_PREVIEW.RADIUS = get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_RADIUS", theme->IMAGE_PREVIEW.RADIUS);
    theme->IMAGE_PREVIEW.RECOLOUR = get_ini_hex(muos_theme, "image_list", "IMAGE_PREVIEW_RECOLOUR", theme->IMAGE_PREVIEW.RECOLOUR);
    theme->IMAGE_PREVIEW.RECOLOUR_ALPHA = get_ini_int(muos_theme, "image_list", "IMAGE_PREVIEW_RECOLOUR_ALPHA", theme->IMAGE_PREVIEW.RECOLOUR_ALPHA);

    theme->CHARGER.BACKGROUND = get_ini_hex(muos_theme, "charging", "CHARGER_BACKGROUND", theme->CHARGER.BACKGROUND);
    theme->CHARGER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "charging", "CHARGER_BACKGROUND_ALPHA", theme->CHARGER.BACKGROUND_ALPHA);
    theme->CHARGER.TEXT = get_ini_hex(muos_theme, "charging", "CHARGER_TEXT", theme->CHARGER.TEXT);
    theme->CHARGER.TEXT_ALPHA = get_ini_int(muos_theme, "charging", "CHARGER_TEXT_ALPHA", theme->CHARGER.TEXT_ALPHA);
    theme->CHARGER.Y_POS = get_ini_int(muos_theme, "charging", "CHARGER_Y_POS", theme->CHARGER.Y_POS);

    theme->VERBOSE_BOOT.BACKGROUND = get_ini_hex(muos_theme, "verbose", "VERBOSE_BOOT_BACKGROUND", theme->VERBOSE_BOOT.BACKGROUND);
    theme->VERBOSE_BOOT.BACKGROUND_ALPHA = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_BACKGROUND_ALPHA", theme->VERBOSE_BOOT.BACKGROUND_ALPHA);
    theme->VERBOSE_BOOT.TEXT = get_ini_hex(muos_theme, "verbose", "VERBOSE_BOOT_TEXT", theme->VERBOSE_BOOT.TEXT);
    theme->VERBOSE_BOOT.TEXT_ALPHA = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_TEXT_ALPHA", theme->VERBOSE_BOOT.TEXT_ALPHA);
    theme->VERBOSE_BOOT.Y_POS = get_ini_int(muos_theme, "verbose", "VERBOSE_BOOT_Y_POS", theme->VERBOSE_BOOT.Y_POS);

    theme->OSK.BACKGROUND = get_ini_hex(muos_theme, "keyboard", "OSK_BACKGROUND", theme->OSK.BACKGROUND);
    theme->OSK.BACKGROUND_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_BACKGROUND_ALPHA", theme->OSK.BACKGROUND_ALPHA);
    theme->OSK.BORDER = get_ini_hex(muos_theme, "keyboard", "OSK_BORDER", theme->OSK.BORDER);
    theme->OSK.BORDER_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_BORDER_ALPHA", theme->OSK.BORDER_ALPHA);
    theme->OSK.RADIUS = get_ini_int(muos_theme, "keyboard", "OSK_RADIUS", theme->OSK.RADIUS);
    theme->OSK.TEXT = get_ini_hex(muos_theme, "keyboard", "OSK_TEXT", theme->OSK.TEXT);
    theme->OSK.TEXT_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_TEXT_ALPHA", theme->OSK.TEXT_ALPHA);
    theme->OSK.TEXT_FOCUS = get_ini_hex(muos_theme, "keyboard", "OSK_TEXT_FOCUS", theme->OSK.TEXT_FOCUS);
    theme->OSK.TEXT_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_TEXT_FOCUS_ALPHA", theme->OSK.TEXT_FOCUS_ALPHA);
    theme->OSK.ITEM.BACKGROUND = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND", theme->OSK.ITEM.BACKGROUND);
    theme->OSK.ITEM.BACKGROUND_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_ALPHA", theme->OSK.ITEM.BACKGROUND_ALPHA);
    theme->OSK.ITEM.BACKGROUND_FOCUS = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_FOCUS", theme->OSK.ITEM.BACKGROUND_FOCUS);
    theme->OSK.ITEM.BACKGROUND_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BACKGROUND_FOCUS_ALPHA",
                                                         theme->OSK.ITEM.BACKGROUND_FOCUS_ALPHA);
    theme->OSK.ITEM.BORDER = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BORDER", theme->OSK.ITEM.BORDER);
    theme->OSK.ITEM.BORDER_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BORDER_ALPHA", theme->OSK.ITEM.BORDER_ALPHA);
    theme->OSK.ITEM.BORDER_FOCUS = get_ini_hex(muos_theme, "keyboard", "OSK_ITEM_BORDER_FOCUS", theme->OSK.ITEM.BORDER_FOCUS);
    theme->OSK.ITEM.BORDER_FOCUS_ALPHA = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_BORDER_FOCUS_ALPHA", theme->OSK.ITEM.BORDER_FOCUS_ALPHA);
    theme->OSK.ITEM.RADIUS = get_ini_int(muos_theme, "keyboard", "OSK_ITEM_RADIUS", theme->OSK.ITEM.RADIUS);

    theme->MESSAGE.BACKGROUND = get_ini_hex(muos_theme, "notification", "MSG_BACKGROUND", theme->MESSAGE.BACKGROUND);
    theme->MESSAGE.BACKGROUND_ALPHA = get_ini_int(muos_theme, "notification", "MSG_BACKGROUND_ALPHA", theme->MESSAGE.BACKGROUND_ALPHA);
    theme->MESSAGE.BORDER = get_ini_hex(muos_theme, "notification", "MSG_BORDER", theme->MESSAGE.BORDER);
    theme->MESSAGE.BORDER_ALPHA = get_ini_int(muos_theme, "notification", "MSG_BORDER_ALPHA", theme->MESSAGE.BORDER_ALPHA);
    theme->MESSAGE.RADIUS = get_ini_int(muos_theme, "notification", "MSG_RADIUS", theme->MESSAGE.RADIUS);
    theme->MESSAGE.TEXT = get_ini_hex(muos_theme, "notification", "MSG_TEXT", theme->MESSAGE.TEXT);
    theme->MESSAGE.TEXT_ALPHA = get_ini_int(muos_theme, "notification", "MSG_TEXT_ALPHA", theme->MESSAGE.TEXT_ALPHA);

    theme->BAR.PANEL_WIDTH = get_ini_int(muos_theme, "bar", "BAR_WIDTH", theme->BAR.PANEL_WIDTH);
    theme->BAR.PANEL_HEIGHT = get_ini_int(muos_theme, "bar", "BAR_HEIGHT", theme->BAR.PANEL_HEIGHT);
    theme->BAR.PANEL_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_BACKGROUND", theme->BAR.PANEL_BACKGROUND);
    theme->BAR.PANEL_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_BACKGROUND_ALPHA", theme->BAR.PANEL_BACKGROUND_ALPHA);
    theme->BAR.PANEL_BORDER = get_ini_hex(muos_theme, "bar", "BAR_BORDER", theme->BAR.PANEL_BORDER);
    theme->BAR.PANEL_BORDER_ALPHA = get_ini_int(muos_theme, "bar", "BAR_BORDER_ALPHA", theme->BAR.PANEL_BORDER_ALPHA);
    theme->BAR.PANEL_BORDER_RADIUS = get_ini_int(muos_theme, "bar", "BAR_RADIUS", theme->BAR.PANEL_BORDER_RADIUS);
    theme->BAR.PROGRESS_WIDTH = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_WIDTH",
                                            theme->BAR.PROGRESS_WIDTH);
    theme->BAR.PROGRESS_HEIGHT = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_HEIGHT", theme->BAR.PROGRESS_HEIGHT);
    theme->BAR.PROGRESS_MAIN_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND", theme->BAR.PROGRESS_MAIN_BACKGROUND);
    theme->BAR.PROGRESS_MAIN_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_BACKGROUND_ALPHA", theme->BAR.PROGRESS_MAIN_BACKGROUND_ALPHA);
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND = get_ini_hex(muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND", theme->BAR.PROGRESS_ACTIVE_BACKGROUND);
    theme->BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_ACTIVE_BACKGROUND_ALPHA",
                                                              theme->BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA);
    theme->BAR.PROGRESS_RADIUS = get_ini_int(muos_theme, "bar", "BAR_PROGRESS_RADIUS", theme->BAR.PROGRESS_RADIUS);
    theme->BAR.ICON = get_ini_hex(muos_theme, "bar", "BAR_ICON", theme->BAR.ICON);
    theme->BAR.ICON_ALPHA = get_ini_int(muos_theme, "bar", "BAR_ICON_ALPHA", theme->BAR.ICON_ALPHA);
    theme->BAR.Y_POS = get_ini_int(muos_theme, "bar", "BAR_Y_POS", theme->BAR.Y_POS);

    theme->ROLL.TEXT = get_ini_hex(muos_theme, "roll", "ROLL_TEXT", theme->ROLL.TEXT);
    theme->ROLL.TEXT_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_TEXT_ALPHA", theme->ROLL.TEXT_ALPHA);
    theme->ROLL.BACKGROUND = get_ini_hex(muos_theme, "roll", "ROLL_BACKGROUND", theme->ROLL.BACKGROUND);
    theme->ROLL.BACKGROUND_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_BACKGROUND_ALPHA", theme->ROLL.BACKGROUND_ALPHA);
    theme->ROLL.RADIUS = get_ini_int(muos_theme, "roll", "ROLL_RADIUS", theme->ROLL.RADIUS);
    theme->ROLL.SELECT_TEXT = get_ini_hex(muos_theme, "roll", "ROLL_SELECT_TEXT", theme->ROLL.SELECT_TEXT);
    theme->ROLL.SELECT_TEXT_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_SELECT_TEXT_ALPHA", theme->ROLL.SELECT_TEXT_ALPHA);
    theme->ROLL.SELECT_BACKGROUND = get_ini_hex(muos_theme, "roll", "ROLL_SELECT_BACKGROUND", theme->ROLL.SELECT_BACKGROUND);
    theme->ROLL.SELECT_BACKGROUND_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_SELECT_BACKGROUND_ALPHA", theme->ROLL.SELECT_BACKGROUND_ALPHA);
    theme->ROLL.SELECT_RADIUS = get_ini_int(muos_theme, "roll", "ROLL_SELECT_RADIUS", theme->ROLL.SELECT_RADIUS);
    theme->ROLL.BORDER_COLOUR = get_ini_hex(muos_theme, "roll", "ROLL_BORDER_COLOUR", theme->ROLL.BORDER_COLOUR);
    theme->ROLL.BORDER_ALPHA = get_ini_int(muos_theme, "roll", "ROLL_BORDER_ALPHA", theme->ROLL.BORDER_ALPHA);
    theme->ROLL.BORDER_RADIUS = get_ini_int(muos_theme, "roll", "ROLL_BORDER_RADIUS", theme->ROLL.BORDER_RADIUS);

    theme->COUNTER.ALIGNMENT = get_ini_int(muos_theme, "counter", "COUNTER_ALIGNMENT", theme->COUNTER.ALIGNMENT);
    theme->COUNTER.PADDING_AROUND = get_ini_int(muos_theme, "counter", "COUNTER_PADDING_AROUND", theme->COUNTER.PADDING_AROUND);
    theme->COUNTER.PADDING_SIDE = get_ini_int(muos_theme, "counter", "COUNTER_PADDING_SIDE", theme->COUNTER.PADDING_SIDE);
    theme->COUNTER.PADDING_TOP = get_ini_int(muos_theme, "counter", "COUNTER_PADDING_TOP", theme->COUNTER.PADDING_TOP);
    theme->COUNTER.BORDER_COLOUR = get_ini_hex(muos_theme, "counter", "COUNTER_BORDER_COLOUR", theme->COUNTER.BORDER_COLOUR);
    theme->COUNTER.BORDER_ALPHA = get_ini_int(muos_theme, "counter", "COUNTER_BORDER_ALPHA", theme->COUNTER.BORDER_ALPHA);
    theme->COUNTER.BORDER_WIDTH = get_ini_int(muos_theme, "counter", "COUNTER_BORDER_WIDTH", theme->COUNTER.BORDER_WIDTH);
    theme->COUNTER.RADIUS = get_ini_int(muos_theme, "counter", "COUNTER_RADIUS", theme->COUNTER.RADIUS);
    theme->COUNTER.BACKGROUND = get_ini_hex(muos_theme, "counter", "COUNTER_BACKGROUND", theme->COUNTER.BACKGROUND);
    theme->COUNTER.BACKGROUND_ALPHA = get_ini_int(muos_theme, "counter", "COUNTER_BACKGROUND_ALPHA", theme->COUNTER.BACKGROUND_ALPHA);
    theme->COUNTER.TEXT = get_ini_hex(muos_theme, "counter", "COUNTER_TEXT", theme->COUNTER.TEXT);
    theme->COUNTER.TEXT_ALPHA = get_ini_int(muos_theme, "counter", "COUNTER_TEXT_ALPHA", theme->COUNTER.TEXT_ALPHA);
    theme->COUNTER.TEXT_FADE_TIME = get_ini_int(muos_theme, "counter", "COUNTER_TEXT_FADE_TIME", theme->COUNTER.TEXT_FADE_TIME);
    strncpy(theme->COUNTER.TEXT_SEPARATOR, get_ini_string(muos_theme, "counter", "COUNTER_TEXT_SEPARATOR", theme->COUNTER.TEXT_SEPARATOR),
            MAX_BUFFER_SIZE - 1);
    theme->COUNTER.TEXT_SEPARATOR[MAX_BUFFER_SIZE - 1] = '\0';

    theme->MISC.STATIC_ALIGNMENT = get_ini_int(muos_theme, "misc", "STATIC_ALIGNMENT", theme->MISC.STATIC_ALIGNMENT);
    theme->MUX.ITEM.COUNT = get_ini_int(muos_theme, "misc", "CONTENT_ITEM_COUNT", theme->MUX.ITEM.COUNT);
    theme->MUX.ITEM.HEIGHT = get_ini_int(muos_theme, "misc", "CONTENT_ITEM_HEIGHT", theme->MUX.ITEM.HEIGHT);
    theme->MISC.CONTENT.SIZE_TO_CONTENT = get_ini_int(muos_theme, "misc", "CONTENT_SIZE_TO_CONTENT", theme->MISC.CONTENT.SIZE_TO_CONTENT);
    theme->MISC.CONTENT.ALIGNMENT = get_ini_int(muos_theme, "misc", "CONTENT_ALIGNMENT", theme->MISC.CONTENT.ALIGNMENT);
    theme->MISC.CONTENT.PADDING_LEFT = get_ini_int(muos_theme, "misc", "CONTENT_PADDING_LEFT", theme->MISC.CONTENT.PADDING_LEFT);
    theme->MISC.CONTENT.PADDING_TOP = get_ini_int(muos_theme, "misc", "CONTENT_PADDING_TOP", theme->MISC.CONTENT.PADDING_TOP);
    theme->MISC.CONTENT.HEIGHT = get_ini_int(muos_theme, "misc", "CONTENT_HEIGHT", theme->MISC.CONTENT.HEIGHT);
    theme->MISC.CONTENT.WIDTH = get_ini_int(muos_theme, "misc", "CONTENT_WIDTH", theme->MISC.CONTENT.WIDTH);
    theme->MISC.ANIMATED_BACKGROUND = get_ini_int(muos_theme, "misc", "ANIMATED_BACKGROUND", theme->MISC.ANIMATED_BACKGROUND);
    theme->MISC.RANDOM_BACKGROUND = get_ini_int(muos_theme, "misc", "RANDOM_BACKGROUND", theme->MISC.RANDOM_BACKGROUND);
    theme->MISC.IMAGE_OVERLAY = get_ini_int(muos_theme, "misc", "IMAGE_OVERLAY", theme->MISC.IMAGE_OVERLAY);
    theme->MISC.NAVIGATION_TYPE = get_ini_int(muos_theme, "misc", "NAVIGATION_TYPE", theme->MISC.NAVIGATION_TYPE);
    theme->MISC.ANTIALIASING = get_ini_int(muos_theme, "misc", "ANTIALIASING", theme->MISC.ANTIALIASING);

    strncpy(theme->TERMINAL.FOREGROUND, get_ini_string(muos_theme, "terminal", "FOREGROUND", theme->TERMINAL.FOREGROUND),
            MAX_BUFFER_SIZE - 1);
    theme->TERMINAL.FOREGROUND[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(theme->TERMINAL.BACKGROUND, get_ini_string(muos_theme, "terminal", "BACKGROUND", theme->TERMINAL.BACKGROUND),
            MAX_BUFFER_SIZE - 1);
    theme->TERMINAL.BACKGROUND[MAX_BUFFER_SIZE - 1] = '\0';

    mini_free(muos_theme);
}

int get_alt_scheme_path(char *alt_scheme_path, size_t alt_scheme_path_size){
    char *theme_path;
    if (config.BOOT.FACTORY_RESET) {
        theme_path = INTERNAL_THEME;
    } else {
        theme_path = STORAGE_THEME;
    }

    char active_path[MAX_BUFFER_SIZE];
    snprintf(active_path, sizeof(active_path), "%s/active.txt", theme_path);
    if (file_exist(active_path)) {
        snprintf(alt_scheme_path, alt_scheme_path_size, "%s/alternate/%s.ini", theme_path, str_replace(read_line_from_file(active_path, 1), "\r", ""));
        return file_exist(alt_scheme_path);
    }
    return 0;
}

void load_theme(struct theme_config *theme, struct mux_config *config, struct mux_device *device) {
    char scheme[MAX_BUFFER_SIZE];
    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    // If theme does not support device resolution fallback to default but only after factory reset
    if (!config->BOOT.FACTORY_RESET) {
        if (config->SETTINGS.HDMI.ENABLED && read_int_from_file(device->SCREEN.HDMI, 1) &&
            config->SETTINGS.HDMI.THEME_RESOLUTION > 0) {
            device->MUX.WIDTH = config->SETTINGS.HDMI.THEME_RESOLUTION_WIDTH;
            device->MUX.HEIGHT = config->SETTINGS.HDMI.THEME_RESOLUTION_HEIGHT;
            get_mux_dimension(mux_dimension, sizeof(mux_dimension));
        }

        char theme_device_folder[MAX_BUFFER_SIZE];
        snprintf(theme_device_folder, sizeof(theme_device_folder), "%s/%s", STORAGE_THEME, mux_dimension);
        if (!directory_exist(theme_device_folder)) {
            device->MUX.WIDTH = 640;
            device->MUX.HEIGHT = 480;
            get_mux_dimension(mux_dimension, sizeof(mux_dimension));
        }
    }

    init_theme_config(theme, device);
    const char *schemes[] = {"global", "default", mux_module};
    int scheme_loaded = 0;

    if (theme_compat()) {
        for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); i++) {
            if (load_scheme(STORAGE_THEME, mux_dimension, schemes[i], scheme, sizeof(scheme))) {
                LOG_INFO(mux_module, "Loading STORAGE Theme Scheme: %s", scheme);
                scheme_loaded = 1;
                load_theme_from_scheme(scheme, theme, device);
            }
        }
        if (scheme_loaded) {
            char alternate_scheme_path[MAX_BUFFER_SIZE];
            if (get_alt_scheme_path(alternate_scheme_path, sizeof(alternate_scheme_path))) {
                load_theme_from_scheme(alternate_scheme_path, theme, device);
            }
        }
    }

    if (!scheme_loaded) {
        for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); i++) {
            if (load_scheme(INTERNAL_THEME, mux_dimension, schemes[i], scheme, sizeof(scheme))) {
                LOG_INFO(mux_module, "Loading INTERNAL Theme Scheme: %s", scheme)
                scheme_loaded = 1;
                load_theme_from_scheme(scheme, theme, device);
            }
        }
        if (scheme_loaded) {
            char alternate_scheme_path[MAX_BUFFER_SIZE];
            if (get_alt_scheme_path(alternate_scheme_path, sizeof(alternate_scheme_path))) {
                load_theme_from_scheme(alternate_scheme_path, theme, device);
            }
        }
    }

    char scheme_override[MAX_BUFFER_SIZE];
    snprintf(scheme_override, sizeof(scheme_override), (RUN_STORAGE_PATH "theme/override/%s.ini"), mux_module);
    if (file_exist(scheme_override)) {
        load_theme_from_scheme(scheme_override, theme, device);
    }

    theme->GRID.ENABLED = (theme->GRID.COLUMN_COUNT > 0 && theme->GRID.ROW_COUNT > 0);
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
}

void set_label_long_mode(struct theme_config *theme, lv_obj_t *ui_lblItem, char *item_text) {
    if (theme->LIST_DEFAULT.LABEL_LONG_MODE == LV_LABEL_LONG_WRAP) return;

    char *content_label = lv_label_get_text(ui_lblItem);

    size_t len = strlen(content_label);
    bool ends_with_ellipse = len > 3 && strcmp(&content_label[len - 3], "…") == 0;

    if (strcasecmp(item_text, content_label) != 0 && ends_with_ellipse) {
        lv_label_set_long_mode(ui_lblItem, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_label_set_text(ui_lblItem, item_text);
    }
}

void apply_text_long_dot(struct theme_config *theme, lv_obj_t *ui_pnlContent,
                         lv_obj_t *ui_lblItem, const char *item_text) {
    if (theme->LIST_DEFAULT.LABEL_LONG_MODE == LV_LABEL_LONG_WRAP) return;

    lv_label_set_long_mode(ui_lblItem, LV_LABEL_LONG_WRAP);
    const lv_font_t *font = lv_obj_get_style_text_font(ui_pnlContent, LV_PART_MAIN);
    const lv_coord_t letter_space = lv_obj_get_style_text_letter_space(ui_pnlContent, LV_PART_MAIN);
    lv_coord_t act_line_length = lv_txt_get_width(item_text, strlen(item_text), font, letter_space,
                                                  LV_TEXT_FLAG_EXPAND);
    int max_item_width = theme->MISC.CONTENT.WIDTH - theme->FONT.LIST_PAD_LEFT - theme->FONT.LIST_PAD_RIGHT -
                         (theme->LIST_DEFAULT.BORDER_WIDTH * 2);

    if (act_line_length > max_item_width) {
        int len = strlen(item_text);
        for (int i = len; i >= 0; i--) {
            char *new_string = (char *) malloc(i + 4);
            strncpy(new_string, item_text, i);
            new_string[i] = '\0';
            strcat(new_string, "…");

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
                                theme->MISC.CONTENT.WIDTH - (theme->LIST_DEFAULT.BORDER_WIDTH * 2));
        // When using size to content right padding needs to be zero to prevent text from wrapping.
        // The overall width of the control will include the right padding
        lv_obj_set_style_pad_right(ui_lblItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_width(ui_lblItem, item_width);
        lv_obj_set_x(ui_lblItemGlyph, theme->LIST_DEFAULT.GLYPH_PADDING_LEFT - (item_width / 2) -
                                      theme->LIST_DEFAULT.BORDER_WIDTH);
    }
}

void init_panel_style(struct theme_config *theme) {
    lv_style_init(&style_list_panel_default);
    lv_style_init(&style_list_panel_focused);

    // List panel default style
    lv_style_set_width(&style_list_panel_default, theme->MISC.CONTENT.WIDTH);
    lv_style_set_height(&style_list_panel_default, theme->MUX.ITEM.HEIGHT);

    lv_style_set_border_width(&style_list_panel_default, theme->LIST_DEFAULT.BORDER_WIDTH);
    lv_style_set_border_side(&style_list_panel_default, theme->LIST_DEFAULT.BORDER_SIDE);
    lv_style_set_border_color(&style_list_panel_default, lv_color_hex(theme->LIST_DEFAULT.INDICATOR));
    lv_style_set_border_opa(&style_list_panel_default, theme->LIST_DEFAULT.INDICATOR_ALPHA);

    lv_style_set_bg_main_stop(&style_list_panel_default, theme->LIST_DEFAULT.GRADIENT_START);
    lv_style_set_bg_color(&style_list_panel_default, lv_color_hex(theme->LIST_DEFAULT.BACKGROUND));
    lv_style_set_bg_opa(&style_list_panel_default, theme->LIST_DEFAULT.BACKGROUND_ALPHA);

    lv_style_set_bg_grad_color(&style_list_panel_default, lv_color_hex(theme->LIST_DEFAULT.BACKGROUND_GRADIENT));
    lv_style_set_bg_grad_dir(&style_list_panel_default,
                             (theme->LIST_DEFAULT.GRADIENT_DIRECTION == 1) ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR);
    lv_style_set_bg_grad_stop(&style_list_panel_default, theme->LIST_DEFAULT.GRADIENT_STOP);

    lv_style_set_pad_left(&style_list_panel_default, 0);
    lv_style_set_pad_right(&style_list_panel_default, 0);
    lv_style_set_pad_top(&style_list_panel_default, 0);
    lv_style_set_pad_bottom(&style_list_panel_default, 0);
    lv_style_set_pad_row(&style_list_panel_default, 0);
    lv_style_set_pad_column(&style_list_panel_default, 0);

    lv_style_set_radius(&style_list_panel_default, theme->LIST_DEFAULT.RADIUS);

    // List panel focused style
    lv_style_set_border_width(&style_list_panel_focused, theme->LIST_FOCUS.BORDER_WIDTH);
    lv_style_set_border_side(&style_list_panel_focused, theme->LIST_FOCUS.BORDER_SIDE);
    lv_style_set_border_color(&style_list_panel_focused, lv_color_hex(theme->LIST_FOCUS.INDICATOR));
    lv_style_set_border_opa(&style_list_panel_focused, theme->LIST_FOCUS.INDICATOR_ALPHA);

    lv_style_set_bg_main_stop(&style_list_panel_focused, theme->LIST_FOCUS.GRADIENT_START);
    lv_style_set_bg_color(&style_list_panel_focused, lv_color_hex(theme->LIST_FOCUS.BACKGROUND));
    lv_style_set_bg_opa(&style_list_panel_focused, theme->LIST_FOCUS.BACKGROUND_ALPHA);

    lv_style_set_bg_grad_color(&style_list_panel_focused, lv_color_hex(theme->LIST_FOCUS.BACKGROUND_GRADIENT));
    lv_style_set_bg_grad_dir(&style_list_panel_focused,
                             (theme->LIST_FOCUS.GRADIENT_DIRECTION == 1) ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR);
    lv_style_set_bg_grad_stop(&style_list_panel_focused, theme->LIST_FOCUS.GRADIENT_STOP);
}

void init_item_animation() {
    lv_anim_init(&style_list_item_animation);
    lv_anim_set_delay(&style_list_item_animation, 250);
    lv_style_set_anim(&style_list_item_default, &style_list_item_animation);
    lv_style_set_anim_speed(&style_list_item_default, 70);
}

void init_item_style(struct theme_config *theme) {
    lv_style_init(&style_list_item_default);
    lv_style_init(&style_list_item_focused);

    lv_style_set_width(&style_list_item_default, theme->MISC.CONTENT.WIDTH - theme->FONT.LIST_PAD_RIGHT);
    lv_style_set_align(&style_list_item_default, LV_ALIGN_LEFT_MID);

    lv_style_set_text_color(&style_list_item_default, lv_color_hex(theme->LIST_DEFAULT.TEXT));
    lv_style_set_text_opa(&style_list_item_default, theme->LIST_DEFAULT.TEXT_ALPHA);

    lv_style_set_pad_left(&style_list_item_default, theme->FONT.LIST_PAD_LEFT);
    lv_style_set_pad_top(&style_list_item_default, theme->FONT.LIST_PAD_TOP);

    lv_style_set_text_color(&style_list_item_focused, lv_color_hex(theme->LIST_FOCUS.TEXT));
    lv_style_set_text_opa(&style_list_item_focused, theme->LIST_FOCUS.TEXT_ALPHA);
}

void init_glyph_style(struct theme_config *theme) {
    lv_style_init(&style_list_glyph_default);
    lv_style_init(&style_list_glyph_focused);

    lv_style_set_x(&style_list_glyph_default, theme->LIST_DEFAULT.GLYPH_PADDING_LEFT - (theme->MISC.CONTENT.WIDTH / 2));
    lv_style_set_align(&style_list_glyph_default, LV_ALIGN_CENTER);

    lv_style_set_img_opa(&style_list_glyph_default, theme->LIST_DEFAULT.GLYPH_ALPHA);
    lv_style_set_img_recolor(&style_list_glyph_default, lv_color_hex(theme->LIST_DEFAULT.GLYPH_RECOLOUR));
    lv_style_set_img_recolor_opa(&style_list_glyph_default, theme->LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA);

    lv_style_set_pad_top(&style_list_glyph_default, theme->FONT.LIST_ICON_PAD_TOP);
    lv_style_set_pad_bottom(&style_list_glyph_default, theme->FONT.LIST_ICON_PAD_BOTTOM);

    lv_style_set_img_opa(&style_list_glyph_focused, theme->LIST_FOCUS.GLYPH_ALPHA);
    lv_style_set_img_recolor(&style_list_glyph_focused, lv_color_hex(theme->LIST_FOCUS.GLYPH_RECOLOUR));
    lv_style_set_img_recolor_opa(&style_list_glyph_focused, theme->LIST_FOCUS.GLYPH_RECOLOUR_ALPHA);
}

void apply_theme_list_panel(lv_obj_t *ui_pnlList) {
    lv_obj_set_scrollbar_mode(ui_pnlList, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_style(ui_pnlList, &style_list_panel_default, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_pnlList, &style_list_panel_focused, LV_PART_MAIN | LV_STATE_FOCUSED);
}

void apply_theme_list_item(struct theme_config *theme, lv_obj_t *ui_lblItem, const char *item_text) {
    lv_label_set_text(ui_lblItem, item_text);
    lv_label_set_long_mode(ui_lblItem, LV_LABEL_LONG_WRAP);

    lv_obj_add_style(ui_lblItem, &style_list_item_default, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_lblItem, &style_list_item_focused, LV_PART_MAIN | LV_STATE_FOCUSED);

    if (theme->LIST_DEFAULT.LABEL_LONG_MODE == LV_LABEL_LONG_WRAP) {
        lv_obj_set_height(ui_lblItem, LV_SIZE_CONTENT);
    } else {
        lv_obj_set_height(ui_lblItem, lv_font_get_line_height(lv_obj_get_style_text_font(ui_lblItem, LV_PART_MAIN)));
    }
}

void apply_theme_list_glyph(struct theme_config *theme, lv_obj_t *ui_lblItemGlyph,
                            const char *screen_name, char *item_glyph) {
    if (theme->LIST_DEFAULT.GLYPH_ALPHA == 0 && theme->LIST_FOCUS.GLYPH_ALPHA == 0) return;

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (get_glyph_path(screen_name, item_glyph, glyph_image_embed, MAX_BUFFER_SIZE)) {
        lv_img_set_src(ui_lblItemGlyph, glyph_image_embed);
    }

    lv_obj_add_style(ui_lblItemGlyph, &style_list_glyph_default, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_lblItemGlyph, &style_list_glyph_focused, LV_PART_MAIN | LV_STATE_FOCUSED);
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

void apply_pass_theme(lv_obj_t *ui_rolComboOne, lv_obj_t *ui_rolComboTwo, lv_obj_t *ui_rolComboThree,
                      lv_obj_t *ui_rolComboFour, lv_obj_t *ui_rolComboFive, lv_obj_t *ui_rolComboSix) {
    struct pt_big roll_text_elements[] = {
            {ui_rolComboOne,   theme.ROLL.TEXT},
            {ui_rolComboTwo,   theme.ROLL.TEXT},
            {ui_rolComboThree, theme.ROLL.TEXT},
            {ui_rolComboFour,  theme.ROLL.TEXT},
            {ui_rolComboFive,  theme.ROLL.TEXT},
            {ui_rolComboSix,   theme.ROLL.TEXT},
    };
    for (size_t i = 0; i < sizeof(roll_text_elements) / sizeof(roll_text_elements[0]); ++i) {
        lv_obj_set_style_text_color(roll_text_elements[i].e, lv_color_hex(roll_text_elements[i].c),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct pt_big roll_selected_text_elements[] = {
            {ui_rolComboOne,   theme.ROLL.SELECT_TEXT},
            {ui_rolComboTwo,   theme.ROLL.SELECT_TEXT},
            {ui_rolComboThree, theme.ROLL.SELECT_TEXT},
            {ui_rolComboFour,  theme.ROLL.SELECT_TEXT},
            {ui_rolComboFive,  theme.ROLL.SELECT_TEXT},
            {ui_rolComboSix,   theme.ROLL.SELECT_TEXT},
    };
    for (size_t i = 0; i < sizeof(roll_selected_text_elements) / sizeof(roll_selected_text_elements[0]); ++i) {
        lv_obj_set_style_text_color(roll_selected_text_elements[i].e, lv_color_hex(roll_selected_text_elements[i].c),
                                    LV_PART_SELECTED | LV_STATE_DEFAULT);
    }

    struct pt_big roll_text_alpha_elements[] = {
            {ui_rolComboOne,   theme.ROLL.TEXT_ALPHA},
            {ui_rolComboTwo,   theme.ROLL.TEXT_ALPHA},
            {ui_rolComboThree, theme.ROLL.TEXT_ALPHA},
            {ui_rolComboFour,  theme.ROLL.TEXT_ALPHA},
            {ui_rolComboFive,  theme.ROLL.TEXT_ALPHA},
            {ui_rolComboSix,   theme.ROLL.TEXT_ALPHA},
    };
    for (size_t i = 0; i < sizeof(roll_text_alpha_elements) / sizeof(roll_text_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(roll_text_alpha_elements[i].e, roll_text_alpha_elements[i].c,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    struct pt_big roll_selected_text_alpha_elements[] = {
            {ui_rolComboOne,   theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboTwo,   theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboThree, theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboFour,  theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboFive,  theme.ROLL.SELECT_TEXT_ALPHA},
            {ui_rolComboSix,   theme.ROLL.SELECT_TEXT_ALPHA},
    };
    for (size_t i = 0;
         i < sizeof(roll_selected_text_alpha_elements) / sizeof(roll_selected_text_alpha_elements[0]); ++i) {
        lv_obj_set_style_text_opa(roll_selected_text_alpha_elements[i].e, roll_selected_text_alpha_elements[i].c,
                                  LV_PART_SELECTED | LV_STATE_DEFAULT);
    }

    struct pt_big roll_background_elements[] = {
            {ui_rolComboOne,   theme.ROLL.BACKGROUND},
            {ui_rolComboTwo,   theme.ROLL.BACKGROUND},
            {ui_rolComboThree, theme.ROLL.BACKGROUND},
            {ui_rolComboFour,  theme.ROLL.BACKGROUND},
            {ui_rolComboFive,  theme.ROLL.BACKGROUND},
            {ui_rolComboSix,   theme.ROLL.BACKGROUND},
    };
    for (size_t i = 0; i < sizeof(roll_background_elements) / sizeof(roll_background_elements[0]); ++i) {
        lv_obj_set_style_bg_color(roll_background_elements[i].e, lv_color_hex(roll_background_elements[i].c),
                                  LV_PART_SELECTED | LV_STATE_DEFAULT);
    }

    struct pt_small roll_background_alpha_elements[] = {
            {ui_rolComboOne,   theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboTwo,   theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboThree, theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboFour,  theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboFive,  theme.ROLL.BACKGROUND_ALPHA},
            {ui_rolComboSix,   theme.ROLL.BACKGROUND_ALPHA},
    };
    for (size_t i = 0; i < sizeof(roll_background_alpha_elements) / sizeof(roll_background_alpha_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(roll_background_alpha_elements[i].e, roll_background_alpha_elements[i].c,
                                LV_PART_SELECTED | LV_STATE_DEFAULT);
    }

    struct pt_big roll_selected_background_elements[] = {
            {ui_rolComboOne,   theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboTwo,   theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboThree, theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboFour,  theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboFive,  theme.ROLL.SELECT_BACKGROUND},
            {ui_rolComboSix,   theme.ROLL.SELECT_BACKGROUND},
    };
    for (size_t i = 0;
         i < sizeof(roll_selected_background_elements) / sizeof(roll_selected_background_elements[0]); ++i) {
        lv_obj_set_style_bg_color(roll_selected_background_elements[i].e,
                                  lv_color_hex(roll_selected_background_elements[i].c),
                                  LV_PART_SELECTED | LV_STATE_FOCUSED);
    }

    struct pt_small roll_selected_background_alpha_elements[] = {
            {ui_rolComboOne,   theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboTwo,   theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboThree, theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboFour,  theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboFive,  theme.ROLL.SELECT_BACKGROUND_ALPHA},
            {ui_rolComboSix,   theme.ROLL.SELECT_BACKGROUND_ALPHA},
    };
    for (size_t i = 0; i < sizeof(roll_selected_background_alpha_elements) /
                           sizeof(roll_selected_background_alpha_elements[0]); ++i) {
        lv_obj_set_style_bg_opa(roll_selected_background_alpha_elements[i].e,
                                roll_selected_background_alpha_elements[i].c,
                                LV_PART_SELECTED | LV_STATE_FOCUSED);
    }

    struct pt_small roll_radius_elements[] = {
            {ui_rolComboOne,   theme.ROLL.RADIUS},
            {ui_rolComboTwo,   theme.ROLL.RADIUS},
            {ui_rolComboThree, theme.ROLL.RADIUS},
            {ui_rolComboFour,  theme.ROLL.RADIUS},
            {ui_rolComboFive,  theme.ROLL.RADIUS},
            {ui_rolComboSix,   theme.ROLL.RADIUS},
    };
    for (size_t i = 0; i < sizeof(roll_radius_elements) / sizeof(roll_radius_elements[0]); ++i) {
        lv_obj_set_style_radius(roll_radius_elements[i].e, roll_radius_elements[i].c,
                                LV_PART_SELECTED | LV_STATE_DEFAULT);
    }
    struct pt_small roll_selected_radius_elements[] = {
            {ui_rolComboOne,   theme.ROLL.SELECT_RADIUS},
            {ui_rolComboTwo,   theme.ROLL.SELECT_RADIUS},
            {ui_rolComboThree, theme.ROLL.SELECT_RADIUS},
            {ui_rolComboFour,  theme.ROLL.SELECT_RADIUS},
            {ui_rolComboFive,  theme.ROLL.SELECT_RADIUS},
            {ui_rolComboSix,   theme.ROLL.SELECT_RADIUS},
    };
    for (size_t i = 0; i < sizeof(roll_selected_radius_elements) / sizeof(roll_selected_radius_elements[0]); ++i) {
        lv_obj_set_style_radius(roll_selected_radius_elements[i].e, roll_selected_radius_elements[i].c,
                                LV_PART_SELECTED | LV_STATE_FOCUSED);
    }

    struct pt_small roll_border_radius_elements[] = {
            {ui_rolComboOne,   theme.ROLL.BORDER_RADIUS},
            {ui_rolComboTwo,   theme.ROLL.BORDER_RADIUS},
            {ui_rolComboThree, theme.ROLL.BORDER_RADIUS},
            {ui_rolComboFour,  theme.ROLL.BORDER_RADIUS},
            {ui_rolComboFive,  theme.ROLL.BORDER_RADIUS},
            {ui_rolComboSix,   theme.ROLL.BORDER_RADIUS},
    };
    for (size_t i = 0; i < sizeof(roll_border_radius_elements) / sizeof(roll_border_radius_elements[0]); ++i) {
        lv_obj_set_style_radius(roll_border_radius_elements[i].e, roll_border_radius_elements[i].c,
                                LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    struct pt_big roll_border_colour_elements[] = {
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

    struct pt_small roll_border_alpha_elements[] = {
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
