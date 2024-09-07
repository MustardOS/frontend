#ifndef _MUXSPLASH_UI_H
#define _MUXSPLASH_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl/lvgl.h"

void ui_scrSplash_screen_init(void);

extern lv_obj_t *ui_scrSplash;
extern lv_obj_t *ui____initial_actions0;

LV_FONT_DECLARE(ui_font_AwesomeSmall);
LV_FONT_DECLARE(ui_font_GamepadNav);
LV_FONT_DECLARE(ui_font_NotoSans);

void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
