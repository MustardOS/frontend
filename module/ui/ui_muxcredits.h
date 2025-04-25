#pragma once

#include "../../lvgl/lvgl.h"

void animFade_Animation(lv_obj_t *TargetObject, int delay);

void init_muxcredits(const lv_font_t *header_font);

extern lv_obj_t *ui_scrCredits;

extern lv_obj_t *ui_conStart;
extern lv_obj_t *ui_conOfficial;
extern lv_obj_t *ui_conWizard;
extern lv_obj_t *ui_conHeroOne;
extern lv_obj_t *ui_conHeroTwo;
extern lv_obj_t *ui_conKnightOne;
extern lv_obj_t *ui_conKnightTwo;
extern lv_obj_t *ui_conSpecial;
extern lv_obj_t *ui_conKofi;
extern lv_obj_t *ui_conMusic;

LV_IMG_DECLARE(ui_image_Kofi)
LV_IMG_DECLARE(ui_image_Nothing)
