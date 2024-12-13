#include "ui_muxtweakgen.h"

lv_obj_t *ui_pnlHidden;
lv_obj_t *ui_pnlBGM;
lv_obj_t *ui_pnlSound;
lv_obj_t *ui_pnlAudioSink;
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
lv_obj_t *ui_lblAudioSink;
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
lv_obj_t *ui_icoAudioSink;
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
lv_obj_t *ui_droAudioSink;
lv_obj_t *ui_droStartup;
lv_obj_t *ui_droColour;
lv_obj_t *ui_droBrightness;
lv_obj_t *ui_droHDMI;
lv_obj_t *ui_droPower;
lv_obj_t *ui_droInterface;
lv_obj_t *ui_droAdvanced;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_pnlHidden = lv_obj_create(ui_pnlContent);
    ui_pnlBGM = lv_obj_create(ui_pnlContent);
    ui_pnlSound = lv_obj_create(ui_pnlContent);
    ui_pnlAudioSink = lv_obj_create(ui_pnlContent);
    ui_pnlStartup = lv_obj_create(ui_pnlContent);
    ui_pnlColour = lv_obj_create(ui_pnlContent);
    ui_pnlBrightness = lv_obj_create(ui_pnlContent);
    ui_pnlHDMI = lv_obj_create(ui_pnlContent);
    ui_pnlPower = lv_obj_create(ui_pnlContent);
    ui_pnlInterface = lv_obj_create(ui_pnlContent);
    ui_pnlAdvanced = lv_obj_create(ui_pnlContent);

    ui_lblHidden = lv_label_create(ui_pnlHidden);
    ui_lblBGM = lv_label_create(ui_pnlBGM);
    ui_lblSound = lv_label_create(ui_pnlSound);
    ui_lblAudioSink = lv_label_create(ui_pnlAudioSink);
    ui_lblStartup = lv_label_create(ui_pnlStartup);
    ui_lblColour = lv_label_create(ui_pnlColour);
    ui_lblBrightness = lv_label_create(ui_pnlBrightness);
    ui_lblHDMI = lv_label_create(ui_pnlHDMI);
    ui_lblPower = lv_label_create(ui_pnlPower);
    ui_lblInterface = lv_label_create(ui_pnlInterface);
    ui_lblAdvanced = lv_label_create(ui_pnlAdvanced);

    ui_icoHidden = lv_img_create(ui_pnlHidden);
    ui_icoBGM = lv_img_create(ui_pnlBGM);
    ui_icoSound = lv_img_create(ui_pnlSound);
    ui_icoAudioSink = lv_img_create(ui_pnlAudioSink);
    ui_icoStartup = lv_img_create(ui_pnlStartup);
    ui_icoColour = lv_img_create(ui_pnlColour);
    ui_icoBrightness = lv_img_create(ui_pnlBrightness);
    ui_icoHDMI = lv_img_create(ui_pnlHDMI);
    ui_icoPower = lv_img_create(ui_pnlPower);
    ui_icoInterface = lv_img_create(ui_pnlInterface);
    ui_icoAdvanced = lv_img_create(ui_pnlAdvanced);

    ui_droHidden = lv_dropdown_create(ui_pnlHidden);
    ui_droBGM = lv_dropdown_create(ui_pnlBGM);
    ui_droSound = lv_dropdown_create(ui_pnlSound);
    ui_droAudioSink = lv_dropdown_create(ui_pnlAudioSink);
    ui_droStartup = lv_dropdown_create(ui_pnlStartup);
    ui_droColour = lv_dropdown_create(ui_pnlColour);
    ui_droBrightness = lv_dropdown_create(ui_pnlBrightness);
    ui_droHDMI = lv_dropdown_create(ui_pnlHDMI);
    ui_droPower = lv_dropdown_create(ui_pnlPower);
    ui_droInterface = lv_dropdown_create(ui_pnlInterface);
    ui_droAdvanced = lv_dropdown_create(ui_pnlAdvanced);
}
