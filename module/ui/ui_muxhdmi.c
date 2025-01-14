#include "ui_muxhdmi.h"

lv_obj_t *ui_pnlEnable;
lv_obj_t *ui_pnlResolution;
lv_obj_t *ui_pnlThemeResolution;
lv_obj_t *ui_pnlSpace;
lv_obj_t *ui_pnlDepth;
lv_obj_t *ui_pnlRange;
lv_obj_t *ui_pnlScan;
lv_obj_t *ui_pnlAudio;

lv_obj_t *ui_lblEnable;
lv_obj_t *ui_lblResolution;
lv_obj_t *ui_lblThemeResolution;
lv_obj_t *ui_lblSpace;
lv_obj_t *ui_lblDepth;
lv_obj_t *ui_lblRange;
lv_obj_t *ui_lblScan;
lv_obj_t *ui_lblAudio;

lv_obj_t *ui_icoEnable;
lv_obj_t *ui_icoResolution;
lv_obj_t *ui_icoThemeResolution;
lv_obj_t *ui_icoSpace;
lv_obj_t *ui_icoDepth;
lv_obj_t *ui_icoRange;
lv_obj_t *ui_icoScan;
lv_obj_t *ui_icoAudio;

lv_obj_t *ui_droEnable;
lv_obj_t *ui_droResolution;
lv_obj_t *ui_droThemeResolution;
lv_obj_t *ui_droSpace;
lv_obj_t *ui_droDepth;
lv_obj_t *ui_droRange;
lv_obj_t *ui_droScan;
lv_obj_t *ui_droAudio;

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlEnable = lv_obj_create(ui_pnlContent);
    ui_pnlResolution = lv_obj_create(ui_pnlContent);
    ui_pnlThemeResolution = lv_obj_create(ui_pnlContent);
    ui_pnlSpace = lv_obj_create(ui_pnlContent);
    ui_pnlDepth = lv_obj_create(ui_pnlContent);
    ui_pnlRange = lv_obj_create(ui_pnlContent);
    ui_pnlScan = lv_obj_create(ui_pnlContent);
    ui_pnlAudio = lv_obj_create(ui_pnlContent);

    ui_lblEnable = lv_label_create(ui_pnlEnable);
    ui_lblResolution = lv_label_create(ui_pnlResolution);
    ui_lblThemeResolution = lv_label_create(ui_pnlThemeResolution);
    ui_lblSpace = lv_label_create(ui_pnlSpace);
    ui_lblDepth = lv_label_create(ui_pnlDepth);
    ui_lblRange = lv_label_create(ui_pnlRange);
    ui_lblScan = lv_label_create(ui_pnlScan);
    ui_lblAudio = lv_label_create(ui_pnlAudio);

    ui_icoEnable = lv_img_create(ui_pnlEnable);
    ui_icoResolution = lv_img_create(ui_pnlResolution);
    ui_icoThemeResolution = lv_img_create(ui_pnlThemeResolution);
    ui_icoSpace = lv_img_create(ui_pnlSpace);
    ui_icoDepth = lv_img_create(ui_pnlDepth);
    ui_icoRange = lv_img_create(ui_pnlRange);
    ui_icoScan = lv_img_create(ui_pnlScan);
    ui_icoAudio = lv_img_create(ui_pnlAudio);

    ui_droEnable = lv_dropdown_create(ui_pnlEnable);
    ui_droResolution = lv_dropdown_create(ui_pnlResolution);
    ui_droThemeResolution = lv_dropdown_create(ui_pnlThemeResolution);
    ui_droSpace = lv_dropdown_create(ui_pnlSpace);
    ui_droDepth = lv_dropdown_create(ui_pnlDepth);
    ui_droRange = lv_dropdown_create(ui_pnlRange);
    ui_droScan = lv_dropdown_create(ui_pnlScan);
    ui_droAudio = lv_dropdown_create(ui_pnlAudio);
}
