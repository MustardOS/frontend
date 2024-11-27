#pragma once

#include "../../lvgl/lvgl.h"

void animScroll_Animation(lv_obj_t *TargetObject, int delay);

void animFade_Animation(lv_obj_t *TargetObject, int delay);

void ui_init(void);

extern lv_obj_t *ui_scrCredits;

extern lv_obj_t *ui_conCredits;
extern lv_obj_t *ui_conStart;
extern lv_obj_t *ui_conScroll;
extern lv_obj_t *ui_conSpecial;
extern lv_obj_t *ui_conKofi;

extern lv_obj_t *ui_imgKofi;

extern lv_obj_t *ui_lblStartTitle;
extern lv_obj_t *ui_lblStartMessage;

extern lv_obj_t *ui_lblOfficialTitle;

extern lv_obj_t *ui_lblCommanderTitle;
extern lv_obj_t *ui_lblCommanderPeople;

extern lv_obj_t *ui_lblEnforcerTitle;
extern lv_obj_t *ui_lblEnforcerPeople;

extern lv_obj_t *ui_lblWizardTitle;
extern lv_obj_t *ui_lblWizardLeft;
extern lv_obj_t *ui_lblWizardRight;

extern lv_obj_t *ui_lblHeroTitle;
extern lv_obj_t *ui_lblHeroLeft;
extern lv_obj_t *ui_lblHeroRight;

extern lv_obj_t *ui_lblKnightTitle;
extern lv_obj_t *ui_lblKnightLeft;
extern lv_obj_t *ui_lblKnightRight;

extern lv_obj_t *ui_lblSpecialTitle;
extern lv_obj_t *ui_lblSpecialMid;

extern lv_obj_t *ui_lblBongleTitle;
extern lv_obj_t *ui_lblBongleMid;

extern lv_obj_t *ui_lblKofiTitle;
extern lv_obj_t *ui_lblKofiMessageOne;
extern lv_obj_t *ui_lblKofiMessageTwo;


LV_IMG_DECLARE(ui_image_Kofi)
LV_IMG_DECLARE(ui_image_Nothing)

typedef struct ui_anim_user_data_t {
    lv_obj_t *target;
    int32_t val;
} ui_anim_user_data_t;

void ui_anim_callback_free_user_data(lv_anim_t *a);

void ui_anim_callback_set_y(lv_anim_t *a, int32_t v);

void ui_anim_callback_set_opacity(lv_anim_t *a, int32_t v);

int32_t ui_anim_callback_get_y(lv_anim_t *a);

int32_t ui_anim_callback_get_opacity(lv_anim_t *a);
