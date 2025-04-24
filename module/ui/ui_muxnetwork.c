#include "ui_muxnetwork.h"
#include "../../common/device.h"

lv_obj_t *ui_scrNetwork_network;

lv_obj_t *ui_pnlIdentifier_network;
lv_obj_t *ui_pnlPassword_network;
lv_obj_t *ui_pnlScan_network;
lv_obj_t *ui_pnlType_network;
lv_obj_t *ui_pnlAddress_network;
lv_obj_t *ui_pnlSubnet_network;
lv_obj_t *ui_pnlGateway_network;
lv_obj_t *ui_pnlDNS_network;
lv_obj_t *ui_pnlConnect_network;

lv_obj_t *ui_lblIdentifier_network;
lv_obj_t *ui_lblPassword_network;
lv_obj_t *ui_lblScan_network;
lv_obj_t *ui_lblType_network;
lv_obj_t *ui_lblAddress_network;
lv_obj_t *ui_lblSubnet_network;
lv_obj_t *ui_lblGateway_network;
lv_obj_t *ui_lblDNS_network;
lv_obj_t *ui_lblConnect_network;

lv_obj_t *ui_icoIdentifier_network;
lv_obj_t *ui_icoPassword_network;
lv_obj_t *ui_icoScan_network;
lv_obj_t *ui_icoType_network;
lv_obj_t *ui_icoAddress_network;
lv_obj_t *ui_icoSubnet_network;
lv_obj_t *ui_icoGateway_network;
lv_obj_t *ui_icoDNS_network;
lv_obj_t *ui_icoConnect_network;

lv_obj_t *ui_lblIdentifier_networkValue;
lv_obj_t *ui_lblPassword_networkValue;
lv_obj_t *ui_lblScan_networkValue;
lv_obj_t *ui_lblType_networkValue;
lv_obj_t *ui_lblAddress_networkValue;
lv_obj_t *ui_lblSubnet_networkValue;
lv_obj_t *ui_lblGateway_networkValue;
lv_obj_t *ui_lblDNS_networkValue;
lv_obj_t *ui_lblConnect_networkValue;

lv_obj_t *ui_pnlEntry_network;
lv_obj_t *ui_txtEntry_network;

void init_muxnetwork(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
    ui_pnlIdentifier_network = lv_obj_create(ui_pnlContent);
    ui_pnlPassword_network = lv_obj_create(ui_pnlContent);
    ui_pnlScan_network = lv_obj_create(ui_pnlContent);
    ui_pnlType_network = lv_obj_create(ui_pnlContent);
    ui_pnlAddress_network = lv_obj_create(ui_pnlContent);
    ui_pnlSubnet_network = lv_obj_create(ui_pnlContent);
    ui_pnlGateway_network = lv_obj_create(ui_pnlContent);
    ui_pnlDNS_network = lv_obj_create(ui_pnlContent);
    ui_pnlConnect_network = lv_obj_create(ui_pnlContent);

    ui_lblIdentifier_network = lv_label_create(ui_pnlIdentifier_network);
    lv_label_set_text(ui_lblIdentifier_network, "");
    ui_lblPassword_network = lv_label_create(ui_pnlPassword_network);
    lv_label_set_text(ui_lblPassword_network, "");
    ui_lblScan_network = lv_label_create(ui_pnlScan_network);
    lv_label_set_text(ui_lblScan_network, "");
    ui_lblType_network = lv_label_create(ui_pnlType_network);
    lv_label_set_text(ui_lblType_network, "");
    ui_lblAddress_network = lv_label_create(ui_pnlAddress_network);
    lv_label_set_text(ui_lblAddress_network, "");
    ui_lblSubnet_network = lv_label_create(ui_pnlSubnet_network);
    lv_label_set_text(ui_lblSubnet_network, "");
    ui_lblGateway_network = lv_label_create(ui_pnlGateway_network);
    lv_label_set_text(ui_lblGateway_network, "");
    ui_lblDNS_network = lv_label_create(ui_pnlDNS_network);
    lv_label_set_text(ui_lblDNS_network, "");
    ui_lblConnect_network = lv_label_create(ui_pnlConnect_network);
    lv_label_set_text(ui_lblConnect_network, "");

    ui_icoIdentifier_network = lv_img_create(ui_pnlIdentifier_network);
    ui_icoPassword_network = lv_img_create(ui_pnlPassword_network);
    ui_icoScan_network = lv_img_create(ui_pnlScan_network);
    ui_icoType_network = lv_img_create(ui_pnlType_network);
    ui_icoAddress_network = lv_img_create(ui_pnlAddress_network);
    ui_icoSubnet_network = lv_img_create(ui_pnlSubnet_network);
    ui_icoGateway_network = lv_img_create(ui_pnlGateway_network);
    ui_icoDNS_network = lv_img_create(ui_pnlDNS_network);
    ui_icoConnect_network = lv_img_create(ui_pnlConnect_network);

    ui_lblIdentifier_networkValue = lv_label_create(ui_pnlIdentifier_network);
    lv_label_set_text(ui_lblIdentifier_networkValue, "");
    ui_lblPassword_networkValue = lv_label_create(ui_pnlPassword_network);
    lv_label_set_text(ui_lblPassword_networkValue, "");
    ui_lblScan_networkValue = lv_label_create(ui_pnlScan_network);
    lv_label_set_text(ui_lblScan_networkValue, "");
    ui_lblType_networkValue = lv_label_create(ui_pnlType_network);
    lv_label_set_text(ui_lblType_networkValue, "");
    ui_lblAddress_networkValue = lv_label_create(ui_pnlAddress_network);
    lv_label_set_text(ui_lblAddress_networkValue, "");
    ui_lblSubnet_networkValue = lv_label_create(ui_pnlSubnet_network);
    lv_label_set_text(ui_lblSubnet_networkValue, "");
    ui_lblGateway_networkValue = lv_label_create(ui_pnlGateway_network);
    lv_label_set_text(ui_lblGateway_networkValue, "");
    ui_lblDNS_networkValue = lv_label_create(ui_pnlDNS_network);
    lv_label_set_text(ui_lblDNS_networkValue, "");
    ui_lblConnect_networkValue = lv_label_create(ui_pnlConnect_network);
    lv_label_set_text(ui_lblConnect_networkValue, "");

    ui_pnlEntry_network = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlEntry_network, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlEntry_network, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlEntry_network, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlEntry_network, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlEntry_network, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlEntry_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlEntry_network, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlEntry_network, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlEntry_network, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlEntry_network, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlEntry_network, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlEntry_network, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlEntry_network, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlEntry_network, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlEntry_network, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlEntry_network, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlEntry_network, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_txtEntry_network = lv_textarea_create(ui_pnlEntry_network);
    lv_obj_set_width(ui_txtEntry_network, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(ui_txtEntry_network, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txtEntry_network, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txtEntry_network, 1024);
    lv_textarea_set_one_line(ui_txtEntry_network, true);
    lv_obj_set_style_radius(ui_txtEntry_network, theme->OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_txtEntry_network, lv_color_hex(theme->OSK.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_txtEntry_network, theme->OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_txtEntry_network, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
}
