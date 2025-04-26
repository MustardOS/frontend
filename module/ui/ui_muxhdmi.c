#include "ui_muxhdmi.h"

lv_obj_t *ui_pnlResolution_hdmi;
lv_obj_t *ui_pnlSpace_hdmi;
lv_obj_t *ui_pnlDepth_hdmi;
lv_obj_t *ui_pnlRange_hdmi;
lv_obj_t *ui_pnlScan_hdmi;
lv_obj_t *ui_pnlAudio_hdmi;

lv_obj_t *ui_lblResolution_hdmi;
lv_obj_t *ui_lblSpace_hdmi;
lv_obj_t *ui_lblDepth_hdmi;
lv_obj_t *ui_lblRange_hdmi;
lv_obj_t *ui_lblScan_hdmi;
lv_obj_t *ui_lblAudio_hdmi;

lv_obj_t *ui_icoResolution_hdmi;
lv_obj_t *ui_icoSpace_hdmi;
lv_obj_t *ui_icoDepth_hdmi;
lv_obj_t *ui_icoRange_hdmi;
lv_obj_t *ui_icoScan_hdmi;
lv_obj_t *ui_icoAudio_hdmi;

lv_obj_t *ui_droResolution_hdmi;
lv_obj_t *ui_droSpace_hdmi;
lv_obj_t *ui_droDepth_hdmi;
lv_obj_t *ui_droRange_hdmi;
lv_obj_t *ui_droScan_hdmi;
lv_obj_t *ui_droAudio_hdmi;

void init_muxhdmi(lv_obj_t *ui_pnlContent) {
    ui_pnlResolution_hdmi = lv_obj_create(ui_pnlContent);
    ui_pnlSpace_hdmi = lv_obj_create(ui_pnlContent);
    ui_pnlDepth_hdmi = lv_obj_create(ui_pnlContent);
    ui_pnlRange_hdmi = lv_obj_create(ui_pnlContent);
    ui_pnlScan_hdmi = lv_obj_create(ui_pnlContent);
    ui_pnlAudio_hdmi = lv_obj_create(ui_pnlContent);

    ui_lblResolution_hdmi = lv_label_create(ui_pnlResolution_hdmi);
    lv_label_set_text(ui_lblResolution_hdmi, "");
    ui_lblSpace_hdmi = lv_label_create(ui_pnlSpace_hdmi);
    lv_label_set_text(ui_lblSpace_hdmi, "");
    ui_lblDepth_hdmi = lv_label_create(ui_pnlDepth_hdmi);
    lv_label_set_text(ui_lblDepth_hdmi, "");
    ui_lblRange_hdmi = lv_label_create(ui_pnlRange_hdmi);
    lv_label_set_text(ui_lblRange_hdmi, "");
    ui_lblScan_hdmi = lv_label_create(ui_pnlScan_hdmi);
    lv_label_set_text(ui_lblScan_hdmi, "");
    ui_lblAudio_hdmi = lv_label_create(ui_pnlAudio_hdmi);
    lv_label_set_text(ui_lblAudio_hdmi, "");

    ui_icoResolution_hdmi = lv_img_create(ui_pnlResolution_hdmi);
    ui_icoSpace_hdmi = lv_img_create(ui_pnlSpace_hdmi);
    ui_icoDepth_hdmi = lv_img_create(ui_pnlDepth_hdmi);
    ui_icoRange_hdmi = lv_img_create(ui_pnlRange_hdmi);
    ui_icoScan_hdmi = lv_img_create(ui_pnlScan_hdmi);
    ui_icoAudio_hdmi = lv_img_create(ui_pnlAudio_hdmi);

    ui_droResolution_hdmi = lv_dropdown_create(ui_pnlResolution_hdmi);
    lv_dropdown_clear_options(ui_droResolution_hdmi);
    lv_obj_set_style_text_opa(ui_droResolution_hdmi, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droSpace_hdmi = lv_dropdown_create(ui_pnlSpace_hdmi);
    lv_dropdown_clear_options(ui_droSpace_hdmi);
    lv_obj_set_style_text_opa(ui_droSpace_hdmi, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droDepth_hdmi = lv_dropdown_create(ui_pnlDepth_hdmi);
    lv_dropdown_clear_options(ui_droDepth_hdmi);
    lv_obj_set_style_text_opa(ui_droDepth_hdmi, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droRange_hdmi = lv_dropdown_create(ui_pnlRange_hdmi);
    lv_dropdown_clear_options(ui_droRange_hdmi);
    lv_obj_set_style_text_opa(ui_droRange_hdmi, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droScan_hdmi = lv_dropdown_create(ui_pnlScan_hdmi);
    lv_dropdown_clear_options(ui_droScan_hdmi);
    lv_obj_set_style_text_opa(ui_droScan_hdmi, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droAudio_hdmi = lv_dropdown_create(ui_pnlAudio_hdmi);
    lv_dropdown_clear_options(ui_droAudio_hdmi);
    lv_obj_set_style_text_opa(ui_droAudio_hdmi, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}
