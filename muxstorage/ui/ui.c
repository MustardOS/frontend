#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

lv_obj_t *ui_pnlBIOS;
lv_obj_t *ui_pnlConfig;
lv_obj_t *ui_pnlCatalogue;
lv_obj_t *ui_pnlConman;
lv_obj_t *ui_pnlMusic;
lv_obj_t *ui_pnlSave;
lv_obj_t *ui_pnlScreenshot;
lv_obj_t *ui_pnlTheme;
lv_obj_t *ui_pnlLanguage;
lv_obj_t *ui_pnlNetwork;
lv_obj_t *ui_lblBIOS;
lv_obj_t *ui_lblConfig;
lv_obj_t *ui_lblCatalogue;
lv_obj_t *ui_lblConman;
lv_obj_t *ui_lblMusic;
lv_obj_t *ui_lblSave;
lv_obj_t *ui_lblScreenshot;
lv_obj_t *ui_lblTheme;
lv_obj_t *ui_lblLanguage;
lv_obj_t *ui_lblNetwork;
lv_obj_t *ui_icoBIOS;
lv_obj_t *ui_icoConfig;
lv_obj_t *ui_icoCatalogue;
lv_obj_t *ui_icoConman;
lv_obj_t *ui_icoMusic;
lv_obj_t *ui_icoSave;
lv_obj_t *ui_icoScreenshot;
lv_obj_t *ui_icoTheme;
lv_obj_t *ui_icoLanguage;
lv_obj_t *ui_icoNetwork;
lv_obj_t *ui_droBIOS;
lv_obj_t *ui_droConfig;
lv_obj_t *ui_droCatalogue;
lv_obj_t *ui_droConman;
lv_obj_t *ui_droMusic;
lv_obj_t *ui_droSave;
lv_obj_t *ui_droScreenshot;
lv_obj_t *ui_droTheme;
lv_obj_t *ui_droLanguage;
lv_obj_t *ui_droNetwork;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_screen_init(ui_pnlContent);
}
