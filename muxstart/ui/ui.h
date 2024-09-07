#ifndef _MUXSTART_UI_H
#define _MUXSTART_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl/lvgl.h"

void ui_scrStart_screen_init(void);

extern lv_obj_t *ui_scrStart;
extern lv_obj_t *ui_pnlWall;
extern lv_obj_t *ui_imgWall;
extern lv_obj_t *ui_pnlMessage;
extern lv_obj_t *ui_lblMessage;
extern lv_obj_t *ui____initial_actions0;

LV_IMG_DECLARE(ui_img_nothing_png);

LV_FONT_DECLARE(ui_font_AwesomeSmall);
LV_FONT_DECLARE(ui_font_Gamepad);
LV_FONT_DECLARE(ui_font_NotoSans);

void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
