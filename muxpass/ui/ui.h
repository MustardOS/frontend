#ifndef _MUXPASS_UI_H
#define _MUXPASS_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl/lvgl.h"

void ui_scrPass_screen_init(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_rolComboOne;
extern lv_obj_t *ui_rolComboTwo;
extern lv_obj_t *ui_rolComboThree;
extern lv_obj_t *ui_rolComboFour;
extern lv_obj_t *ui_rolComboFive;
extern lv_obj_t *ui_rolComboSix;

LV_FONT_DECLARE(ui_font_NotoSansBig);

void ui_init(lv_obj_t *ui_pnlContent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
