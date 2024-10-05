#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

lv_obj_t *ui_pnlHidden;
lv_obj_t *ui_pnlBGM;
lv_obj_t *ui_pnlSound;
lv_obj_t *ui_pnlStartup;
lv_obj_t *ui_pnlColour;
lv_obj_t *ui_pnlBrightness;
lv_obj_t *ui_pnlHDMI;
lv_obj_t *ui_pnlPower;
lv_obj_t *ui_pnlInterface;
lv_obj_t *ui_pnlAdvanced;
lv_obj_t *ui_lblHidden;
lv_obj_t *ui_lblBGM;
lv_obj_t *ui_lblSound;
lv_obj_t *ui_lblStartup;
lv_obj_t *ui_lblColour;
lv_obj_t *ui_lblBrightness;
lv_obj_t *ui_lblHDMI;
lv_obj_t *ui_lblPower;
lv_obj_t *ui_lblInterface;
lv_obj_t *ui_lblAdvanced;
lv_obj_t *ui_icoHidden;
lv_obj_t *ui_icoBGM;
lv_obj_t *ui_icoSound;
lv_obj_t *ui_icoStartup;
lv_obj_t *ui_icoColour;
lv_obj_t *ui_icoBrightness;
lv_obj_t *ui_icoHDMI;
lv_obj_t *ui_icoPower;
lv_obj_t *ui_icoInterface;
lv_obj_t *ui_icoAdvanced;
lv_obj_t *ui_droHidden;
lv_obj_t *ui_droBGM;
lv_obj_t *ui_droSound;
lv_obj_t *ui_droStartup;
lv_obj_t *ui_droColour;
lv_obj_t *ui_droBrightness;
lv_obj_t *ui_droHDMI;
lv_obj_t *ui_droPower;
lv_obj_t *ui_droInterface;
lv_obj_t *ui_droAdvanced;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_screen_init(ui_pnlContent);
}
