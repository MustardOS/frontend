#pragma once

#include "submenu.h"

const submenu_def *video_menu_definition(void);

const submenu_def *visuals_menu_definition(void);

const submenu_def *overlay_menu_definition(void);

const submenu_def *viewport_menu_definition(void);

const submenu_def *hud_menu_definition(void);

const submenu_def *sound_menu_definition(void);

const submenu_def *input_menu_definition(void);

const submenu_def *storage_menu_definition(void);

const submenu_def *performance_menu_definition(void);

const submenu_def *cheevo_settings_definition(void);

int cheevo_settings_child_tick(void);

int viewport_settings_child_tick(void);
