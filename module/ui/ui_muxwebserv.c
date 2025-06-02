#include "../../common/options.h"
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
    CREATE_ELEMENT_ITEM(webserv, SSHD);
    CREATE_ELEMENT_ITEM(webserv, SFTPGo);
    CREATE_ELEMENT_ITEM(webserv, TTYD);
    CREATE_ELEMENT_ITEM(webserv, Syncthing);
    CREATE_ELEMENT_ITEM(webserv, RSLSync);
    CREATE_ELEMENT_ITEM(webserv, NTP);
    CREATE_ELEMENT_ITEM(webserv, Tailscaled);
}
