#include "ui_muxvisual.h"

lv_obj_t *ui_pnlBattery;
lv_obj_t *ui_pnlNetwork;
lv_obj_t *ui_pnlClock;
lv_obj_t *ui_pnlName;
lv_obj_t *ui_pnlDash;
lv_obj_t *ui_pnlFriendlyFolder;
lv_obj_t *ui_pnlTheTitleFormat;
lv_obj_t *ui_pnlTitleIncludeRootDrive;
lv_obj_t *ui_pnlFolderItemCount;
lv_obj_t *ui_pnlDisplayEmptyFolder;
lv_obj_t *ui_pnlMenuCounterFolder;
lv_obj_t *ui_pnlMenuCounterFile;
lv_obj_t *ui_pnlHidden;

lv_obj_t *ui_lblBattery;
lv_obj_t *ui_lblNetwork;
lv_obj_t *ui_lblClock;
lv_obj_t *ui_lblName;
lv_obj_t *ui_lblDash;
lv_obj_t *ui_lblFriendlyFolder;
lv_obj_t *ui_lblTheTitleFormat;
lv_obj_t *ui_lblTitleIncludeRootDrive;
lv_obj_t *ui_lblFolderItemCount;
lv_obj_t *ui_lblDisplayEmptyFolder;
lv_obj_t *ui_lblMenuCounterFolder;
lv_obj_t *ui_lblMenuCounterFile;
lv_obj_t *ui_lblHidden;

lv_obj_t *ui_icoBattery;
lv_obj_t *ui_icoNetwork;
lv_obj_t *ui_icoClock;
lv_obj_t *ui_icoName;
lv_obj_t *ui_icoDash;
lv_obj_t *ui_icoFriendlyFolder;
lv_obj_t *ui_icoTheTitleFormat;
lv_obj_t *ui_icoTitleIncludeRootDrive;
lv_obj_t *ui_icoFolderItemCount;
lv_obj_t *ui_icoDisplayEmptyFolder;
lv_obj_t *ui_icoMenuCounterFolder;
lv_obj_t *ui_icoMenuCounterFile;
lv_obj_t *ui_icoHidden;

lv_obj_t *ui_droBattery;
lv_obj_t *ui_droNetwork;
lv_obj_t *ui_droClock;
lv_obj_t *ui_droName;
lv_obj_t *ui_droDash;
lv_obj_t *ui_droFriendlyFolder;
lv_obj_t *ui_droTheTitleFormat;
lv_obj_t *ui_droTitleIncludeRootDrive;
lv_obj_t *ui_droFolderItemCount;
lv_obj_t *ui_droDisplayEmptyFolder;
lv_obj_t *ui_droMenuCounterFolder;
lv_obj_t *ui_droMenuCounterFile;
lv_obj_t *ui_droHidden;

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlBattery = lv_obj_create(ui_pnlContent);
    ui_pnlClock = lv_obj_create(ui_pnlContent);
    ui_pnlNetwork = lv_obj_create(ui_pnlContent);
    ui_pnlDash = lv_obj_create(ui_pnlContent);
    ui_pnlName = lv_obj_create(ui_pnlContent);
    ui_pnlDisplayEmptyFolder = lv_obj_create(ui_pnlContent);
    ui_pnlTheTitleFormat = lv_obj_create(ui_pnlContent);
    ui_pnlFolderItemCount = lv_obj_create(ui_pnlContent);
    ui_pnlFriendlyFolder = lv_obj_create(ui_pnlContent);
    ui_pnlMenuCounterFile = lv_obj_create(ui_pnlContent);
    ui_pnlMenuCounterFolder = lv_obj_create(ui_pnlContent);
    ui_pnlHidden = lv_obj_create(ui_pnlContent);
    ui_pnlTitleIncludeRootDrive = lv_obj_create(ui_pnlContent);

    ui_lblBattery = lv_label_create(ui_pnlBattery);
    ui_lblNetwork = lv_label_create(ui_pnlNetwork);
    ui_lblClock = lv_label_create(ui_pnlClock);
    ui_lblName = lv_label_create(ui_pnlName);
    ui_lblDash = lv_label_create(ui_pnlDash);
    ui_lblFriendlyFolder = lv_label_create(ui_pnlFriendlyFolder);
    ui_lblTheTitleFormat = lv_label_create(ui_pnlTheTitleFormat);
    ui_lblTitleIncludeRootDrive = lv_label_create(ui_pnlTitleIncludeRootDrive);
    ui_lblFolderItemCount = lv_label_create(ui_pnlFolderItemCount);
    ui_lblDisplayEmptyFolder = lv_label_create(ui_pnlDisplayEmptyFolder);
    ui_lblMenuCounterFolder = lv_label_create(ui_pnlMenuCounterFolder);
    ui_lblMenuCounterFile = lv_label_create(ui_pnlMenuCounterFile);
    ui_lblHidden = lv_label_create(ui_pnlHidden);

    ui_icoBattery = lv_img_create(ui_pnlBattery);
    ui_icoNetwork = lv_img_create(ui_pnlNetwork);
    ui_icoClock = lv_img_create(ui_pnlClock);
    ui_icoName = lv_img_create(ui_pnlName);
    ui_icoDash = lv_img_create(ui_pnlDash);
    ui_icoFriendlyFolder = lv_img_create(ui_pnlFriendlyFolder);
    ui_icoTheTitleFormat = lv_img_create(ui_pnlTheTitleFormat);
    ui_icoTitleIncludeRootDrive = lv_img_create(ui_pnlTitleIncludeRootDrive);
    ui_icoFolderItemCount = lv_img_create(ui_pnlFolderItemCount);
    ui_icoDisplayEmptyFolder = lv_img_create(ui_pnlDisplayEmptyFolder);
    ui_icoMenuCounterFolder = lv_img_create(ui_pnlMenuCounterFolder);
    ui_icoMenuCounterFile = lv_img_create(ui_pnlMenuCounterFile);
    ui_icoHidden = lv_img_create(ui_pnlHidden);

    ui_droBattery = lv_dropdown_create(ui_pnlBattery);
    ui_droNetwork = lv_dropdown_create(ui_pnlNetwork);
    ui_droClock = lv_dropdown_create(ui_pnlClock);
    ui_droName = lv_dropdown_create(ui_pnlName);
    ui_droDash = lv_dropdown_create(ui_pnlDash);
    ui_droFriendlyFolder = lv_dropdown_create(ui_pnlFriendlyFolder);
    ui_droTheTitleFormat = lv_dropdown_create(ui_pnlTheTitleFormat);
    ui_droTitleIncludeRootDrive = lv_dropdown_create(ui_pnlTitleIncludeRootDrive);
    ui_droFolderItemCount = lv_dropdown_create(ui_pnlFolderItemCount);
    ui_droDisplayEmptyFolder = lv_dropdown_create(ui_pnlDisplayEmptyFolder);
    ui_droMenuCounterFolder = lv_dropdown_create(ui_pnlMenuCounterFolder);
    ui_droMenuCounterFile = lv_dropdown_create(ui_pnlMenuCounterFile);
    ui_droHidden = lv_dropdown_create(ui_pnlHidden);
}
