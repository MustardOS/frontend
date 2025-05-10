#include "ui_muxnetinfo.h"
#include "../../common/device.h"

lv_obj_t *ui_pnlHostname_netinfo;
lv_obj_t *ui_pnlMAC_netinfo;
lv_obj_t *ui_pnlIP_netinfo;
lv_obj_t *ui_pnlSSID_netinfo;
lv_obj_t *ui_pnlGateway_netinfo;
lv_obj_t *ui_pnlDNS_netinfo;
lv_obj_t *ui_pnlSignal_netinfo;
lv_obj_t *ui_pnlChannel_netinfo;
lv_obj_t *ui_pnlACTraffic_netinfo;
lv_obj_t *ui_pnlTPTraffic_netinfo;

lv_obj_t *ui_lblHostname_netinfo;
lv_obj_t *ui_lblMAC_netinfo;
lv_obj_t *ui_lblIP_netinfo;
lv_obj_t *ui_lblSSID_netinfo;
lv_obj_t *ui_lblGateway_netinfo;
lv_obj_t *ui_lblDNS_netinfo;
lv_obj_t *ui_lblSignal_netinfo;
lv_obj_t *ui_lblChannel_netinfo;
lv_obj_t *ui_lblACTraffic_netinfo;
lv_obj_t *ui_lblTPTraffic_netinfo;

lv_obj_t *ui_icoHostname_netinfo;
lv_obj_t *ui_icoMAC_netinfo;
lv_obj_t *ui_icoIP_netinfo;
lv_obj_t *ui_icoSSID_netinfo;
lv_obj_t *ui_icoGateway_netinfo;
lv_obj_t *ui_icoDNS_netinfo;
lv_obj_t *ui_icoSignal_netinfo;
lv_obj_t *ui_icoChannel_netinfo;
lv_obj_t *ui_icoACTraffic_netinfo;
lv_obj_t *ui_icoTPTraffic_netinfo;

lv_obj_t *ui_lblHostnameValue_netinfo;
lv_obj_t *ui_lblMACValue_netinfo;
lv_obj_t *ui_lblIPValue_netinfo;
lv_obj_t *ui_lblSSIDValue_netinfo;
lv_obj_t *ui_lblGatewayValue_netinfo;
lv_obj_t *ui_lblDNSValue_netinfo;
lv_obj_t *ui_lblSignalValue_netinfo;
lv_obj_t *ui_lblChannelValue_netinfo;
lv_obj_t *ui_lblACTrafficValue_netinfo;
lv_obj_t *ui_lblTPTrafficValue_netinfo;

lv_obj_t *ui_pnlEntry_netinfo;
lv_obj_t *ui_txtEntry_netinfo;

