#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent) {
    ui_pnlBIOS = lv_obj_create(ui_pnlContent);
    ui_pnlConfig = lv_obj_create(ui_pnlContent);
    ui_pnlCatalogue = lv_obj_create(ui_pnlContent);
    ui_pnlConman = lv_obj_create(ui_pnlContent);
    ui_pnlMusic = lv_obj_create(ui_pnlContent);
    ui_pnlSave = lv_obj_create(ui_pnlContent);
    ui_pnlScreenshot = lv_obj_create(ui_pnlContent);
    ui_pnlTheme = lv_obj_create(ui_pnlContent);
    ui_pnlLanguage = lv_obj_create(ui_pnlContent);
    ui_pnlNetwork = lv_obj_create(ui_pnlContent);

    ui_lblBIOS = lv_label_create(ui_pnlBIOS);
    ui_lblConfig = lv_label_create(ui_pnlConfig);
    ui_lblCatalogue = lv_label_create(ui_pnlCatalogue);
    ui_lblConman = lv_label_create(ui_pnlConman);
    ui_lblMusic = lv_label_create(ui_pnlMusic);
    ui_lblSave = lv_label_create(ui_pnlSave);
    ui_lblScreenshot = lv_label_create(ui_pnlScreenshot);
    ui_lblTheme = lv_label_create(ui_pnlTheme);
    ui_lblLanguage = lv_label_create(ui_pnlLanguage);
    ui_lblNetwork = lv_label_create(ui_pnlNetwork);

    ui_icoBIOS = lv_img_create(ui_pnlBIOS);
    ui_icoConfig = lv_img_create(ui_pnlConfig);
    ui_icoCatalogue = lv_img_create(ui_pnlCatalogue);
    ui_icoConman = lv_img_create(ui_pnlConman);
    ui_icoMusic = lv_img_create(ui_pnlMusic);
    ui_icoSave = lv_img_create(ui_pnlSave);
    ui_icoScreenshot = lv_img_create(ui_pnlScreenshot);
    ui_icoTheme = lv_img_create(ui_pnlTheme);
    ui_icoLanguage = lv_img_create(ui_pnlLanguage);
    ui_icoNetwork = lv_img_create(ui_pnlNetwork);

    ui_droBIOS = lv_dropdown_create(ui_pnlBIOS);
    ui_droConfig = lv_dropdown_create(ui_pnlConfig);
    ui_droCatalogue = lv_dropdown_create(ui_pnlCatalogue);
    ui_droConman = lv_dropdown_create(ui_pnlConman);
    ui_droMusic = lv_dropdown_create(ui_pnlMusic);
    ui_droSave = lv_dropdown_create(ui_pnlSave);
    ui_droScreenshot = lv_dropdown_create(ui_pnlScreenshot);
    ui_droTheme = lv_dropdown_create(ui_pnlTheme);
    ui_droLanguage = lv_dropdown_create(ui_pnlLanguage);
    ui_droNetwork = lv_dropdown_create(ui_pnlNetwork);
}
