#include "ui_muxvisual.h"

lv_obj_t *ui_pnlBattery_visual;
lv_obj_t *ui_pnlClock_visual;
lv_obj_t *ui_pnlNetwork_visual;
lv_obj_t *ui_pnlName_visual;
lv_obj_t *ui_pnlDash_visual;
lv_obj_t *ui_pnlFriendlyFolder_visual;
lv_obj_t *ui_pnlTheTitleFormat_visual;
lv_obj_t *ui_pnlTitleIncludeRootDrive_visual;
lv_obj_t *ui_pnlFolderItemCount_visual;
lv_obj_t *ui_pnlDisplayEmptyFolder_visual;
lv_obj_t *ui_pnlMenuCounterFolder_visual;
lv_obj_t *ui_pnlMenuCounterFile_visual;
lv_obj_t *ui_pnlHidden_visual;
lv_obj_t *ui_pnlOverlayImage_visual;
lv_obj_t *ui_pnlOverlayTransparency_visual;

lv_obj_t *ui_lblBattery_visual;
lv_obj_t *ui_lblClock_visual;
lv_obj_t *ui_lblNetwork_visual;
lv_obj_t *ui_lblName_visual;
lv_obj_t *ui_lblDash_visual;
lv_obj_t *ui_lblFriendlyFolder_visual;
lv_obj_t *ui_lblTheTitleFormat_visual;
lv_obj_t *ui_lblTitleIncludeRootDrive_visual;
lv_obj_t *ui_lblFolderItemCount_visual;
lv_obj_t *ui_lblDisplayEmptyFolder_visual;
lv_obj_t *ui_lblMenuCounterFolder_visual;
lv_obj_t *ui_lblMenuCounterFile_visual;
lv_obj_t *ui_lblHidden_visual;
lv_obj_t *ui_lblOverlayImage_visual;
lv_obj_t *ui_lblOverlayTransparency_visual;

lv_obj_t *ui_icoBattery_visual;
lv_obj_t *ui_icoClock_visual;
lv_obj_t *ui_icoNetwork_visual;
lv_obj_t *ui_icoName_visual;
lv_obj_t *ui_icoDash_visual;
lv_obj_t *ui_icoFriendlyFolder_visual;
lv_obj_t *ui_icoTheTitleFormat_visual;
lv_obj_t *ui_icoTitleIncludeRootDrive_visual;
lv_obj_t *ui_icoFolderItemCount_visual;
lv_obj_t *ui_icoDisplayEmptyFolder_visual;
lv_obj_t *ui_icoMenuCounterFolder_visual;
lv_obj_t *ui_icoMenuCounterFile_visual;
lv_obj_t *ui_icoHidden_visual;
lv_obj_t *ui_icoOverlayImage_visual;
lv_obj_t *ui_icoOverlayTransparency_visual;

lv_obj_t *ui_droBattery_visual;
lv_obj_t *ui_droClock_visual;
lv_obj_t *ui_droNetwork_visual;
lv_obj_t *ui_droName_visual;
lv_obj_t *ui_droDash_visual;
lv_obj_t *ui_droFriendlyFolder_visual;
lv_obj_t *ui_droTheTitleFormat_visual;
lv_obj_t *ui_droTitleIncludeRootDrive_visual;
lv_obj_t *ui_droFolderItemCount_visual;
lv_obj_t *ui_droDisplayEmptyFolder_visual;
lv_obj_t *ui_droMenuCounterFolder_visual;
lv_obj_t *ui_droMenuCounterFile_visual;
lv_obj_t *ui_droHidden_visual;
lv_obj_t *ui_droOverlayImage_visual;
lv_obj_t *ui_droOverlayTransparency_visual;

void init_muxvisual(lv_obj_t *ui_pnlContent) {
#define CREATE_ELEMENT_ITEM(module, name) do {                                                       \
        ui_pnl##name##_##module = lv_obj_create(ui_pnlContent);                                      \
        ui_lbl##name##_##module = lv_label_create(ui_pnl##name##_##module);                          \
        lv_label_set_text(ui_lbl##name##_##module, "");                                              \
        ui_ico##name##_##module = lv_img_create(ui_pnl##name##_##module);                            \
        ui_dro##name##_##module = lv_dropdown_create(ui_pnl##name##_##module);                       \
        lv_dropdown_clear_options(ui_dro##name##_##module);                                          \
        lv_obj_set_style_text_opa(ui_dro##name##_##module, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT); \
    } while (0)

    CREATE_ELEMENT_ITEM(visual, Battery);
    CREATE_ELEMENT_ITEM(visual, Clock);
    CREATE_ELEMENT_ITEM(visual, Network);
    CREATE_ELEMENT_ITEM(visual, Name);
    CREATE_ELEMENT_ITEM(visual, Dash);
    CREATE_ELEMENT_ITEM(visual, FriendlyFolder);
    CREATE_ELEMENT_ITEM(visual, TheTitleFormat);
    CREATE_ELEMENT_ITEM(visual, TitleIncludeRootDrive);
    CREATE_ELEMENT_ITEM(visual, FolderItemCount);
    CREATE_ELEMENT_ITEM(visual, DisplayEmptyFolder);
    CREATE_ELEMENT_ITEM(visual, MenuCounterFolder);
    CREATE_ELEMENT_ITEM(visual, MenuCounterFile);
    CREATE_ELEMENT_ITEM(visual, Hidden);
    CREATE_ELEMENT_ITEM(visual, OverlayImage);
    CREATE_ELEMENT_ITEM(visual, OverlayTransparency);

#undef CREATE_ELEMENT_ITEM
}
