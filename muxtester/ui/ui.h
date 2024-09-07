#ifndef _MUXTESTER_UI_H
#define _MUXTESTER_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl/lvgl.h"

void ui_scrTester_screen_init(lv_obj_t * ui_pnlContent);
extern lv_obj_t * ui_lblButton;
extern lv_obj_t * ui_lblFirst;

LV_FONT_DECLARE(ui_font_Gamepad);

void ui_init(lv_obj_t * ui_pnlContent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
