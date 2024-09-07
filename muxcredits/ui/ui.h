#ifndef _MUXCREDITS_UI_H
#define _MUXCREDITS_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl/lvgl.h"

void animScroll_Animation(lv_obj_t *TargetObject, int delay);

void animFade_Animation(lv_obj_t *TargetObject, int delay);

void ui_scrCredits_screen_init(void);

extern lv_obj_t *ui_scrCredits;
extern lv_obj_t *ui_conCredits;
extern lv_obj_t *ui_conStart;
extern lv_obj_t *ui_lblStartTitle;
extern lv_obj_t *ui_lblStartMessage;
extern lv_obj_t *ui_conScroll;
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
extern lv_obj_t *ui_conKofi;
extern lv_obj_t *ui_lblKofiTitle;
extern lv_obj_t *ui_lblKofiMessageOne;
extern lv_obj_t *ui_lblKofiMessageTwo;
extern lv_obj_t *ui_imgKofi;
extern lv_obj_t *ui____initial_actions0;

LV_IMG_DECLARE(ui_img_muoskofi_png);
LV_IMG_DECLARE(ui_img_nothing_png);

LV_FONT_DECLARE(ui_font_AwesomeSmall);
LV_FONT_DECLARE(ui_font_GamepadNav);
LV_FONT_DECLARE(ui_font_NotoSans);
LV_FONT_DECLARE(ui_font_NotoSansBig);

typedef struct _ui_anim_user_data_t {
    lv_obj_t *target;
    lv_img_dsc_t **imgset;
    int32_t imgset_size;
    int32_t val;
} ui_anim_user_data_t;

void _ui_anim_callback_free_user_data(lv_anim_t *a);

void _ui_anim_callback_set_x(lv_anim_t *a, int32_t v);

void _ui_anim_callback_set_y(lv_anim_t *a, int32_t v);

void _ui_anim_callback_set_width(lv_anim_t *a, int32_t v);

void _ui_anim_callback_set_height(lv_anim_t *a, int32_t v);

void _ui_anim_callback_set_opacity(lv_anim_t *a, int32_t v);

void _ui_anim_callback_set_image_zoom(lv_anim_t *a, int32_t v);

void _ui_anim_callback_set_image_angle(lv_anim_t *a, int32_t v);

void _ui_anim_callback_set_image_frame(lv_anim_t *a, int32_t v);

int32_t _ui_anim_callback_get_x(lv_anim_t *a);

int32_t _ui_anim_callback_get_y(lv_anim_t *a);

int32_t _ui_anim_callback_get_width(lv_anim_t *a);

int32_t _ui_anim_callback_get_height(lv_anim_t *a);

int32_t _ui_anim_callback_get_opacity(lv_anim_t *a);

int32_t _ui_anim_callback_get_image_zoom(lv_anim_t *a);

int32_t _ui_anim_callback_get_image_angle(lv_anim_t *a);

int32_t _ui_anim_callback_get_image_frame(lv_anim_t *a);

void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
