#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxnetwork(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

extern lv_obj_t *ui_pnlIdentifier_network;
extern lv_obj_t *ui_pnlPassword_network;
extern lv_obj_t *ui_pnlScan_network;
extern lv_obj_t *ui_pnlType_network;
extern lv_obj_t *ui_pnlAddress_network;
extern lv_obj_t *ui_pnlSubnet_network;
extern lv_obj_t *ui_pnlGateway_network;
extern lv_obj_t *ui_pnlDNS_network;
extern lv_obj_t *ui_pnlConnect_network;

extern lv_obj_t *ui_lblIdentifier_network;
extern lv_obj_t *ui_lblPassword_network;
extern lv_obj_t *ui_lblScan_network;
extern lv_obj_t *ui_lblType_network;
extern lv_obj_t *ui_lblAddress_network;
extern lv_obj_t *ui_lblSubnet_network;
extern lv_obj_t *ui_lblGateway_network;
extern lv_obj_t *ui_lblDNS_network;
extern lv_obj_t *ui_lblConnect_network;

extern lv_obj_t *ui_icoIdentifier_network;
extern lv_obj_t *ui_icoPassword_network;
extern lv_obj_t *ui_icoScan_network;
extern lv_obj_t *ui_icoType_network;
extern lv_obj_t *ui_icoAddress_network;
extern lv_obj_t *ui_icoSubnet_network;
extern lv_obj_t *ui_icoGateway_network;
extern lv_obj_t *ui_icoDNS_network;
extern lv_obj_t *ui_icoConnect_network;

extern lv_obj_t *ui_lblIdentifier_networkValue;
extern lv_obj_t *ui_lblPassword_networkValue;
extern lv_obj_t *ui_lblScan_networkValue;
extern lv_obj_t *ui_lblType_networkValue;
extern lv_obj_t *ui_lblAddress_networkValue;
extern lv_obj_t *ui_lblSubnet_networkValue;
extern lv_obj_t *ui_lblGateway_networkValue;
extern lv_obj_t *ui_lblDNS_networkValue;
extern lv_obj_t *ui_lblConnect_networkValue;

extern lv_obj_t *ui_pnlEntry_network;
extern lv_obj_t *ui_txtEntry_network;
