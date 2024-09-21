#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent) {
    ui_pnlBattery = lv_obj_create(ui_pnlContent);
    ui_pnlNetwork = lv_obj_create(ui_pnlContent);
    ui_pnlBluetooth = lv_obj_create(ui_pnlContent);
    lv_obj_add_flag(ui_pnlBluetooth, LV_OBJ_FLAG_HIDDEN);
    ui_pnlClock = lv_obj_create(ui_pnlContent);
    ui_pnlBoxArt = lv_obj_create(ui_pnlContent);
    ui_pnlName = lv_obj_create(ui_pnlContent);
    ui_pnlDash = lv_obj_create(ui_pnlContent);
    ui_pnlFriendlyFolder = lv_obj_create(ui_pnlContent);
    ui_pnlTheTitleFormat = lv_obj_create(ui_pnlContent);
    ui_pnlFolderItemCount = lv_obj_create(ui_pnlContent);
    ui_pnlMenuCounterFolder = lv_obj_create(ui_pnlContent);
    ui_pnlMenuCounterFile = lv_obj_create(ui_pnlContent);
    ui_pnlBackgroundAnimation = lv_obj_create(ui_pnlContent);

    ui_lblBattery = lv_label_create(ui_pnlBattery);
    ui_lblNetwork = lv_label_create(ui_pnlNetwork);
    ui_lblBluetooth = lv_label_create(ui_pnlBluetooth);
    ui_lblClock = lv_label_create(ui_pnlClock);
    ui_lblBoxArt = lv_label_create(ui_pnlBoxArt);
    ui_lblName = lv_label_create(ui_pnlName);
    ui_lblDash = lv_label_create(ui_pnlDash);
    ui_lblFriendlyFolder = lv_label_create(ui_pnlFriendlyFolder);
    ui_lblTheTitleFormat = lv_label_create(ui_pnlTheTitleFormat);
    ui_lblFolderItemCount = lv_label_create(ui_pnlFolderItemCount);
    ui_lblMenuCounterFolder = lv_label_create(ui_pnlMenuCounterFolder);
    ui_lblMenuCounterFile = lv_label_create(ui_pnlMenuCounterFile);
    ui_lblBackgroundAnimation = lv_label_create(ui_pnlBackgroundAnimation);

    ui_icoBattery = lv_img_create(ui_pnlBattery);
    ui_icoNetwork = lv_img_create(ui_pnlNetwork);
    ui_icoBluetooth = lv_img_create(ui_pnlBluetooth);
    ui_icoClock = lv_img_create(ui_pnlClock);
    ui_icoBoxArt = lv_img_create(ui_pnlBoxArt);
    ui_icoName = lv_img_create(ui_pnlName);
    ui_icoDash = lv_img_create(ui_pnlDash);
    ui_icoFriendlyFolder = lv_img_create(ui_pnlFriendlyFolder);
    ui_icoTheTitleFormat = lv_img_create(ui_pnlTheTitleFormat);
    ui_icoFolderItemCount = lv_img_create(ui_pnlFolderItemCount);
    ui_icoMenuCounterFolder = lv_img_create(ui_pnlMenuCounterFolder);
    ui_icoMenuCounterFile = lv_img_create(ui_pnlMenuCounterFile);
    ui_icoBackgroundAnimation = lv_img_create(ui_pnlBackgroundAnimation);

    ui_droBattery = lv_dropdown_create(ui_pnlBattery);
    ui_droNetwork = lv_dropdown_create(ui_pnlNetwork);
    ui_droBluetooth = lv_dropdown_create(ui_pnlBluetooth);
    ui_droClock = lv_dropdown_create(ui_pnlClock);
    ui_droBoxArt = lv_dropdown_create(ui_pnlBoxArt);
    ui_droName = lv_dropdown_create(ui_pnlName);
    ui_droDash = lv_dropdown_create(ui_pnlDash);
    ui_droFriendlyFolder = lv_dropdown_create(ui_pnlFriendlyFolder);
    ui_droTheTitleFormat = lv_dropdown_create(ui_pnlTheTitleFormat);
    ui_droFolderItemCount = lv_dropdown_create(ui_pnlFolderItemCount);
    ui_droMenuCounterFolder = lv_dropdown_create(ui_pnlMenuCounterFolder);
    ui_droMenuCounterFile = lv_dropdown_create(ui_pnlMenuCounterFile);
    ui_droBackgroundAnimation = lv_dropdown_create(ui_pnlBackgroundAnimation);
}
