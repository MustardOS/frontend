#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

lv_obj_t *ui_pnlTweakGeneral;
lv_obj_t *ui_pnlTheme;
lv_obj_t *ui_pnlNetwork;
lv_obj_t *ui_pnlServices;
lv_obj_t *ui_pnlRTC;
lv_obj_t *ui_pnlLanguage;
lv_obj_t *ui_pnlStorage;
lv_obj_t *ui_lblTweakGeneral;
lv_obj_t *ui_lblTheme;
lv_obj_t *ui_lblNetwork;
lv_obj_t *ui_lblServices;
lv_obj_t *ui_lblRTC;
lv_obj_t *ui_lblLanguage;
lv_obj_t *ui_lblStorage;
lv_obj_t *ui_icoTweakGeneral;
lv_obj_t *ui_icoTheme;
lv_obj_t *ui_icoNetwork;
lv_obj_t *ui_icoServices;
lv_obj_t *ui_icoRTC;
lv_obj_t *ui_icoLanguage;
lv_obj_t *ui_icoStorage;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_screen_init(ui_pnlContent);
}
