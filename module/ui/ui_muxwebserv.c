#include "ui_muxwebserv.h"

lv_obj_t *ui_pnlSSHD_webserv;
lv_obj_t *ui_pnlSFTPGo_webserv;
lv_obj_t *ui_pnlTTYD_webserv;
lv_obj_t *ui_pnlSyncthing_webserv;
lv_obj_t *ui_pnlRSLSync_webserv;
lv_obj_t *ui_pnlNTP_webserv;
lv_obj_t *ui_pnlTailscaled_webserv;

lv_obj_t *ui_lblSSHD_webserv;
lv_obj_t *ui_lblSFTPGo_webserv;
lv_obj_t *ui_lblTTYD_webserv;
lv_obj_t *ui_lblSyncthing_webserv;
lv_obj_t *ui_lblRSLSync_webserv;
lv_obj_t *ui_lblNTP_webserv;
lv_obj_t *ui_lblTailscaled_webserv;

lv_obj_t *ui_icoSSHD_webserv;
lv_obj_t *ui_icoSFTPGo_webserv;
lv_obj_t *ui_icoTTYD_webserv;
lv_obj_t *ui_icoSyncthing_webserv;
lv_obj_t *ui_icoRSLSync_webserv;
lv_obj_t *ui_icoNTP_webserv;
lv_obj_t *ui_icoTailscaled_webserv;

lv_obj_t *ui_droSSHD_webserv;
lv_obj_t *ui_droSFTPGo_webserv;
lv_obj_t *ui_droTTYD_webserv;
lv_obj_t *ui_droSyncthing_webserv;
lv_obj_t *ui_droRSLSync_webserv;
lv_obj_t *ui_droNTP_webserv;
lv_obj_t *ui_droTailscaled_webserv;

void init_muxwebserv(lv_obj_t *ui_pnlContent) {
#define CREATE_ELEMENT_ITEM(module, name) do {                                                       \
        ui_pnl##name##_##module = lv_obj_create(ui_pnlContent);                                      \
        ui_lbl##name##_##module = lv_label_create(ui_pnl##name##_##module);                          \
        lv_label_set_text(ui_lbl##name##_##module, "");                                              \
        ui_ico##name##_##module = lv_img_create(ui_pnl##name##_##module);                            \
        ui_dro##name##_##module = lv_dropdown_create(ui_pnl##name##_##module);                       \
        lv_dropdown_clear_options(ui_dro##name##_##module);                                          \
        lv_obj_set_style_text_opa(ui_dro##name##_##module, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT); \
    } while (0)

    CREATE_ELEMENT_ITEM(webserv, SSHD);
    CREATE_ELEMENT_ITEM(webserv, SFTPGo);
    CREATE_ELEMENT_ITEM(webserv, TTYD);
    CREATE_ELEMENT_ITEM(webserv, Syncthing);
    CREATE_ELEMENT_ITEM(webserv, RSLSync);
    CREATE_ELEMENT_ITEM(webserv, NTP);
    CREATE_ELEMENT_ITEM(webserv, Tailscaled);

#undef CREATE_ELEMENT_ITEM
}
