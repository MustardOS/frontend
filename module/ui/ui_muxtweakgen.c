#include "ui_muxtweakgen.h"

lv_obj_t *ui_pnlStartup_tweakgen;
lv_obj_t *ui_pnlColour_tweakgen;
lv_obj_t *ui_pnlRTC_tweakgen;
lv_obj_t *ui_pnlBrightness_tweakgen;
lv_obj_t *ui_pnlVolume_tweakgen;
lv_obj_t *ui_pnlHDMI_tweakgen;
lv_obj_t *ui_pnlPower_tweakgen;
lv_obj_t *ui_pnlInterface_tweakgen;
lv_obj_t *ui_pnlAdvanced_tweakgen;

lv_obj_t *ui_lblStartup_tweakgen;
lv_obj_t *ui_lblColour_tweakgen;
lv_obj_t *ui_lblRTC_tweakgen;
lv_obj_t *ui_lblBrightness_tweakgen;
lv_obj_t *ui_lblVolume_tweakgen;
lv_obj_t *ui_lblHDMI_tweakgen;
lv_obj_t *ui_lblPower_tweakgen;
lv_obj_t *ui_lblInterface_tweakgen;
lv_obj_t *ui_lblAdvanced_tweakgen;

lv_obj_t *ui_icoStartup_tweakgen;
lv_obj_t *ui_icoColour_tweakgen;
lv_obj_t *ui_icoRTC_tweakgen;
lv_obj_t *ui_icoBrightness_tweakgen;
lv_obj_t *ui_icoVolume_tweakgen;
lv_obj_t *ui_icoHDMI_tweakgen;
lv_obj_t *ui_icoPower_tweakgen;
lv_obj_t *ui_icoInterface_tweakgen;
lv_obj_t *ui_icoAdvanced_tweakgen;

lv_obj_t *ui_droStartup_tweakgen;
lv_obj_t *ui_droColour_tweakgen;
lv_obj_t *ui_droRTC_tweakgen;
lv_obj_t *ui_droBrightness_tweakgen;
lv_obj_t *ui_droVolume_tweakgen;
lv_obj_t *ui_droHDMI_tweakgen;
lv_obj_t *ui_droPower_tweakgen;
lv_obj_t *ui_droInterface_tweakgen;
lv_obj_t *ui_droAdvanced_tweakgen;

void init_muxtweakgen(lv_obj_t *ui_pnlContent) {
    ui_pnlRTC_tweakgen = lv_obj_create(ui_pnlContent);
    ui_pnlHDMI_tweakgen = lv_obj_create(ui_pnlContent);
    ui_pnlAdvanced_tweakgen = lv_obj_create(ui_pnlContent);
    ui_pnlBrightness_tweakgen = lv_obj_create(ui_pnlContent);
    ui_pnlVolume_tweakgen = lv_obj_create(ui_pnlContent);
    ui_pnlColour_tweakgen = lv_obj_create(ui_pnlContent);
    ui_pnlStartup_tweakgen = lv_obj_create(ui_pnlContent);

    ui_lblStartup_tweakgen = lv_label_create(ui_pnlStartup_tweakgen);
    lv_label_set_text(ui_lblStartup_tweakgen, "");
    ui_lblColour_tweakgen = lv_label_create(ui_pnlColour_tweakgen);
    lv_label_set_text(ui_lblColour_tweakgen, "");
    ui_lblRTC_tweakgen = lv_label_create(ui_pnlRTC_tweakgen);
    lv_label_set_text(ui_lblRTC_tweakgen, "");
    ui_lblBrightness_tweakgen = lv_label_create(ui_pnlBrightness_tweakgen);
    lv_label_set_text(ui_lblBrightness_tweakgen, "");
    ui_lblVolume_tweakgen = lv_label_create(ui_pnlVolume_tweakgen);
    lv_label_set_text(ui_lblVolume_tweakgen, "");
    ui_lblHDMI_tweakgen = lv_label_create(ui_pnlHDMI_tweakgen);
    lv_label_set_text(ui_lblHDMI_tweakgen, "");
    ui_lblAdvanced_tweakgen = lv_label_create(ui_pnlAdvanced_tweakgen);
    lv_label_set_text(ui_lblAdvanced_tweakgen, "");

    ui_icoStartup_tweakgen = lv_img_create(ui_pnlStartup_tweakgen);
    ui_icoColour_tweakgen = lv_img_create(ui_pnlColour_tweakgen);
    ui_icoRTC_tweakgen = lv_img_create(ui_pnlRTC_tweakgen);
    ui_icoBrightness_tweakgen = lv_img_create(ui_pnlBrightness_tweakgen);
    ui_icoVolume_tweakgen = lv_img_create(ui_pnlVolume_tweakgen);
    ui_icoHDMI_tweakgen = lv_img_create(ui_pnlHDMI_tweakgen);
    ui_icoAdvanced_tweakgen = lv_img_create(ui_pnlAdvanced_tweakgen);

    ui_droStartup_tweakgen = lv_dropdown_create(ui_pnlStartup_tweakgen);
    lv_dropdown_clear_options(ui_droStartup_tweakgen);
    lv_obj_set_style_text_opa(ui_droStartup_tweakgen, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droColour_tweakgen = lv_dropdown_create(ui_pnlColour_tweakgen);
    lv_dropdown_clear_options(ui_droColour_tweakgen);
    lv_obj_set_style_text_opa(ui_droColour_tweakgen, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droRTC_tweakgen = lv_dropdown_create(ui_pnlRTC_tweakgen);
    lv_dropdown_clear_options(ui_droRTC_tweakgen);
    lv_obj_set_style_text_opa(ui_droRTC_tweakgen, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droBrightness_tweakgen = lv_dropdown_create(ui_pnlBrightness_tweakgen);
    lv_dropdown_clear_options(ui_droBrightness_tweakgen);
    lv_obj_set_style_text_opa(ui_droBrightness_tweakgen, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droVolume_tweakgen = lv_dropdown_create(ui_pnlVolume_tweakgen);
    lv_dropdown_clear_options(ui_droVolume_tweakgen);
    lv_obj_set_style_text_opa(ui_droVolume_tweakgen, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droHDMI_tweakgen = lv_dropdown_create(ui_pnlHDMI_tweakgen);
    lv_dropdown_clear_options(ui_droHDMI_tweakgen);
    lv_obj_set_style_text_opa(ui_droHDMI_tweakgen, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droAdvanced_tweakgen = lv_dropdown_create(ui_pnlAdvanced_tweakgen);
    lv_dropdown_clear_options(ui_droAdvanced_tweakgen);
    lv_obj_set_style_text_opa(ui_droAdvanced_tweakgen, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}
