#pragma once

#include <common/display/theme.h>

#define MENU_ITEM_COUNT 8

enum menu_layout_option {
    layout_size,
    layout_spacing,
    layout_tilt,
    layout_swell,
    layout_position,
    layout_pile,
    layout_names,
    layout_large,
    layout_fit,
    layout_glyph,
    layout_text,
    layout_focus,
    layout_perch,
    layout_slicetext,
    layout_bounce,
    layout_gap,
    layout_slant,
    layout_textx,
    layout_textbg,
    layout_texty,
    layout_textangle,
    layout_namex,
    layout_namey,
    layout_nameangle,
    layout_namebg,
    layout_mascotscale,
    layout_radius,
    layout_glyphscale,
    layout_namewidth,
    LAYOUT_OPTION_COUNT
};

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
    menu_style_zigzag,
    menu_style_tiles,
    menu_style_sidebar,
    menu_style_fluent,
    menu_style_spotlight,
    menu_style_banners,
    menu_style_crossbar,
    menu_style_desktop,
    menu_style_tabbar,
    menu_style_paged,
    menu_style_slices,
    menu_style_mascot,
};

int menu_style_active(void);

int menu_name_angle(void);

int menu_style_card_art(void);

int menu_style_index(const char *name);

int menu_style_named_for(const char *theme_path);

int menu_layout_choices(int option);

int menu_style_current(void);

#define MENU_ARRANGEMENT_MAX 3

int menu_style_resolve(int style);

int menu_style_base(int style);

int menu_style_arrangements(int base, int *styles);

int menu_style_customised(int style);

int menu_style_reset_layout(int style);

const int *menu_item_order(void);

int menu_style_any_direction(void);

int menu_style_spatial(void);

int menu_style_blocks(int vertical);

int menu_layout_options(void);

int menu_style_paged_active(void);

int menu_layout_value(int option);

void menu_layout_edit(int editing);

void menu_layout_step(int option, int direction);

int menu_layout_save(int target);

const char *menu_style_key(int style);

void menu_layout_default(int option);

int menu_layout_reset_all(void);

void menu_style_apply(struct theme_config *theme, int width, int height);

const char *menu_style_layout_json(void);
