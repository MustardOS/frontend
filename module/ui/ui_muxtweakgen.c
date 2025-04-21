#include "ui_muxtweakgen.h"

lv_obj_t *ui_pnlStartup;
lv_obj_t *ui_pnlColour;
lv_obj_t *ui_pnlRTC;
lv_obj_t *ui_pnlBrightness;
lv_obj_t *ui_pnlVolume;
lv_obj_t *ui_pnlHDMI;
lv_obj_t *ui_pnlPower;
lv_obj_t *ui_pnlInterface;
lv_obj_t *ui_pnlAdvanced;

lv_obj_t *ui_lblStartup;
lv_obj_t *ui_lblColour;
lv_obj_t *ui_lblRTC;
lv_obj_t *ui_lblBrightness;
lv_obj_t *ui_lblVolume;
lv_obj_t *ui_lblHDMI;
lv_obj_t *ui_lblPower;
lv_obj_t *ui_lblInterface;
lv_obj_t *ui_lblAdvanced;

lv_obj_t *ui_icoStartup;
lv_obj_t *ui_icoColour;
lv_obj_t *ui_icoRTC;
lv_obj_t *ui_icoBrightness;
lv_obj_t *ui_icoVolume;
lv_obj_t *ui_icoHDMI;
lv_obj_t *ui_icoPower;
lv_obj_t *ui_icoInterface;
lv_obj_t *ui_icoAdvanced;

lv_obj_t *ui_droStartup;
lv_obj_t *ui_droColour;
lv_obj_t *ui_droRTC;
lv_obj_t *ui_droBrightness;
lv_obj_t *ui_droVolume;
lv_obj_t *ui_droHDMI;
lv_obj_t *ui_droPower;
lv_obj_t *ui_droInterface;
lv_obj_t *ui_droAdvanced;

void init_muxtweakgen(lv_obj_t *ui_pnlContent) {
    ui_pnlRTC = lv_obj_create(ui_pnlContent);
    ui_pnlHDMI = lv_obj_create(ui_pnlContent);
    ui_pnlAdvanced = lv_obj_create(ui_pnlContent);
    ui_pnlBrightness = lv_obj_create(ui_pnlContent);
    ui_pnlVolume = lv_obj_create(ui_pnlContent);
    ui_pnlColour = lv_obj_create(ui_pnlContent);
    ui_pnlStartup = lv_obj_create(ui_pnlContent);

    ui_lblStartup = lv_label_create(ui_pnlStartup);
    lv_label_set_text(ui_lblStartup, "");
    ui_lblColour = lv_label_create(ui_pnlColour);
    lv_label_set_text(ui_lblColour, "");
    ui_lblRTC = lv_label_create(ui_pnlRTC);
    lv_label_set_text(ui_lblRTC, "");
    ui_lblBrightness = lv_label_create(ui_pnlBrightness);
    lv_label_set_text(ui_lblBrightness, "");
    ui_lblVolume = lv_label_create(ui_pnlVolume);
    lv_label_set_text(ui_lblVolume, "");
    ui_lblHDMI = lv_label_create(ui_pnlHDMI);
    lv_label_set_text(ui_lblHDMI, "");
    ui_lblAdvanced = lv_label_create(ui_pnlAdvanced);
    lv_label_set_text(ui_lblAdvanced, "");

    ui_icoStartup = lv_img_create(ui_pnlStartup);
    ui_icoColour = lv_img_create(ui_pnlColour);
    ui_icoRTC = lv_img_create(ui_pnlRTC);
    ui_icoBrightness = lv_img_create(ui_pnlBrightness);
    ui_icoVolume = lv_img_create(ui_pnlVolume);
    ui_icoHDMI = lv_img_create(ui_pnlHDMI);
    ui_icoAdvanced = lv_img_create(ui_pnlAdvanced);

    ui_droStartup = lv_dropdown_create(ui_pnlStartup);
    lv_dropdown_clear_options(ui_droStartup);
    lv_obj_set_style_text_opa(ui_droStartup, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droColour = lv_dropdown_create(ui_pnlColour);
    lv_dropdown_clear_options(ui_droColour);
    lv_obj_set_style_text_opa(ui_droColour, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droRTC = lv_dropdown_create(ui_pnlRTC);
    lv_dropdown_clear_options(ui_droRTC);
    lv_obj_set_style_text_opa(ui_droRTC, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droBrightness = lv_dropdown_create(ui_pnlBrightness);
    lv_dropdown_clear_options(ui_droBrightness);
    lv_obj_set_style_text_opa(ui_droBrightness, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droVolume = lv_dropdown_create(ui_pnlVolume);
    lv_dropdown_clear_options(ui_droVolume);
    lv_obj_set_style_text_opa(ui_droVolume, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droHDMI = lv_dropdown_create(ui_pnlHDMI);
    lv_dropdown_clear_options(ui_droHDMI);
    lv_obj_set_style_text_opa(ui_droHDMI, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droAdvanced = lv_dropdown_create(ui_pnlAdvanced);
    lv_dropdown_clear_options(ui_droAdvanced);
    lv_obj_set_style_text_opa(ui_droAdvanced, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}
