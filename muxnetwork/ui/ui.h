#ifndef _MUXNETWORK_UI_H
#define _MUXNETWORK_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void ui_screen_init(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

extern lv_obj_t *ui_scrNetwork;
extern lv_obj_t *ui_pnlEnable;
extern lv_obj_t *ui_pnlIdentifier;
extern lv_obj_t *ui_pnlPassword;
extern lv_obj_t *ui_pnlType;
extern lv_obj_t *ui_pnlAddress;
extern lv_obj_t *ui_pnlSubnet;
extern lv_obj_t *ui_pnlGateway;
extern lv_obj_t *ui_pnlDNS;
extern lv_obj_t *ui_pnlConnect;
extern lv_obj_t *ui_pnlStatusCore;
extern lv_obj_t *ui_pnlStatus;
extern lv_obj_t *ui_lblEnable;
extern lv_obj_t *ui_lblIdentifier;
extern lv_obj_t *ui_lblPassword;
extern lv_obj_t *ui_lblType;
extern lv_obj_t *ui_lblAddress;
extern lv_obj_t *ui_lblSubnet;
extern lv_obj_t *ui_lblGateway;
extern lv_obj_t *ui_lblDNS;
extern lv_obj_t *ui_lblConnect;
extern lv_obj_t *ui_lblStatus;
extern lv_obj_t *ui_icoEnable;
extern lv_obj_t *ui_icoIdentifier;
extern lv_obj_t *ui_icoPassword;
extern lv_obj_t *ui_icoType;
extern lv_obj_t *ui_icoAddress;
extern lv_obj_t *ui_icoSubnet;
extern lv_obj_t *ui_icoGateway;
extern lv_obj_t *ui_icoDNS;
extern lv_obj_t *ui_icoConnect;
extern lv_obj_t *ui_icoStatus;
extern lv_obj_t *ui_lblEnableValue;
extern lv_obj_t *ui_lblIdentifierValue;
extern lv_obj_t *ui_lblPasswordValue;
extern lv_obj_t *ui_lblTypeValue;
extern lv_obj_t *ui_lblAddressValue;
extern lv_obj_t *ui_lblSubnetValue;
extern lv_obj_t *ui_lblGatewayValue;
extern lv_obj_t *ui_lblDNSValue;
extern lv_obj_t *ui_lblConnectValue;
extern lv_obj_t *ui_lblStatusValue;
extern lv_obj_t *ui_pnlEntry;
extern lv_obj_t *ui_txtEntry;

void ui_init(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
