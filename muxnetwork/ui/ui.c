#include "ui.h"

void ui_screen_init(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

lv_obj_t *ui_scrNetwork;
lv_obj_t *ui_pnlEnable;
lv_obj_t *ui_pnlIdentifier;
lv_obj_t *ui_pnlPassword;
lv_obj_t *ui_pnlType;
lv_obj_t *ui_pnlAddress;
lv_obj_t *ui_pnlSubnet;
lv_obj_t *ui_pnlGateway;
lv_obj_t *ui_pnlDNS;
lv_obj_t *ui_pnlConnect;
lv_obj_t *ui_pnlStatusCore;
lv_obj_t *ui_pnlStatus;
lv_obj_t *ui_lblEnable;
lv_obj_t *ui_lblIdentifier;
lv_obj_t *ui_lblPassword;
lv_obj_t *ui_lblType;
lv_obj_t *ui_lblAddress;
lv_obj_t *ui_lblSubnet;
lv_obj_t *ui_lblGateway;
lv_obj_t *ui_lblDNS;
lv_obj_t *ui_lblConnect;
lv_obj_t *ui_lblStatus;
lv_obj_t *ui_icoEnable;
lv_obj_t *ui_icoIdentifier;
lv_obj_t *ui_icoPassword;
lv_obj_t *ui_icoType;
lv_obj_t *ui_icoAddress;
lv_obj_t *ui_icoSubnet;
lv_obj_t *ui_icoGateway;
lv_obj_t *ui_icoDNS;
lv_obj_t *ui_icoConnect;
lv_obj_t *ui_icoStatus;
lv_obj_t *ui_lblEnableValue;
lv_obj_t *ui_lblIdentifierValue;
lv_obj_t *ui_lblPasswordValue;
lv_obj_t *ui_lblTypeValue;
lv_obj_t *ui_lblAddressValue;
lv_obj_t *ui_lblSubnetValue;
lv_obj_t *ui_lblGatewayValue;
lv_obj_t *ui_lblDNSValue;
lv_obj_t *ui_lblConnectValue;
lv_obj_t *ui_lblStatusValue;
lv_obj_t *ui_pnlEntry;
lv_obj_t *ui_txtEntry;

void ui_init(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
    ui_screen_init(ui_screen, ui_pnlContent, theme);
}
