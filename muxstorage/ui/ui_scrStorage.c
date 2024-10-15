#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent) {
    ui_pnlBIOS = lv_obj_create(ui_pnlContent);
    ui_pnlCatalogue = lv_obj_create(ui_pnlContent);
    ui_pnlName = lv_obj_create(ui_pnlContent);
    ui_pnlRetroArch = lv_obj_create(ui_pnlContent);
    ui_pnlConfig = lv_obj_create(ui_pnlContent);
    ui_pnlCore = lv_obj_create(ui_pnlContent);
    ui_pnlFavourite = lv_obj_create(ui_pnlContent);
    ui_pnlHistory = lv_obj_create(ui_pnlContent);
    ui_pnlMusic = lv_obj_create(ui_pnlContent);
    ui_pnlSave = lv_obj_create(ui_pnlContent);
    ui_pnlScreenshot = lv_obj_create(ui_pnlContent);
    ui_pnlTheme = lv_obj_create(ui_pnlContent);
    ui_pnlLanguage = lv_obj_create(ui_pnlContent);
    ui_pnlNetwork = lv_obj_create(ui_pnlContent);
    ui_pnlSyncthing = lv_obj_create(ui_pnlContent);

    ui_lblBIOS = lv_label_create(ui_pnlBIOS);
    ui_lblCatalogue = lv_label_create(ui_pnlCatalogue);
    ui_lblName = lv_label_create(ui_pnlName);
    ui_lblRetroArch = lv_label_create(ui_pnlRetroArch);
    ui_lblConfig = lv_label_create(ui_pnlConfig);
    ui_lblCore = lv_label_create(ui_pnlCore);
    ui_lblFavourite = lv_label_create(ui_pnlFavourite);
    ui_lblHistory = lv_label_create(ui_pnlHistory);
    ui_lblMusic = lv_label_create(ui_pnlMusic);
    ui_lblSave = lv_label_create(ui_pnlSave);
    ui_lblScreenshot = lv_label_create(ui_pnlScreenshot);
    ui_lblTheme = lv_label_create(ui_pnlTheme);
    ui_lblLanguage = lv_label_create(ui_pnlLanguage);
    ui_lblNetwork = lv_label_create(ui_pnlNetwork);
    ui_lblSyncthing = lv_label_create(ui_pnlSyncthing);

    ui_icoBIOS = lv_img_create(ui_pnlBIOS);
    ui_icoCatalogue = lv_img_create(ui_pnlCatalogue);
    ui_icoName = lv_img_create(ui_pnlName);
    ui_icoRetroArch = lv_img_create(ui_pnlRetroArch);
    ui_icoConfig = lv_img_create(ui_pnlConfig);
    ui_icoCore = lv_img_create(ui_pnlCore);
    ui_icoFavourite = lv_img_create(ui_pnlFavourite);
    ui_icoHistory = lv_img_create(ui_pnlHistory);
    ui_icoMusic = lv_img_create(ui_pnlMusic);
    ui_icoSave = lv_img_create(ui_pnlSave);
    ui_icoScreenshot = lv_img_create(ui_pnlScreenshot);
    ui_icoTheme = lv_img_create(ui_pnlTheme);
    ui_icoLanguage = lv_img_create(ui_pnlLanguage);
    ui_icoNetwork = lv_img_create(ui_pnlNetwork);
    ui_icoSyncthing = lv_img_create(ui_pnlSyncthing);

    ui_lblBIOSValue = lv_label_create(ui_pnlBIOS);
    ui_lblCatalogueValue = lv_label_create(ui_pnlCatalogue);
    ui_lblNameValue = lv_label_create(ui_pnlName);
    ui_lblRetroArchValue = lv_label_create(ui_pnlRetroArch);
    ui_lblConfigValue = lv_label_create(ui_pnlConfig);
    ui_lblCoreValue = lv_label_create(ui_pnlCore);
    ui_lblFavouriteValue = lv_label_create(ui_pnlFavourite);
    ui_lblHistoryValue = lv_label_create(ui_pnlHistory);
    ui_lblMusicValue = lv_label_create(ui_pnlMusic);
    ui_lblSaveValue = lv_label_create(ui_pnlSave);
    ui_lblScreenshotValue = lv_label_create(ui_pnlScreenshot);
    ui_lblThemeValue = lv_label_create(ui_pnlTheme);
    ui_lblLanguageValue = lv_label_create(ui_pnlLanguage);
    ui_lblNetworkValue = lv_label_create(ui_pnlNetwork);
    ui_lblSyncthingValue = lv_label_create(ui_pnlSyncthing);
}