void init_muxnetinfo(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
    ui_pnlHostname_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlMAC_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlIP_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlSSID_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlGateway_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlDNS_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlSignal_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlChannel_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlACTraffic_netinfo = lv_obj_create(ui_pnlContent);
    ui_pnlTPTraffic_netinfo = lv_obj_create(ui_pnlContent);

    ui_lblHostname_netinfo = lv_label_create(ui_pnlHostname_netinfo);
    lv_label_set_text(ui_lblHostname_netinfo, "");
    ui_lblMAC_netinfo = lv_label_create(ui_pnlMAC_netinfo);
    lv_label_set_text(ui_lblMAC_netinfo, "");
    ui_lblIP_netinfo = lv_label_create(ui_pnlIP_netinfo);
    lv_label_set_text(ui_lblIP_netinfo, "");
    ui_lblSSID_netinfo = lv_label_create(ui_pnlSSID_netinfo);
    lv_label_set_text(ui_lblSSID_netinfo, "");
    ui_lblGateway_netinfo = lv_label_create(ui_pnlGateway_netinfo);
    lv_label_set_text(ui_lblGateway_netinfo, "");
    ui_lblDNS_netinfo = lv_label_create(ui_pnlDNS_netinfo);
    lv_label_set_text(ui_lblDNS_netinfo, "");
    ui_lblSignal_netinfo = lv_label_create(ui_pnlSignal_netinfo);
    lv_label_set_text(ui_lblSignal_netinfo, "");
    ui_lblChannel_netinfo = lv_label_create(ui_pnlChannel_netinfo);
    lv_label_set_text(ui_lblChannel_netinfo, "");
    ui_lblACTraffic_netinfo = lv_label_create(ui_pnlACTraffic_netinfo);
    lv_label_set_text(ui_lblACTraffic_netinfo, "");
    ui_lblTPTraffic_netinfo = lv_label_create(ui_pnlTPTraffic_netinfo);
    lv_label_set_text(ui_lblTPTraffic_netinfo, "");

    ui_icoHostname_netinfo = lv_img_create(ui_pnlHostname_netinfo);
    ui_icoMAC_netinfo = lv_img_create(ui_pnlMAC_netinfo);
    ui_icoIP_netinfo = lv_img_create(ui_pnlIP_netinfo);
    ui_icoSSID_netinfo = lv_img_create(ui_pnlSSID_netinfo);
    ui_icoGateway_netinfo = lv_img_create(ui_pnlGateway_netinfo);
    ui_icoDNS_netinfo = lv_img_create(ui_pnlDNS_netinfo);
    ui_icoSignal_netinfo = lv_img_create(ui_pnlSignal_netinfo);
    ui_icoChannel_netinfo = lv_img_create(ui_pnlChannel_netinfo);
    ui_icoACTraffic_netinfo = lv_img_create(ui_pnlACTraffic_netinfo);
    ui_icoTPTraffic_netinfo = lv_img_create(ui_pnlTPTraffic_netinfo);

    ui_lblHostnameValue_netinfo = lv_label_create(ui_pnlHostname_netinfo);
    lv_label_set_text(ui_lblHostnameValue_netinfo, "");
    ui_lblMACValue_netinfo = lv_label_create(ui_pnlMAC_netinfo);
    lv_label_set_text(ui_lblMACValue_netinfo, "");
    ui_lblIPValue_netinfo = lv_label_create(ui_pnlIP_netinfo);
    lv_label_set_text(ui_lblIPValue_netinfo, "");
    ui_lblSSIDValue_netinfo = lv_label_create(ui_pnlSSID_netinfo);
    lv_label_set_text(ui_lblSSIDValue_netinfo, "");
    ui_lblGatewayValue_netinfo = lv_label_create(ui_pnlGateway_netinfo);
    lv_label_set_text(ui_lblGatewayValue_netinfo, "");
    ui_lblDNSValue_netinfo = lv_label_create(ui_pnlDNS_netinfo);
    lv_label_set_text(ui_lblDNSValue_netinfo, "");
    ui_lblSignalValue_netinfo = lv_label_create(ui_pnlSignal_netinfo);
    lv_label_set_text(ui_lblSignalValue_netinfo, "");
    ui_lblChannelValue_netinfo = lv_label_create(ui_pnlChannel_netinfo);
    lv_label_set_text(ui_lblChannelValue_netinfo, "");
    ui_lblACTrafficValue_netinfo = lv_label_create(ui_pnlACTraffic_netinfo);
    lv_label_set_text(ui_lblACTrafficValue_netinfo, "");
    ui_lblTPTrafficValue_netinfo = lv_label_create(ui_pnlTPTraffic_netinfo);
    lv_label_set_text(ui_lblTPTrafficValue_netinfo, "");

    ui_pnlEntry_netinfo = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlEntry_netinfo, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlEntry_netinfo, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlEntry_netinfo, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlEntry_netinfo, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlEntry_netinfo, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlEntry_netinfo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlEntry_netinfo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlEntry_netinfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlEntry_netinfo, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlEntry_netinfo, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlEntry_netinfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_txtEntry_netinfo = lv_textarea_create(ui_pnlEntry_netinfo);
    lv_obj_set_width(ui_txtEntry_netinfo, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(ui_txtEntry_netinfo, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txtEntry_netinfo, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txtEntry_netinfo, 1024);
    lv_textarea_set_one_line(ui_txtEntry_netinfo, true);
    lv_obj_set_style_radius(ui_txtEntry_netinfo, theme->OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_txtEntry_netinfo, lv_color_hex(theme->OSK.BORDER),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_txtEntry_netinfo, theme->OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_txtEntry_netinfo, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
}
