#ifndef _MUXOPTION_UI_H
#define _MUXOPTION_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl/lvgl.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_scrOption;
extern lv_obj_t *ui_pnlCore;
extern lv_obj_t *ui_pnlGovernor;
extern lv_obj_t *ui_lblCore;
extern lv_obj_t *ui_lblGovernor;
extern lv_obj_t *ui_icoCore;
extern lv_obj_t *ui_icoGovernor;

void ui_init(lv_obj_t *ui_pnlContent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
