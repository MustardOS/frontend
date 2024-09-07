#ifndef _MUXCHARGE_UI_H
#define _MUXCHARGE_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl/lvgl.h"

void ui_scrCharge_screen_init(void);

extern lv_obj_t *ui_scrCharge;
extern lv_obj_t *ui_pnlWall;
extern lv_obj_t *ui_imgWall;
extern lv_obj_t *ui_pnlCharge;
extern lv_obj_t *ui_lblCapacity;
extern lv_obj_t *ui_lblVoltage;
extern lv_obj_t *ui_lblHealth;
extern lv_obj_t *ui_lblBoot;
extern lv_obj_t *ui____initial_actions0;

LV_IMG_DECLARE(ui_img_nothing_png);

LV_FONT_DECLARE(ui_font_AwesomeSmall);
LV_FONT_DECLARE(ui_font_GamepadNav);
LV_FONT_DECLARE(ui_font_NotoSans);

void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
