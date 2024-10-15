#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

lv_obj_t *ui_pnlBIOS;
lv_obj_t *ui_pnlCatalogue;
lv_obj_t *ui_pnlName;
lv_obj_t *ui_pnlRetroArch;
lv_obj_t *ui_pnlConfig;
lv_obj_t *ui_pnlCore;
lv_obj_t *ui_pnlFavourite;
lv_obj_t *ui_pnlHistory;
lv_obj_t *ui_pnlMusic;
lv_obj_t *ui_pnlSave;
lv_obj_t *ui_pnlScreenshot;
lv_obj_t *ui_pnlTheme;
lv_obj_t *ui_pnlLanguage;
lv_obj_t *ui_pnlNetwork;
lv_obj_t *ui_pnlSyncthing;

lv_obj_t *ui_lblBIOS;
lv_obj_t *ui_lblCatalogue;
lv_obj_t *ui_lblName;
lv_obj_t *ui_lblRetroArch;
lv_obj_t *ui_lblConfig;
lv_obj_t *ui_lblCore;
lv_obj_t *ui_lblFavourite;
lv_obj_t *ui_lblHistory;
lv_obj_t *ui_lblMusic;
lv_obj_t *ui_lblSave;
lv_obj_t *ui_lblScreenshot;
lv_obj_t *ui_lblTheme;
lv_obj_t *ui_lblLanguage;
lv_obj_t *ui_lblNetwork;
lv_obj_t *ui_lblSyncthing;

lv_obj_t *ui_icoBIOS;
lv_obj_t *ui_icoCatalogue;
lv_obj_t *ui_icoName;
lv_obj_t *ui_icoRetroArch;
lv_obj_t *ui_icoConfig;
lv_obj_t *ui_icoCore;
lv_obj_t *ui_icoFavourite;
lv_obj_t *ui_icoHistory;
lv_obj_t *ui_icoMusic;
lv_obj_t *ui_icoSave;
lv_obj_t *ui_icoScreenshot;
lv_obj_t *ui_icoTheme;
lv_obj_t *ui_icoLanguage;
lv_obj_t *ui_icoNetwork;
lv_obj_t *ui_icoSyncthing;

lv_obj_t *ui_lblBIOSValue;
lv_obj_t *ui_lblCatalogueValue;
lv_obj_t *ui_lblNameValue;
lv_obj_t *ui_lblRetroArchValue;
lv_obj_t *ui_lblConfigValue;
lv_obj_t *ui_lblCoreValue;
lv_obj_t *ui_lblFavouriteValue;
lv_obj_t *ui_lblHistoryValue;
lv_obj_t *ui_lblMusicValue;
lv_obj_t *ui_lblSaveValue;
lv_obj_t *ui_lblScreenshotValue;
lv_obj_t *ui_lblThemeValue;
lv_obj_t *ui_lblLanguageValue;
lv_obj_t *ui_lblNetworkValue;
lv_obj_t *ui_lblSyncthingValue;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_screen_init(ui_pnlContent);
}
