#pragma once

#include <common/display/theme.h>

#define MENU_ITEM_COUNT 8

enum menu_style {
    menu_style_theme = 0,
    menu_style_list,
    menu_style_grid,
    menu_style_carousel,
    menu_style_coverflow,
    menu_style_bubbles,
    menu_style_orbit,
    menu_style_dial,
    menu_style_stack,
    menu_style_clock,
    menu_style_rolodex,
    menu_style_dock,
    menu_style_fan,
    menu_style_honeycomb,
    menu_style_tiles,
    menu_style_sidebar,
    menu_style_fluent,
    menu_style_spotlight,
    menu_style_banners,
    menu_style_crossbar,
    menu_style_desktop,
    menu_style_tabbar,
    menu_style_blades,
};

int menu_style_active(void);

const int *menu_item_order(void);

int menu_style_any_direction(void);

int menu_style_spatial(void);

void menu_style_apply(struct theme_config *theme, int width, int height);

const char *menu_style_layout_json(void);
