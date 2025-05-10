#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxnetinfo(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

extern lv_obj_t *ui_pnlHostname_netinfo;
extern lv_obj_t *ui_pnlMAC_netinfo;
extern lv_obj_t *ui_pnlIP_netinfo;
extern lv_obj_t *ui_pnlSSID_netinfo;
extern lv_obj_t *ui_pnlGateway_netinfo;
extern lv_obj_t *ui_pnlDNS_netinfo;
extern lv_obj_t *ui_pnlSignal_netinfo;
extern lv_obj_t *ui_pnlChannel_netinfo;
extern lv_obj_t *ui_pnlACTraffic_netinfo;
extern lv_obj_t *ui_pnlTPTraffic_netinfo;

extern lv_obj_t *ui_lblHostname_netinfo;
extern lv_obj_t *ui_lblMAC_netinfo;
extern lv_obj_t *ui_lblIP_netinfo;
extern lv_obj_t *ui_lblSSID_netinfo;
extern lv_obj_t *ui_lblGateway_netinfo;
extern lv_obj_t *ui_lblDNS_netinfo;
extern lv_obj_t *ui_lblSignal_netinfo;
extern lv_obj_t *ui_lblChannel_netinfo;
extern lv_obj_t *ui_lblACTraffic_netinfo;
extern lv_obj_t *ui_lblTPTraffic_netinfo;

extern lv_obj_t *ui_icoHostname_netinfo;
extern lv_obj_t *ui_icoMAC_netinfo;
extern lv_obj_t *ui_icoIP_netinfo;
extern lv_obj_t *ui_icoSSID_netinfo;
extern lv_obj_t *ui_icoGateway_netinfo;
extern lv_obj_t *ui_icoDNS_netinfo;
extern lv_obj_t *ui_icoSignal_netinfo;
extern lv_obj_t *ui_icoChannel_netinfo;
extern lv_obj_t *ui_icoACTraffic_netinfo;
extern lv_obj_t *ui_icoTPTraffic_netinfo;

extern lv_obj_t *ui_lblHostnameValue_netinfo;
extern lv_obj_t *ui_lblMACValue_netinfo;
extern lv_obj_t *ui_lblIPValue_netinfo;
extern lv_obj_t *ui_lblSSIDValue_netinfo;
extern lv_obj_t *ui_lblGatewayValue_netinfo;
extern lv_obj_t *ui_lblDNSValue_netinfo;
extern lv_obj_t *ui_lblSignalValue_netinfo;
extern lv_obj_t *ui_lblChannelValue_netinfo;
extern lv_obj_t *ui_lblACTrafficValue_netinfo;
extern lv_obj_t *ui_lblTPTrafficValue_netinfo;

extern lv_obj_t *ui_pnlEntry_netinfo;
extern lv_obj_t *ui_txtEntry_netinfo;
