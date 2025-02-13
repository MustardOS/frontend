#include "ui_muxcustom.h"

lv_obj_t *ui_pnlBackgroundAnimation;
lv_obj_t *ui_pnlBlackFade;
lv_obj_t *ui_pnlBoxArt;
lv_obj_t *ui_pnlBoxArtAlign;
lv_obj_t *ui_pnlLaunchSplash;
lv_obj_t *ui_pnlFont;
lv_obj_t *ui_pnlTheme;
lv_obj_t *ui_pnlThemeAlternate;
lv_obj_t *ui_pnlCatalogue;
lv_obj_t *ui_pnlConfig;

lv_obj_t *ui_lblBackgroundAnimation;
lv_obj_t *ui_lblBlackFade;
lv_obj_t *ui_lblBoxArt;
lv_obj_t *ui_lblBoxArtAlign;
lv_obj_t *ui_lblLaunchSplash;
lv_obj_t *ui_lblFont;
lv_obj_t *ui_lblTheme;
lv_obj_t *ui_lblThemeAlternate;
lv_obj_t *ui_lblCatalogue;
lv_obj_t *ui_lblConfig;

lv_obj_t *ui_icoBackgroundAnimation;
lv_obj_t *ui_icoBlackFade;
lv_obj_t *ui_icoBoxArt;
lv_obj_t *ui_icoBoxArtAlign;
lv_obj_t *ui_icoLaunchSplash;
lv_obj_t *ui_icoFont;
lv_obj_t *ui_icoTheme;
lv_obj_t *ui_icoThemeAlternate;
lv_obj_t *ui_icoCatalogue;
lv_obj_t *ui_icoConfig;

lv_obj_t *ui_droBackgroundAnimation;
lv_obj_t *ui_droBlackFade;
lv_obj_t *ui_droBoxArt;
lv_obj_t *ui_droBoxArtAlign;
lv_obj_t *ui_droLaunchSplash;
lv_obj_t *ui_droFont;
lv_obj_t *ui_droTheme;
lv_obj_t *ui_droThemeAlternate;
lv_obj_t *ui_droCatalogue;
lv_obj_t *ui_droConfig;

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlBackgroundAnimation = lv_obj_create(ui_pnlContent);
    ui_pnlBlackFade = lv_obj_create(ui_pnlContent);
    ui_pnlCatalogue = lv_obj_create(ui_pnlContent);
    ui_pnlBoxArt = lv_obj_create(ui_pnlContent);
    ui_pnlBoxArtAlign = lv_obj_create(ui_pnlContent);
    ui_pnlLaunchSplash = lv_obj_create(ui_pnlContent);
    ui_pnlFont = lv_obj_create(ui_pnlContent);
    ui_pnlTheme = lv_obj_create(ui_pnlContent);
    ui_pnlThemeAlternate = lv_obj_create(ui_pnlContent);
    ui_pnlConfig = lv_obj_create(ui_pnlContent);

    ui_lblBackgroundAnimation = lv_label_create(ui_pnlBackgroundAnimation);
    ui_lblBlackFade = lv_label_create(ui_pnlBlackFade);
    ui_lblBoxArt = lv_label_create(ui_pnlBoxArt);
    ui_lblBoxArtAlign = lv_label_create(ui_pnlBoxArtAlign);
    ui_lblLaunchSplash = lv_label_create(ui_pnlLaunchSplash);
    ui_lblFont = lv_label_create(ui_pnlFont);
    ui_lblTheme = lv_label_create(ui_pnlTheme);
    ui_lblThemeAlternate = lv_label_create(ui_pnlThemeAlternate);
    ui_lblCatalogue = lv_label_create(ui_pnlCatalogue);
    ui_lblConfig = lv_label_create(ui_pnlConfig);

    ui_icoBackgroundAnimation = lv_img_create(ui_pnlBackgroundAnimation);
    ui_icoBlackFade = lv_img_create(ui_pnlBlackFade);
    ui_icoBoxArt = lv_img_create(ui_pnlBoxArt);
    ui_icoBoxArtAlign = lv_img_create(ui_pnlBoxArtAlign);
    ui_icoLaunchSplash = lv_img_create(ui_pnlLaunchSplash);
    ui_icoFont = lv_img_create(ui_pnlFont);
    ui_icoTheme = lv_img_create(ui_pnlTheme);
    ui_icoThemeAlternate = lv_img_create(ui_pnlThemeAlternate);
    ui_icoCatalogue = lv_img_create(ui_pnlCatalogue);
    ui_icoConfig = lv_img_create(ui_pnlConfig);

    ui_droBackgroundAnimation = lv_dropdown_create(ui_pnlBackgroundAnimation);
    ui_droBlackFade = lv_dropdown_create(ui_pnlBlackFade);
    ui_droBoxArt = lv_dropdown_create(ui_pnlBoxArt);
    ui_droBoxArtAlign = lv_dropdown_create(ui_pnlBoxArtAlign);
    ui_droLaunchSplash = lv_dropdown_create(ui_pnlLaunchSplash);
    ui_droFont = lv_dropdown_create(ui_pnlFont);
    ui_droTheme = lv_dropdown_create(ui_pnlTheme);
    ui_droThemeAlternate = lv_dropdown_create(ui_pnlThemeAlternate);
    ui_droCatalogue = lv_dropdown_create(ui_pnlCatalogue);
    ui_droConfig = lv_dropdown_create(ui_pnlConfig);
}
