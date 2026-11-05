#pragma once

#include <common/display/theme.h>

enum menu_style {
    menu_style_theme = 0,
    menu_style_list,
    menu_style_grid,
    menu_style_carousel,
    menu_style_coverflow,
    menu_style_bubbles,
    menu_style_orbit,
};

int menu_style_active(void);

void menu_style_apply(struct theme_config *theme, int width, int height);

const char *menu_style_layout_json(void);
