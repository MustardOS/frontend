#ifndef _MUXPLORE_UI_H
#define _MUXPLORE_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

extern lv_obj_t *ui_lblCounter;

void ui_init(lv_obj_t *ui_screen, struct theme_config *theme);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
