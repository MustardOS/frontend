#pragma once

#include <lvgl/lvgl.h>
#include "config.h"

typedef enum {
    MENU_CHANGE_NONE = 0,
    MENU_CHANGE_FONT = 1,
    MENU_CHANGE_COLOUR = 2,
} MenuChange;

void menu_init(MuxtermConfig *cfg);

void menu_open(void);

void menu_close(void);

int menu_is_active(void);

void menu_move(int direction);

void menu_adjust(int direction);

void menu_select(void);

void menu_reset(void);

void menu_show_nav(void);

int menu_take_quit(void);

int menu_take_reset(void);

MenuChange menu_take_change(void);
