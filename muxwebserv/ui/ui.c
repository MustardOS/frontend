#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

lv_obj_t *ui_pnlSSHD;
lv_obj_t *ui_pnlSFTPGo;
lv_obj_t *ui_pnlTTYD;
lv_obj_t *ui_pnlSyncthing;
lv_obj_t *ui_pnlRSLSync;
lv_obj_t *ui_pnlNTP;
lv_obj_t *ui_lblSSHD;
lv_obj_t *ui_lblSFTPGo;
lv_obj_t *ui_lblTTYD;
lv_obj_t *ui_lblSyncthing;
lv_obj_t *ui_lblRSLSync;
lv_obj_t *ui_lblNTP;
lv_obj_t *ui_icoSSHD;
lv_obj_t *ui_icoSFTPGo;
lv_obj_t *ui_icoTTYD;
lv_obj_t *ui_icoSyncthing;
lv_obj_t *ui_icoRSLSync;
lv_obj_t *ui_icoNTP;
lv_obj_t *ui_droSSHD;
lv_obj_t *ui_droSFTPGo;
lv_obj_t *ui_droTTYD;
lv_obj_t *ui_droSyncthing;
lv_obj_t *ui_droRSLSync;
lv_obj_t *ui_droNTP;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_screen_init(ui_pnlContent);
}
