#include "ui_muxnetinfo.h"

lv_obj_t *ui_pnlIP_netinfo;
lv_obj_t *ui_pnlMAC_netinfo;
lv_obj_t *ui_pnlSSID_netinfo;
lv_obj_t *ui_pnlGateway_netinfo;
lv_obj_t *ui_pnlDNS_netinfo;
lv_obj_t *ui_pnlSignal_netinfo;
lv_obj_t *ui_pnlTraffic_netinfo;

lv_obj_t *ui_lblIP_netinfo;
lv_obj_t *ui_lblMAC_netinfo;
lv_obj_t *ui_lblSSID_netinfo;
lv_obj_t *ui_lblGateway_netinfo;
lv_obj_t *ui_lblDNS_netinfo;
lv_obj_t *ui_lblSignal_netinfo;
lv_obj_t *ui_lblTraffic_netinfo;

lv_obj_t *ui_icoIP_netinfo;
lv_obj_t *ui_icoMAC_netinfo;
lv_obj_t *ui_icoSSID_netinfo;
lv_obj_t *ui_icoGateway_netinfo;
lv_obj_t *ui_icoDNS_netinfo;
lv_obj_t *ui_icoSignal_netinfo;
lv_obj_t *ui_icoTraffic_netinfo;

lv_obj_t *ui_lblIPValue_netinfo;
lv_obj_t *ui_lblMACValue_netinfo;
lv_obj_t *ui_lblSSIDValue_netinfo;
lv_obj_t *ui_lblGatewayValue_netinfo;
lv_obj_t *ui_lblDNSValue_netinfo;
lv_obj_t *ui_lblSignalValue_netinfo;
lv_obj_t *ui_lblTrafficValue_netinfo;

void init_muxnetinfo(lv_obj_t *ui_pnlContent) {
    ui_pnlIP_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlMAC_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlSSID_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlGateway_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlDNS_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlSignal_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlTraffic_netinfo = lv_obj_create(ui_pnlContent);

    ui_lblIP_netinfo = lv_label_create(ui_pnlIP_netinfo);
    lv_label_set_text(ui_lblIP_netinfo, "");
    ui_lblMAC_netinfo = lv_label_create(ui_pnlMAC_netinfo);
    lv_label_set_text(ui_lblMAC_netinfo, "");
    ui_lblSSID_netinfo = lv_label_create(ui_pnlSSID_netinfo);
    lv_label_set_text(ui_lblSSID_netinfo, "");
    ui_lblGateway_netinfo = lv_label_create(ui_pnlGateway_netinfo);
    lv_label_set_text(ui_lblGateway_netinfo, "");
    ui_lblDNS_netinfo = lv_label_create(ui_pnlDNS_netinfo);
    lv_label_set_text(ui_lblDNS_netinfo, "");
    ui_lblSignal_netinfo = lv_label_create(ui_pnlSignal_netinfo);
    lv_label_set_text(ui_lblSignal_netinfo, "");
    ui_lblTraffic_netinfo = lv_label_create(ui_pnlTraffic_netinfo);
    lv_label_set_text(ui_lblTraffic_netinfo, "");

    ui_icoIP_netinfo = lv_img_create(ui_pnlIP_netinfo);
    ui_icoMAC_netinfo = lv_img_create(ui_pnlMAC_netinfo);
    ui_icoSSID_netinfo = lv_img_create(ui_pnlSSID_netinfo);
    ui_icoGateway_netinfo = lv_img_create(ui_pnlGateway_netinfo);
    ui_icoDNS_netinfo = lv_img_create(ui_pnlDNS_netinfo);
    ui_icoSignal_netinfo = lv_img_create(ui_pnlSignal_netinfo);
    ui_icoTraffic_netinfo = lv_img_create(ui_pnlTraffic_netinfo);

    ui_lblIPValue_netinfo = lv_label_create(ui_pnlIP_netinfo);
    lv_label_set_text(ui_lblIPValue_netinfo, "");
    ui_lblMACValue_netinfo = lv_label_create(ui_pnlMAC_netinfo);
    lv_label_set_text(ui_lblMACValue_netinfo, "");
    ui_lblSSIDValue_netinfo = lv_label_create(ui_pnlSSID_netinfo);
    lv_label_set_text(ui_lblSSIDValue_netinfo, "");
    ui_lblGatewayValue_netinfo = lv_label_create(ui_pnlGateway_netinfo);
    lv_label_set_text(ui_lblGatewayValue_netinfo, "");
    ui_lblDNSValue_netinfo = lv_label_create(ui_pnlDNS_netinfo);
    lv_label_set_text(ui_lblDNSValue_netinfo, "");
    ui_lblSignalValue_netinfo = lv_label_create(ui_pnlSignal_netinfo);
    lv_label_set_text(ui_lblSignalValue_netinfo, "");
    ui_lblTrafficValue_netinfo = lv_label_create(ui_pnlTraffic_netinfo);
    lv_label_set_text(ui_lblTrafficValue_netinfo, "");
}
